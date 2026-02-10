#include "fl/remote/types.h"

namespace fl {

fl::Json RpcResult::to_json() const {
    fl::Json obj = fl::Json::object();
    obj.set("function", functionName);
    obj.set("result", result);
    obj.set("scheduledAt", static_cast<i64>(scheduledAt));
    obj.set("receivedAt", static_cast<i64>(receivedAt));
    obj.set("executedAt", static_cast<i64>(executedAt));
    obj.set("wasScheduled", wasScheduled);
    return obj;
}

} // namespace fl
