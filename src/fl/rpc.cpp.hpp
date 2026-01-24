#include "fl/rpc.h"

#if FASTLED_ENABLE_JSON

namespace fl {

// =============================================================================
// Rpc::handle() - Process JSON-RPC requests
// =============================================================================

Json Rpc::handle(const Json& request) {
    // Extract method name
    if (!request.contains("method")) {
        return detail::makeJsonRpcError(-32600, "Invalid Request: missing 'method'", request["id"]);
    }

    auto methodOpt = request["method"].as_string();
    if (!methodOpt.has_value()) {
        return detail::makeJsonRpcError(-32600, "Invalid Request: 'method' must be a string", request["id"]);
    }
    fl::string methodName = methodOpt.value();

    // Handle built-in rpc.discover if enabled
    if (mDiscoverEnabled && methodName == "rpc.discover") {
        Json response = Json::object();
        response.set("jsonrpc", "2.0");
        response.set("result", schema(mSchemaTitle.c_str(), mSchemaVersion.c_str()));
        if (request.contains("id")) {
            response.set("id", request["id"]);
        }
        return response;
    }

    // Look up the method
    auto it = mRegistry.find(methodName);
    if (it == mRegistry.end()) {
        return detail::makeJsonRpcError(-32601, "Method not found: " + methodName, request["id"]);
    }

    // Extract params (default to empty array)
    Json params = request.contains("params") ? request["params"] : Json::parse("[]");
    if (!params.is_array()) {
        return detail::makeJsonRpcError(-32602, "Invalid params: must be an array", request["id"]);
    }

    // Invoke the method
    fl::tuple<TypeConversionResult, Json> resultTuple = it->second.mInvoker->invoke(params);
    TypeConversionResult convResult = fl::get<0>(resultTuple);
    Json returnVal = fl::get<1>(resultTuple);

    // Check for conversion errors
    if (!convResult.ok()) {
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
// Rpc::methods() - Returns array of method schemas
// =============================================================================

Json Rpc::methods() const {
    Json arr = Json::array();
    for (auto it = mRegistry.begin(); it != mRegistry.end(); ++it) {
        Json methodObj = Json::object();
        methodObj.set("name", it->first.c_str());

        // Add description if present
        if (!it->second.mDescription.empty()) {
            methodObj.set("description", it->second.mDescription.c_str());
        }

        // Add tags if present (OpenRPC tags for grouping)
        if (!it->second.mTags.empty()) {
            Json tagsArr = Json::array();
            for (fl::size i = 0; i < it->second.mTags.size(); ++i) {
                Json tagObj = Json::object();
                tagObj.set("name", it->second.mTags[i].c_str());
                tagsArr.push_back(tagObj);
            }
            methodObj.set("tags", tagsArr);
        }

        methodObj.set("params", it->second.mSchemaGenerator->params());
        if (it->second.mSchemaGenerator->hasResult()) {
            methodObj.set("result", it->second.mSchemaGenerator->result());
        }
        arr.push_back(methodObj);
    }
    return arr;
}

// =============================================================================
// Rpc::schema() - Returns full OpenRPC document
// =============================================================================

Json Rpc::schema(const char* title, const char* version) const {
    Json doc = Json::object();
    doc.set("openrpc", "1.3.2");

    // Info object
    Json info = Json::object();
    info.set("title", title);
    info.set("version", version);
    doc.set("info", info);

    // Methods array
    doc.set("methods", methods());

    return doc;
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

} // namespace fl

#endif // FASTLED_ENABLE_JSON
