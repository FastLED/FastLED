#pragma once

#ifndef FASTLED_NAMESPACE
/// Start of the FastLED namespace
#define FASTLED_NAMESPACE_BEGIN
/// End of the FastLED namespace
#define FASTLED_NAMESPACE_END
/// "Using" directive for the namespace
#define FASTLED_USING_NAMESPACE
#else
#define FASTLED_NAMESPACE_BEGIN namespace FASTLED_NAMESPACE {
#define FASTLED_NAMESPACE_END }
#define FASTLED_USING_NAMESPACE using namespace FASTLED_NAMESPACE;
#endif
