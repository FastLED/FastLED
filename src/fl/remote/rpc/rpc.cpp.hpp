#include "fl/remote/rpc/rpc.h"

#if FASTLED_ENABLE_JSON

#include "fl/int.h"
#include "fl/json.h"
#include "fl/log.h"
#include "fl/remote/rpc/rpc_invokers.h"
#include "fl/remote/rpc/rpc_registry.h"
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
        FL_ERROR("RPC: Method not found: " << methodName.c_str());
        return detail::makeJsonRpcError(-32601, "Method not found: " + methodName, request["id"]);
    }

    // Extract params (default to empty array)
    Json params = request.contains("params") ? request["params"] : Json::parse("[]");
    if (!params.is_array()) {
        FL_ERROR("RPC: Invalid params - must be an array for method: " << methodName.c_str());
        return detail::makeJsonRpcError(-32602, "Invalid params: must be an array", request["id"]);
    }

    // Invoke the method
    fl::tuple<TypeConversionResult, Json> resultTuple = it->second.mInvoker->invoke(params);

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
        // Format: ["methodName", "returnType", [["param1", "type1"], ["param2", "type2"]]]
        Json methodTuple = Json::array();
        methodTuple.push_back(it->first.c_str());  // Method name
        methodTuple.push_back(it->second.mSchemaGenerator->resultTypeName());  // Return type
        methodTuple.push_back(it->second.mSchemaGenerator->params());  // Params array
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
