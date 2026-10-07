#include "fl/remote/types.h"
#include "fl/stl/noexcept.h"
namespace fl {

fl::json RpcResult::to_json() const FL_NO_EXCEPT {
    fl::json obj = fl::json::object();
    obj.set("function", functionName);
    obj.set("result", result);
    obj.set("scheduledAt", static_cast<i64>(scheduledAt));
    obj.set("receivedAt", static_cast<i64>(receivedAt));
    obj.set("executedAt", static_cast<i64>(executedAt));
    obj.set("wasScheduled", wasScheduled);
    return obj;
}

} // namespace fl
