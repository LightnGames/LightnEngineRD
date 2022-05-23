#include "GpuMaterialManager.h"
namespace ltn {
namespace {
GpuMaterialManager g_gpuMaterialManager;
}
void GpuMaterialManager::initialize() {
}
void GpuMaterialManager::terminate() {
}
void GpuMaterialManager::update() {
}
GpuMaterialManager* GpuMaterialManager::Get() {
	return &g_gpuMaterialManager;
}
}
