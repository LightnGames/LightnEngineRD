#include "Light.h"
#include <Core/Memory.h>
#include <Core/CpuTimerManager.h>

namespace ltn {
namespace {
LightScene g_lightScene;
}
void LightScene::initialize() {
	CpuScopedPerf scopedPerf("LightScene");
	{
		VirtualArray::Desc handleDesc = {};
		handleDesc._size = DIRECTIONAL_LIGHT_CAPACITY;
		_directionalLightAllocations.initialize(handleDesc);
	}
}

void LightScene::terminate() {
	_directionalLightAllocations.terminate();
}

void LightScene::lateUpdate() {
	u32 destroyCount = _directionalLightDestroyInfos.getUpdateCount();
	auto destroyShaders = _directionalLightDestroyInfos.getObjects();
	for (u32 i = 0; i < destroyCount; ++i) {
		u32 shaderIndex = getDirectionalLightIndex(destroyShaders[i]);
		_directionalLightAllocations.freeAllocation(_directionalLightAllocationInfos[shaderIndex]);
		_directionalLights[shaderIndex] = DirectionalLight();
	}

	_directionalLightUpdateInfos.reset();
	_directionalLightDestroyInfos.reset();
}

DirectionalLight* LightScene::createDirectionalLight() {
	VirtualArray::AllocationInfo allocationInfo = _directionalLightAllocations.allocation(1);
	_directionalLightAllocationInfos[allocationInfo._offset] = allocationInfo;

	DirectionalLight* light = &_directionalLights[allocationInfo._offset];
	_directionalLightUpdateInfos.push(light);

	return light;
}

void LightScene::destroyDirectionalLight(const DirectionalLight* light) {
	_directionalLightDestroyInfos.push(light);
}
LightScene* LightScene::Get() {
	return &g_lightScene;
}
}
