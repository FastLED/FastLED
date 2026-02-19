#include "fl/remote/rpc/rpc.h"

#if FASTLED_ENABLE_JSON

#include "fl/int.h"
#include "fl/json.h"
#include "fl/log.h"
#include "fl/remote/rpc/rpc_invokers.h"
#include "fl/remote/rpc/rpc_registry.h"
#include "fl/remote/rpc/response_send.h"
#include "fl/remote/rpc/type_conversion_result.h"
#include "fl/stl/optional.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/string.h"
#include "fl/stl/strstream.h"
#include "fl/stl/tuple.h"
#include "fl/stl/unordered_map.h"
#include "fl/stl/vector.h"

namespace fl {

// =============================================================================
// Rpc::setResponseSink() - Set response sink for async ACKs
// =============================================================================

void Rpc::setResponseSink(fl::function<void(const fl::Json&)> sink) {
    mResponseSink = fl::move(sink);
}

// =============================================================================
// Rpc::bindAsync() - Bind async method with ResponseSend parameter
// =============================================================================

void Rpc::bindAsync(const char* name,
                   fl::function<void(ResponseSend&, const Json&)> fn,
                   fl::RpcMode mode) {
    fl::string key(name);

    detail::RpcEntry entry;
    entry.mTypeTag = detail::TypeTag<void(const Json&)>::id();
    entry.mMode = mode;
    entry.mIsResponseAware = true;
    entry.mResponseAwareFn = fl::move(fn);

    // Create schema generator for void(Json) signature (params not decomposed)
    entry.mSchemaGenerator = fl::make_shared<detail::TypedSchemaGenerator<void(const Json&)>>();
    entry.mDescription = "";
    entry.mTags = {};

    // Create a placeholder invoker (actual invocation handled in handle())
    struct PlaceholderInvoker : public detail::ErasedInvoker {
        fl::tuple<TypeConversionResult, Json> invoke(const Json&) override {
            // Should not be called - handle() will call mResponseAwareFn directly
            return fl::make_tuple(TypeConversionResult::success(), Json(nullptr));
        }
    };
    entry.mInvoker = fl::make_shared<PlaceholderInvoker>();

    mRegistry[key] = fl::move(entry);
}

// =============================================================================
// Rpc::handle() - Process JSON-RPC requests
// =============================================================================

Json Rpc::handle(const Json& request) {
    // Extract method name
    if (!request.contains("method")) {
        FL_ERROR("RPC: Invalid Request - missing 'method' field");
        return detail::makeJsonRpcError(-32600, "Invalid Request: missing 'method'", request["id"]);
    }

    auto methodOpt = request["method"].as_string();
    if (!methodOpt.has_value()) {
        FL_ERROR("RPC: Invalid Request - 'method' must be a string");
        return detail::makeJsonRpcError(-32600, "Invalid Request: 'method' must be a string", request["id"]);
    }
    fl::string methodName = methodOpt.value();

    // Handle built-in rpc.discover method
    if (methodName == "rpc.discover") {
        Json response = Json::object();
        response.set("jsonrpc", "2.0");
        response.set("result", schema());
        if (request.contains("id")) {
            response.set("id", request["id"]);
        }
        return response;
    }

    // Look up the method
    auto it = mRegistry.find(methodName);
    if (it == mRegistry.end()) {
        FL_WARN("RPC: Method not found: " << methodName.c_str());
        return detail::makeJsonRpcError(-32601, "Method not found: " + methodName, request["id"]);
    }

    // Extract params (default to empty array)
    Json params = request.contains("params") ? request["params"] : Json::parse("[]");
    if (!params.is_array()) {
        FL_ERROR("RPC: Invalid params - must be an array for method: " << methodName.c_str());
        return detail::makeJsonRpcError(-32602, "Invalid params: must be an array", request["id"]);
    }

    // Check if this is an async function
    const detail::RpcEntry& entry = it->second;
    bool isAsync = (entry.mMode == RpcMode::ASYNC || entry.mMode == RpcMode::ASYNC_STREAM);

    // Check if this is a response-aware function (uses ResponseSend&)
    bool isResponseAware = entry.mIsResponseAware;

    // For async functions, send ACK immediately
    if (isAsync && mResponseSink && request.contains("id")) {
        Json ack = Json::object();
        ack.set("jsonrpc", "2.0");
        ack.set("id", request["id"]);

        Json ackResult = Json::object();
        ackResult.set("acknowledged", true);
        ack.set("result", ackResult);

        mResponseSink(ack);
        FL_DBG("RPC: Sent ACK for async method: " << methodName.c_str());
    }

    fl::tuple<TypeConversionResult, Json> resultTuple;

    // Handle response-aware methods (with ResponseSend& parameter)
    if (isResponseAware) {
        // Create ResponseSend instance
        fl::Json requestId = request.contains("id") ? request["id"] : Json(nullptr);
        ResponseSend responseSend(requestId, mResponseSink);

        // Invoke user function with ResponseSend& and raw JSON params
        entry.mResponseAwareFn(responseSend, params);

        // Return success with null result (actual responses sent via ResponseSend)
        resultTuple = fl::make_tuple(TypeConversionResult::success(), Json(nullptr));
    } else {
        // Regular invocation
        resultTuple = entry.mInvoker->invoke(params);
    }

    TypeConversionResult convResult = fl::get<0>(resultTuple);
    Json returnVal = fl::get<1>(resultTuple);

    // Check for conversion errors
    if (!convResult.ok()) {
        FL_ERROR("RPC: Invalid params for method '" << methodName.c_str() << "': " << convResult.errorMessage().c_str());
        return detail::makeJsonRpcError(-32602, "Invalid params: " + convResult.errorMessage(), request["id"]);
    }

    // Build success response
    Json response = Json::object();
    response.set("jsonrpc", "2.0");
    response.set("result", returnVal);

    // Include id if present (for request/response correlation)
    if (request.contains("id")) {
        response.set("id", request["id"]);
    }

    // Include warnings if any
    if (convResult.hasWarning()) {
        Json warnings = Json::array();
        for (fl::size i = 0; i < convResult.warnings().size(); ++i) {
            warnings.push_back(Json(convResult.warnings()[i]));
        }
        response.set("warnings", warnings);
    }

    // For async functions, mark response to not queue it (ACK already sent)
    if (isAsync) {
        response.set("__async", true);  // Internal marker
    }

    return response;
}

// =============================================================================
// Rpc::handle_maybe() - Process notifications (no id returns nullopt)
// =============================================================================

fl::optional<Json> Rpc::handle_maybe(const Json& request) {
    // If no id, this is a notification - process but don't return response
    if (!request.contains("id")) {
        // Still need to execute the method
        if (request.contains("method")) {
            auto methodOpt = request["method"].as_string();
            if (methodOpt.has_value()) {
                fl::string methodName = methodOpt.value();
                auto it = mRegistry.find(methodName);
                if (it != mRegistry.end()) {
                    Json params = request.contains("params") ? request["params"] : Json::parse("[]");
                    if (params.is_array()) {
                        it->second.mInvoker->invoke(params);
                    }
                }
            }
        }
        return fl::nullopt;
    }

    return handle(request);
}

// =============================================================================
// Rpc::tags() - Returns list of unique tags
// =============================================================================

fl::vector<fl::string> Rpc::tags() const {
    fl::vector<fl::string> result;
    for (auto it = mRegistry.begin(); it != mRegistry.end(); ++it) {
        for (fl::size i = 0; i < it->second.mTags.size(); ++i) {
            bool found = false;
            for (fl::size j = 0; j < result.size(); ++j) {
                if (result[j] == it->second.mTags[i]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                result.push_back(it->second.mTags[i]);
            }
        }
    }
    return result;
}

// =============================================================================
// Rpc::methods() - Returns flat method array
// =============================================================================

Json Rpc::methods() const {
    Json arr = Json::array();
    for (auto it = mRegistry.begin(); it != mRegistry.end(); ++it) {
        // Format: ["methodName", "returnType", [["param1", "type1"], ["param2", "type2"]], "mode"]
        Json methodTuple = Json::array();
        methodTuple.push_back(it->first.c_str());  // Method name
        methodTuple.push_back(it->second.mSchemaGenerator->resultTypeName());  // Return type
        methodTuple.push_back(it->second.mSchemaGenerator->params());  // Params array

        // Add mode (sync or async)
        const char* modeStr = (it->second.mMode == RpcMode::ASYNC) ? "async" : "sync";
        methodTuple.push_back(modeStr);

        arr.push_back(methodTuple);
    }
    return arr;
}

// =============================================================================
// Rpc::schema() - Returns flat schema
// =============================================================================

Json Rpc::schema() const {
    Json doc = Json::object();
    doc.set("schema", methods());
    return doc;
}

} // namespace fl

#endif // FASTLED_ENABLE_JSON
