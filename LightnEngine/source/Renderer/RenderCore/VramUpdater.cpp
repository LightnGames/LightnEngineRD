#include "VramUpdater.h"

namespace ltn {
namespace {
VramUpdater g_vramUpdater;
}
void VramUpdater::initialize() {
}
void VramUpdater::terminate() {
}
VramUpdater* VramUpdater::Get() {
	return &g_vramUpdater;
}
}
