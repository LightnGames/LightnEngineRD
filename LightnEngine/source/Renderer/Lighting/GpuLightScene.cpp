#include "GpuLight.h"
#include <RendererScene/Light.h>
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/VramUpdater.h>
#include <Renderer/RenderCore/GlobalVideoMemoryAllocator.h>
#include <Core/CpuTimerManager.h>

namespace ltn {
namespace {
GpuLightScene g_gpuLightScene;
}
void GpuLightScene::initialize() {
	CpuScopedPerf scopedPerf("GpuLightScene");
	rhi::Device* device = DeviceManager::Get()->getDevice();

	{

		GpuBufferDesc desc = {};
		desc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
		desc._device = device;
		desc._sizeInByte = LightScene::DIRECTIONAL_LIGHT_CAPACITY * sizeof(gpu::DirectionalLight);
		desc._initialState = rhi::RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		_directionalLightGpuBuffer.initialize(desc);
		_directionalLightGpuBuffer.setName("DirectionalLight");
	}

	{
		DescriptorAllocatorGroup* descriptorAllocator = DescriptorAllocatorGroup::Get();
		_directionalLightSrv = descriptorAllocator->allocateSrvCbvUavGpu();

		rhi::ShaderResourceViewDesc desc = {};
		desc._format = rhi::FORMAT_R32_TYPELESS;
		desc._viewDimension = rhi::SRV_DIMENSION_BUFFER;
		desc._buffer._flags = rhi::BUFFER_SRV_FLAG_RAW;
		desc._buffer._numElements = _directionalLightGpuBuffer.getU32ElementCount();
		device->createShaderResourceView(_directionalLightGpuBuffer.getResource(), &desc, _directionalLightSrv._cpuHandle);
	}
}

void GpuLightScene::terminate() {
	_directionalLightGpuBuffer.terminate();
	DescriptorAllocatorGroup* descriptorAllocator = DescriptorAllocatorGroup::Get();
	descriptorAllocator->deallocSrvCbvUavGpu(_directionalLightSrv);
}

void GpuLightScene::update() {
	VramUpdater* vramUpdater = VramUpdater::Get();
	LightScene* lightScene = LightScene::Get();

	// çXêV
	const UpdateInfos<DirectionalLight>* updateInfos = lightScene->getUpdateInfos();
	u32 updateCount = updateInfos->getUpdateCount();
	auto updateDirectionalLights = updateInfos->getObjects();
	for (u32 i = 0; i < updateCount; ++i) {
		const DirectionalLight* light = updateDirectionalLights[i];
		u32 lightIndex = lightScene->getDirectionalLightIndex(light);

		gpu::DirectionalLight* gpuLight = vramUpdater->enqueueUpdate<gpu::DirectionalLight>(&_directionalLightGpuBuffer, sizeof(gpu::DirectionalLight) * lightIndex);
		gpuLight->_intensity = light->getIntensity();
		gpuLight->_color = light->getColor().getColor3();
		gpuLight->_direction = light->getDirection().getFloat3();
	}

	// îjä¸
	const UpdateInfos<DirectionalLight>* destroyInfos = lightScene->getDestroyInfos();
	u32 destroyCount = destroyInfos->getUpdateCount();
	auto destroyDirectionalLights = destroyInfos->getObjects();
	for (u32 i = 0; i < destroyCount; ++i) {
		const DirectionalLight* light = destroyDirectionalLights[i];
		u32 lightIndex = lightScene->getDirectionalLightIndex(light);

		gpu::DirectionalLight* gpuLight = vramUpdater->enqueueUpdate<gpu::DirectionalLight>(&_directionalLightGpuBuffer, sizeof(gpu::DirectionalLight) * lightIndex);
		memset(gpuLight, 0, sizeof(gpu::DirectionalLight));
	}
}

GpuLightScene* GpuLightScene::Get() {
	return &g_gpuLightScene;
}
}
