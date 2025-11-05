#ifdef ESP32

#include "fl/fastled.h"

FL_EXTERN_C_BEGIN
#include "soc/rmt_struct.h"
FL_EXTERN_C_END

void test() {
    RMT.conf_ch[0].conf1.mem_rd_rst = 1;
}

#endif
