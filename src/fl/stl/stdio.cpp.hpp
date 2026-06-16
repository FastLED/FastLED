#include "fl/stl/stdio.h"

namespace fl { namespace printf_detail {

void format_arg(sstream& stream, const FormatSpec& spec, const fl::string& arg) FL_NOEXCEPT {
    format_arg(stream, spec, arg.c_str());
}

void format_arg(sstream& stream, const FormatSpec& spec, const fl::string_view& arg) FL_NOEXCEPT {
    format_arg(stream, spec, fl::string(arg).c_str());
}

} } // namespace fl::printf_detail
