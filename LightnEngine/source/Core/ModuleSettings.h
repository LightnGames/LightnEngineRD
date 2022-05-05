#pragma once

namespace ltn {
#ifdef LTN_EXPORT
#define LTN_API __declspec(dllexport)
#else
#define LTN_API __declspec(dllimport)
#endif
}