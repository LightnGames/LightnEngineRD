#include "ComputeLod.h"

#include <Core/Utility.h>
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/AssetReloader/PipelineStateReloader.h>
#include <Renderer/MeshRenderer/GeometryResourceManager.h>
#include <Renderer/MeshRenderer/GpuMeshResourceManager.h>
#include <Renderer/MeshRenderer/GpuMeshInstanceManager.h>
#include <Renderer/MeshRenderer/GpuMaterialManager.h>
#include <Renderer/MeshRenderer/GpuTextureManager.h>
#include <Renderer/RenderCore/RendererUtility.h>
#include <Renderer/RenderCore/GlobalVideoMemoryAllocator.h>
#include <Renderer/RenderCore/VramUpdater.h>
#include <Renderer/RenderCore/LodStreamingManager.h>

namespace ltn {
void ComputeLod::initialize() {
	CpuScopedPerf scopedPerf("ComputeLod");
	rhi::Device* device = DeviceManager::Get()->getDevice();

	// LOD 計算
	{
		rhi::DescriptorRange cullingInfoCbvRange(rhi::DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		rhi::DescriptorRange viewInfoCbvRange(rhi::DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
		rhi::DescriptorRange meshSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 4, 0);
		rhi::DescriptorRange meshInstanceSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 3, 4);
		rhi::DescriptorRange lodLevelResultUavRange(rhi::DESCRIPTOR_RANGE_TYPE_UAV, 2, 0);
		rhi::DescriptorRange meshLodLevelUavRange(rhi::DESCRIPTOR_RANGE_TYPE_UAV, 2, 2);
		rhi::DescriptorRange materialLodLevelUavRange(rhi::DESCRIPTOR_RANGE_TYPE_UAV, 1, 4);

		rhi::RootParameter rootParameters[ComputeLodLevelRootParam::COUNT] = {};
		rootParameters[ComputeLodLevelRootParam::CULLING_INFO].initializeDescriptorTable(1, &cullingInfoCbvRange, rhi::SHADER_VISIBILITY_ALL);
		rootParameters[ComputeLodLevelRootParam::VIEW_INFO].initializeDescriptorTable(1, &viewInfoCbvRange, rhi::SHADER_VISIBILITY_ALL);
		rootParameters[ComputeLodLevelRootParam::MESH].initializeDescriptorTable(1, &meshSrvRange, rhi::SHADER_VISIBILITY_ALL);
		rootParameters[ComputeLodLevelRootParam::MESH_INSTANCE].initializeDescriptorTable(1, &meshInstanceSrvRange, rhi::SHADER_VISIBILITY_ALL);
		rootParameters[ComputeLodLevelRootParam::MESH_INSTANCE_LOD_LEVEL].initializeDescriptorTable(1, &lodLevelResultUavRange, rhi::SHADER_VISIBILITY_ALL);
		rootParameters[ComputeLodLevelRootParam::MESH_LOD_LEVEL].initializeDescriptorTable(1, &meshLodLevelUavRange, rhi::SHADER_VISIBILITY_ALL);
		rootParameters[ComputeLodLevelRootParam::MATERIAL_LOD_LEVEL].initializeDescriptorTable(1, &materialLodLevelUavRange, rhi::SHADER_VISIBILITY_ALL);

		rhi::RootSignatureDesc rootSignatureDesc = {};
		rootSignatureDesc._device = device;
		rootSignatureDesc._numParameters = LTN_COUNTOF(rootParameters);
		rootSignatureDesc._parameters = rootParameters;
		_computeLodRootSignature.iniaitlize(rootSignatureDesc);

		AssetPath shaderPath("EngineComponent\\Shader\\MeshRenderer\\ComputeLod.cso");
		rhi::ShaderBlob shader;
		shader.initialize(shaderPath.get());

		rhi::ComputePipelineStateDesc pipelineStateDesc = {};
		pipelineStateDesc._device = device;
		pipelineStateDesc._rootSignature = &_computeLodRootSignature;
		pipelineStateDesc._cs = shader.getShaderByteCode();
		_computeLodPipelineState.iniaitlize(pipelineStateDesc);

		//PipelineStateReloader::ComputePipelineStateRegisterDesc reloaderDesc = {};
		//reloaderDesc._desc = pipelineStateDesc;
		//reloaderDesc._shaderPathHash = shaderPath.get();
		//PipelineStateReloader::Get()->registerPipelineState(&_gpuCullingPipelineState, reloaderDesc);

		shader.terminate();
	}
}
void ComputeLod::terminate() {
	_computeLodRootSignature.terminate();
	_computeLodPipelineState.terminate();
}

void ComputeLod::computeLod(const ComputeLodDesc& desc) {
	rhi::CommandList* commandList = desc._commandList;
	DEBUG_MARKER_CPU_GPU_SCOPED_TIMER(commandList, Color4(), "ComputeLod");

	// 前フレームの LOD 情報をリードバックバッファにコピー
	LodStreamingManager* lodStreamingManager = LodStreamingManager::Get();
	lodStreamingManager->copyReadbackBuffers(commandList);

	// リソースバリア
	ScopedBarrierDesc barriers[] = {
		ScopedBarrierDesc(lodStreamingManager->getMeshMinLodLevelGpuBuffer(), rhi::RESOURCE_STATE_UNORDERED_ACCESS),
		ScopedBarrierDesc(lodStreamingManager->getMeshMaxLodLevelGpuBuffer(), rhi::RESOURCE_STATE_UNORDERED_ACCESS),
		ScopedBarrierDesc(lodStreamingManager->getMeshInstanceLodLevelGpuBuffer(), rhi::RESOURCE_STATE_UNORDERED_ACCESS),
		ScopedBarrierDesc(lodStreamingManager->getMeshInstanceScreenPersentageGpuBuffer(), rhi::RESOURCE_STATE_UNORDERED_ACCESS),
		ScopedBarrierDesc(lodStreamingManager->getMaterialScreenPesentageGpuBuffer(), rhi::RESOURCE_STATE_UNORDERED_ACCESS),
	};
	ScopedBarrier scopedBarriers(commandList, barriers, LTN_COUNTOF(barriers));

	// Mesh Request Lod バッファをクリア
	lodStreamingManager->clearBuffers(commandList);

	rhi::GpuDescriptorHandle meshSrv = GpuMeshResourceManager::Get()->getMeshGpuSrv();
	rhi::GpuDescriptorHandle meshInstanceSrv = GpuMeshInstanceManager::Get()->getMeshInstanceGpuSrv();
	rhi::GpuDescriptorHandle indirectArgumentOffsetSrv = GpuMeshInstanceManager::Get()->getSubMeshDrawOffsetsGpuSrv();
	rhi::GpuDescriptorHandle meshLodLevelUav = lodStreamingManager->getMeshLodLevelGpuUav();
	rhi::GpuDescriptorHandle materialLodLevelUav = lodStreamingManager->getMaterialScreenPersentageGpuUav();
	rhi::GpuDescriptorHandle meshInstanceLodLevelUav = lodStreamingManager->getMeshInstanceLodLevelGpuUav();

	commandList->setComputeRootSignature(&_computeLodRootSignature);
	commandList->setPipelineState(&_computeLodPipelineState);

	commandList->setComputeRootDescriptorTable(ComputeLodLevelRootParam::CULLING_INFO, desc._cullingInfoCbv);
	commandList->setComputeRootDescriptorTable(ComputeLodLevelRootParam::VIEW_INFO, desc._viewCbv);
	commandList->setComputeRootDescriptorTable(ComputeLodLevelRootParam::MESH, meshSrv);
	commandList->setComputeRootDescriptorTable(ComputeLodLevelRootParam::MESH_INSTANCE, meshInstanceSrv);
	commandList->setComputeRootDescriptorTable(ComputeLodLevelRootParam::MESH_LOD_LEVEL, meshLodLevelUav);
	commandList->setComputeRootDescriptorTable(ComputeLodLevelRootParam::MESH_INSTANCE_LOD_LEVEL, meshInstanceLodLevelUav);
	commandList->setComputeRootDescriptorTable(ComputeLodLevelRootParam::MATERIAL_LOD_LEVEL, materialLodLevelUav);

	u32 meshInstanceReserveCount = GpuMeshInstanceManager::Get()->getMeshInstanceReserveCount();
	u32 dispatchCount = RoundDivUp(meshInstanceReserveCount, 128u);
	commandList->dispatch(dispatchCount, 1, 1);
}

namespace {
ComputeLod g_computeLod;
}
ComputeLod* ComputeLod::Get() {
	return &g_computeLod;
}
}
