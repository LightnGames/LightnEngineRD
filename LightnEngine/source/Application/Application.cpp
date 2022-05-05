#include "Application.h"

namespace ltn {
namespace {
ApplicationSysytem g_applicationSystem;
}

ApplicationSysytem* ApplicationSysytem::Get() {
	return &g_applicationSystem;
}
}