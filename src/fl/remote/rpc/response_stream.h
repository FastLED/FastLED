#pragma once

#include "fl/stl/function.h"
#include "fl/stl/json.h"
#include "fl/stl/json_stream_writer.h"
#include "fl/stl/noexcept.h"

namespace fl {

using JsonStreamCallback = fl::function<void(fl::JsonStreamWriter&)>;
using ResponseStreamSink = fl::function<void(fl::JsonStreamCallback)>;
using StreamingRpcHandler =
    fl::function<void(fl::JsonStreamWriter&, const fl::json&, const fl::json&)>;

inline void streamJsonRpcId(fl::JsonStreamWriter& writer,
                            const fl::json& id) FL_NO_EXCEPT {
    if (!id.has_value() || id.is_null()) {
        writer.valueNull();
        return;
    }
    if (id.is_string()) {
        fl::string s = id.as_string().value_or("");
        writer.value(s.c_str());
        return;
    }
    if (id.is_bool()) {
        writer.value(id.as_bool().value_or(false));
        return;
    }
    if (id.is_int()) {
        writer.value(id.as_int().value_or(0));
        return;
    }
    if (id.is_number()) {
        writer.value(static_cast<fl::i64>(id.as_float().value_or(0.0f)));
        return;
    }
    writer.valueNull();
}

inline void streamJsonRpcResultEnvelope(
    fl::JsonStreamWriter& writer,
    const fl::json& requestId,
    bool includeId,
    const fl::JsonStreamCallback& writeResult) FL_NO_EXCEPT {
    writer.beginObject();
    writer.member("jsonrpc", "2.0");
    writer.key("result");
    if (writeResult) {
        writeResult(writer);
    } else {
        writer.valueNull();
    }
    if (includeId) {
        writer.key("id");
        streamJsonRpcId(writer, requestId);
    }
    writer.endObject();
}

} // namespace fl
