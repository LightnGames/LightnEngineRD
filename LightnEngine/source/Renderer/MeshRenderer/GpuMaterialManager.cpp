#include "GpuMaterialManager.h"
#include <Core/Memory.h>
#include <Core/CpuTimerManager.h>
#include <RendererScene/Material.h>
#include <RendererScene/Shader.h>
#include <RendererScene/Texture.h>
#include <Renderer/RenderCore/DescriptorAllocator.h>
#include <Renderer/RenderCore/VramUpdater.h>
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/GpuShader.h>
#include <Renderer/RenderCore/GlobalVideoMemoryAllocator.h>
#include <Renderer/AssetReloader/PipelineStateReloader.h>

namespace ltn {
namespace {
GpuMaterialManager g_gpuMaterialManager;

rhi::InputElementDesc inputElements[] = {
	{"POSITION", 0, rhi::FORMAT_R32G32B32_FLOAT, 0, 0, rhi::INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	{"NORMAL_TANGENT", 0, rhi::FORMAT_R32_UINT, 1, 0, rhi::INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	{"TEXCOORD", 0, rhi::FORMAT_R32_UINT, 2, 0, rhi::INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	{"INSTANCE_INDEX", 0, rhi::FORMAT_R32_UINT, 3, 0, rhi::INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
};
}

void GpuMaterialManager::initialize() {
	CpuScopedPerf scopedPerf("GpuMaterialManager");
	rhi::Device* device = DeviceManager::Get()->getDevice();
	{
		GpuBufferDesc desc = {};
		desc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
		desc._device = device;
		desc._sizeInByte = MaterialScene::MATERIAL_PARAMETER_SIZE_IN_BYTE;
		desc._initialState = rhi::RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		_parameterGpuBuffer.initialize(desc);
		_parameterGpuBuffer.setName("MaterialParameters");

		desc._sizeInByte = MaterialScene::MATERIAL_CAPACITY * sizeof(u32);
		_parameterOffsetGpuBuffer.initialize(desc);
		_parameterOffsetGpuBuffer.setName("MaterialParameterOffsets");
	}

	{
		_parameterSrv = DescriptorAllocatorGroup::Get()->allocateSrvCbvUavGpu();
		_parameterOffsetSrv = DescriptorAllocatorGroup::Get()->allocateSrvCbvUavGpu();

		rhi::ShaderResourceViewDesc desc = {};
		desc._format = rhi::FORMAT_R32_TYPELESS;
		desc._viewDimension = rhi::SRV_DIMENSION_BUFFER;
		desc._buffer._flags = rhi::BUFFER_SRV_FLAG_RAW;
		desc._buffer._numElements = _parameterGpuBuffer.getU32ElementCount();
		device->createShaderResourceView(_parameterGpuBuffer.getResource(), &desc, _parameterSrv._cpuHandle);

		desc._buffer._numElements = _parameterOffsetGpuBuffer.getU32ElementCount();
		device->createShaderResourceView(_parameterOffsetGpuBuffer.getResource(), &desc, _parameterOffsetSrv._cpuHandle);
	}

	_chunkAllocator.allocate([this](ChunkAllocator::Allocation& allocation) {
		_geometryPassRootSignatures = allocation.allocateClearedObjects<rhi::RootSignature>(PipelineSetScene::PIPELINE_SET_CAPACITY);
		_shadingPassRootSignatures = allocation.allocateClearedObjects<rhi::RootSignature>(PipelineSetScene::PIPELINE_SET_CAPACITY);
		_geometryPassPipelineStates = allocation.allocateClearedObjects<rhi::PipelineState>(PipelineSetScene::PIPELINE_SET_CAPACITY);
		_shadingPassPipelineStates = allocation.allocateClearedObjects<rhi::PipelineState>(PipelineSetScene::PIPELINE_SET_CAPACITY);
		_debugVisualizePipelineStates = allocation.allocateClearedObjects<rhi::PipelineState>(PipelineSetScene::PIPELINE_SET_CAPACITY);
		});
}

void GpuMaterialManager::terminate() {
	_parameterGpuBuffer.terminate();
	_parameterOffsetGpuBuffer.terminate();
	_chunkAllocator.free();
	DescriptorAllocatorGroup::Get()->freeSrvCbvUavGpu(_parameterSrv);
	DescriptorAllocatorGroup::Get()->freeSrvCbvUavGpu(_parameterOffsetSrv);
}

void GpuMaterialManager::update() {
	VramUpdater* vramUpdater = VramUpdater::Get();
	PipelineSetScene* pipelineSetScene = PipelineSetScene::Get();
	ShaderScene* shaderScene = ShaderScene::Get();
	GpuShaderScene* gpuShaderScene = GpuShaderScene::Get();

	// 新規作成
	const UpdateInfos<PipelineSet>* createInfos = pipelineSetScene->getCreateInfos();
	u32 createCount = createInfos->getUpdateCount();
	auto createPipelineSets = createInfos->getObjects();
	for (u32 i = 0; i < createCount; ++i) {
		const PipelineSet* pipelineSet = createPipelineSets[i];
		u32 pipelineSetIndex = pipelineSetScene->getPipelineSetIndex(pipelineSet);
		rhi::Device* device = DeviceManager::Get()->getDevice();

		// ジオメトリ ルートシグネチャ
		{
			rhi::DescriptorRange viewInfoCbvRange(rhi::DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
			rhi::DescriptorRange meshInstanceSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
			rhi::DescriptorRange indirectArgumentSubInfoSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

			rhi::RootParameter rootParameters[GeometryRootParam::COUNT] = {};
			rootParameters[GeometryRootParam::VIEW_INFO].initializeDescriptorTable(1, &viewInfoCbvRange, rhi::SHADER_VISIBILITY_VERTEX);
			rootParameters[GeometryRootParam::MESH_INSTANCE].initializeDescriptorTable(1, &meshInstanceSrvRange, rhi::SHADER_VISIBILITY_VERTEX);
			rootParameters[GeometryRootParam::INDIRECT_ARGUMENT_SUB_INFO].initializeDescriptorTable(1, &indirectArgumentSubInfoSrvRange, rhi::SHADER_VISIBILITY_ALL);

			rhi::RootSignatureDesc rootSignatureDesc = {};
			rootSignatureDesc._device = device;
			rootSignatureDesc._numParameters = LTN_COUNTOF(rootParameters);
			rootSignatureDesc._parameters = rootParameters;
			rootSignatureDesc._flags = rhi::ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			rhi::RootSignature& rootSignature = _geometryPassRootSignatures[pipelineSetIndex];
			rootSignature.iniaitlize(rootSignatureDesc);
			rootSignature.setName("RootSigScreenTriangle");
		}

		// シェーディング ルートシグネチャ
		{
			rhi::DescriptorRange shadingInfoCbvRange(rhi::DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
			rhi::DescriptorRange pipelineSetRangeCbvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);

			rhi::DescriptorRange viewInfoCbvRange(rhi::DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
			rhi::DescriptorRange materialParameterSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
			rhi::DescriptorRange materialParameterOffsetSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
			rhi::DescriptorRange meshSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 2, 2);
			rhi::DescriptorRange meshInstanceSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);
			rhi::DescriptorRange meshInstanceLodLevelSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 5);
			rhi::DescriptorRange meshLodStreamedLevelSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 6);
			rhi::DescriptorRange geometryGlobalOffsetSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 7);
			rhi::DescriptorRange vertexResourceSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 4, 8);
			rhi::DescriptorRange triangleAttibuteSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 12);
			rhi::DescriptorRange baryCentricsSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 13);
			rhi::DescriptorRange viewDepthSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 14);
			rhi::DescriptorRange meshInstanceScreenPersentageSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 15);
			rhi::DescriptorRange materialScreenPersentageSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 16);
			rhi::DescriptorRange textureSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, TextureScene::TEXTURE_CAPACITY, 0, 1);

			rhi::RootParameter rootParameters[ShadingRootParam::COUNT] = {};
			rootParameters[ShadingRootParam::SHADING_INFO].initializeDescriptorTable(1, &shadingInfoCbvRange, rhi::SHADER_VISIBILITY_VERTEX);
			rootParameters[ShadingRootParam::PIPELINE_SET_RANGE].initializeDescriptorTable(1, &pipelineSetRangeCbvRange, rhi::SHADER_VISIBILITY_VERTEX);
			rootParameters[ShadingRootParam::PIPELINE_SET_INFO].initializeConstant(0, 1, rhi::SHADER_VISIBILITY_VERTEX);

			rootParameters[ShadingRootParam::VIEW_INFO].initializeDescriptorTable(1, &viewInfoCbvRange, rhi::SHADER_VISIBILITY_PIXEL);
			rootParameters[ShadingRootParam::MATERIAL_PARAMETER].initializeDescriptorTable(1, &materialParameterSrvRange, rhi::SHADER_VISIBILITY_PIXEL);
			rootParameters[ShadingRootParam::MATERIAL_PARAMETER_OFFSET].initializeDescriptorTable(1, &materialParameterOffsetSrvRange, rhi::SHADER_VISIBILITY_PIXEL);
			rootParameters[ShadingRootParam::MESH].initializeDescriptorTable(1, &meshSrvRange, rhi::SHADER_VISIBILITY_PIXEL);
			rootParameters[ShadingRootParam::MESH_INSTANCE].initializeDescriptorTable(1, &meshInstanceSrvRange, rhi::SHADER_VISIBILITY_PIXEL);
			rootParameters[ShadingRootParam::MESH_INSTANCE_LOD_LEVEL].initializeDescriptorTable(1, &meshInstanceLodLevelSrvRange, rhi::SHADER_VISIBILITY_PIXEL);
			rootParameters[ShadingRootParam::MESH_INSTANCE_SCREEN_PERSENTAGE].initializeDescriptorTable(1, &meshInstanceScreenPersentageSrvRange, rhi::SHADER_VISIBILITY_PIXEL);
			rootParameters[ShadingRootParam::MESH_LOD_STREAMED_LEVEL].initializeDescriptorTable(1, &meshLodStreamedLevelSrvRange, rhi::SHADER_VISIBILITY_PIXEL);
			rootParameters[ShadingRootParam::MATERIAL_SCREEN_PERSENTAGE].initializeDescriptorTable(1, &materialScreenPersentageSrvRange, rhi::SHADER_VISIBILITY_PIXEL);
			rootParameters[ShadingRootParam::GEOMETRY_GLOBAL_OFFSET].initializeDescriptorTable(1, &geometryGlobalOffsetSrvRange, rhi::SHADER_VISIBILITY_PIXEL);
			rootParameters[ShadingRootParam::VERTEX_RESOURCE].initializeDescriptorTable(1, &vertexResourceSrvRange, rhi::SHADER_VISIBILITY_PIXEL);
			rootParameters[ShadingRootParam::TRIANGLE_ATTRIBUTE].initializeDescriptorTable(1, &triangleAttibuteSrvRange, rhi::SHADER_VISIBILITY_PIXEL);
			rootParameters[ShadingRootParam::BARY_CENTRICS].initializeDescriptorTable(1, &baryCentricsSrvRange, rhi::SHADER_VISIBILITY_PIXEL);
			rootParameters[ShadingRootParam::VIEW_DEPTH].initializeDescriptorTable(1, &viewDepthSrvRange, rhi::SHADER_VISIBILITY_PIXEL);
			rootParameters[ShadingRootParam::TEXTURE].initializeDescriptorTable(1, &textureSrvRange, rhi::SHADER_VISIBILITY_PIXEL);
			rootParameters[ShadingRootParam::DEBUG_TYPE].initializeConstant(1, 1, rhi::SHADER_VISIBILITY_PIXEL);

			rhi::StaticSamplerDesc staticSamplerDescs[2];
			{
				rhi::StaticSamplerDesc& samplerDesc = staticSamplerDescs[0];
				samplerDesc.Filter = rhi::FILTER_ANISOTROPIC;
				samplerDesc.AddressU = rhi::TEXTURE_ADDRESS_MODE_WRAP;
				samplerDesc.AddressV = rhi::TEXTURE_ADDRESS_MODE_WRAP;
				samplerDesc.AddressW = rhi::TEXTURE_ADDRESS_MODE_WRAP;
				samplerDesc.MipLODBias = 0;
				samplerDesc.MaxAnisotropy = 16;
				samplerDesc.ComparisonFunc = rhi::COMPARISON_FUNC_NEVER;
				samplerDesc.BorderColor = rhi::STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
				samplerDesc.MinLOD = 0.0f;
				samplerDesc.MaxLOD = FLT_MAX;
				samplerDesc.ShaderRegister = 0;
				samplerDesc.RegisterSpace = 0;
				samplerDesc.ShaderVisibility = rhi::SHADER_VISIBILITY_PIXEL;
			}
			{
				rhi::StaticSamplerDesc& samplerDesc = staticSamplerDescs[1];
				samplerDesc.Filter = rhi::FILTER_MIN_MAG_MIP_LINEAR;
				samplerDesc.AddressU = rhi::TEXTURE_ADDRESS_MODE_CLAMP;
				samplerDesc.AddressV = rhi::TEXTURE_ADDRESS_MODE_CLAMP;
				samplerDesc.AddressW = rhi::TEXTURE_ADDRESS_MODE_CLAMP;
				samplerDesc.MipLODBias = 0;
				samplerDesc.MaxAnisotropy = 0;
				samplerDesc.ComparisonFunc = rhi::COMPARISON_FUNC_NEVER;
				samplerDesc.BorderColor = rhi::STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
				samplerDesc.MinLOD = 0.0f;
				samplerDesc.MaxLOD = FLT_MAX;
				samplerDesc.ShaderRegister = 1;
				samplerDesc.RegisterSpace = 0;
				samplerDesc.ShaderVisibility = rhi::SHADER_VISIBILITY_ALL;
			}

			rhi::RootSignatureDesc rootSignatureDesc = {};
			rootSignatureDesc._device = device;
			rootSignatureDesc._numParameters = LTN_COUNTOF(rootParameters);
			rootSignatureDesc._parameters = rootParameters;
			rootSignatureDesc._numStaticSamplers = LTN_COUNTOF(staticSamplerDescs);
			rootSignatureDesc._staticSamplers = staticSamplerDescs;

			rhi::RootSignature& rootSignature = _shadingPassRootSignatures[pipelineSetIndex];
			rootSignature.iniaitlize(rootSignatureDesc);
			rootSignature.setName("RootSigScreenTriangle");
		}

		// ジオメトリ パイプラインステート
		{
			AssetPath pixelShaderPath("EngineComponent\\Shader\\VisibilityBuffer\\GeometryPass.pso");
			rhi::ShaderBlob pixelShader;
			pixelShader.initialize(pixelShaderPath.get());

			const Shader* vertexShader = pipelineSet->getVertexShader();
			u32 vertexShaderIndex = shaderScene->getShaderIndex(vertexShader);

			rhi::GraphicsPipelineStateDesc pipelineStateDesc = {};
			pipelineStateDesc._device = device;
			pipelineStateDesc._vs = gpuShaderScene->getShader(vertexShaderIndex)->getShaderByteCode();
			pipelineStateDesc._ps = pixelShader.getShaderByteCode();
			pipelineStateDesc._topologyType = rhi::PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			pipelineStateDesc._rootSignature = &_geometryPassRootSignatures[pipelineSetIndex];
			pipelineStateDesc._sampleDesc._count = 1;
			pipelineStateDesc._inputElements = inputElements;
			pipelineStateDesc._inputElementCount = LTN_COUNTOF(inputElements);
			pipelineStateDesc._depthComparisonFunc = rhi::COMPARISON_FUNC_LESS_EQUAL;
			pipelineStateDesc._dsvFormat = rhi::FORMAT_D32_FLOAT;

			pipelineStateDesc._numRenderTarget = 3;
			rhi::Format* visibilityBufferGeometryRtvFormats = pipelineStateDesc._rtvFormats;
			visibilityBufferGeometryRtvFormats[0] = rhi::FORMAT_R32G32_UINT;
			visibilityBufferGeometryRtvFormats[1] = rhi::FORMAT_R8_UINT;
			visibilityBufferGeometryRtvFormats[2] = rhi::FORMAT_R16G16_UNORM;

			rhi::PipelineState& pipelineState = _geometryPassPipelineStates[pipelineSetIndex];
			pipelineState.iniaitlize(pipelineStateDesc);
			pipelineState.setName("PsoGeometry");

			PipelineStateReloader::GraphicsPipelineStateRegisterDesc reloaderDesc = {};
			reloaderDesc._desc = pipelineStateDesc;
			reloaderDesc._shaderPathHashes[0] = vertexShader->_assetPathHash;
			reloaderDesc._shaderPathHashes[1] = 0;
			PipelineStateReloader::Get()->registerPipelineState(&_geometryPassPipelineStates[pipelineSetIndex], reloaderDesc);

			pixelShader.terminate();
		}

		// シェーディング パイプラインステート
		{
			AssetPath vertexShaderPath("EngineComponent\\Shader\\VisibilityBuffer\\ShadingQuad.vso");
			rhi::ShaderBlob vertexShader;
			vertexShader.initialize(vertexShaderPath.get());

			const Shader* pixelShader = pipelineSet->getPixelShader();
			u32 pixelShaderIndex = shaderScene->getShaderIndex(pixelShader);

			rhi::GraphicsPipelineStateDesc pipelineStateDesc = {};
			pipelineStateDesc._device = device;
			pipelineStateDesc._vs = vertexShader.getShaderByteCode();
			pipelineStateDesc._ps = gpuShaderScene->getShader(pixelShaderIndex)->getShaderByteCode();
			pipelineStateDesc._numRenderTarget = 1;
			pipelineStateDesc._rtvFormats[0] = rhi::FORMAT_R8G8B8A8_UNORM;
			pipelineStateDesc._topologyType = rhi::PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			pipelineStateDesc._rootSignature = &_shadingPassRootSignatures[pipelineSetIndex];
			pipelineStateDesc._sampleDesc._count = 1;
			pipelineStateDesc._depthComparisonFunc = rhi::COMPARISON_FUNC_EQUAL;
			pipelineStateDesc._dsvFormat = rhi::FORMAT_D16_UNORM;
			pipelineStateDesc._depthWriteMask = rhi::DEPTH_WRITE_MASK_ZERO;

			rhi::PipelineState& pipelineState = _shadingPassPipelineStates[pipelineSetIndex];
			pipelineState.iniaitlize(pipelineStateDesc);
			pipelineState.setName("PsoShading");

			PipelineStateReloader::GraphicsPipelineStateRegisterDesc reloaderDesc = {};
			reloaderDesc._desc = pipelineStateDesc;
			reloaderDesc._shaderPathHashes[0] = 0;
			reloaderDesc._shaderPathHashes[1] = pixelShader->_assetPathHash;
			PipelineStateReloader::Get()->registerPipelineState(&_shadingPassPipelineStates[pipelineSetIndex], reloaderDesc);

			{
				AssetPath debugShaderPath("EngineComponent\\Shader\\Material\\DebugGeometry.pso");
				rhi::ShaderBlob debugShader;
				debugShader.initialize(debugShaderPath.get());

				u32 debugPixelShaderIndex = shaderScene->getShaderIndex(_debugVisualizeShader);
				pipelineStateDesc._ps = debugShader.getShaderByteCode();
				rhi::PipelineState& debugPipelineState = _debugVisualizePipelineStates[pipelineSetIndex];
				debugPipelineState.iniaitlize(pipelineStateDesc);
				debugPipelineState.setName("PsoDebugShading");

				debugShader.terminate();
			}

			vertexShader.terminate();
		}
	}

	// 破棄
	const UpdateInfos<PipelineSet>* destroyInfos = pipelineSetScene->getDestroyInfos();
	u32 destroyCount = destroyInfos->getUpdateCount();
	auto destroyPipelineSets = destroyInfos->getObjects();
	for (u32 i = 0; i < destroyCount; ++i) {
		const PipelineSet* pipelineSet = destroyPipelineSets[i];
		u32 pipelineSetIndex = pipelineSetScene->getPipelineSetIndex(pipelineSet);

		PipelineStateReloader::Get()->unregisterPipelineState(&_geometryPassPipelineStates[pipelineSetIndex]);
		PipelineStateReloader::Get()->unregisterPipelineState(&_shadingPassPipelineStates[pipelineSetIndex]);

		_debugVisualizePipelineStates[pipelineSetIndex].terminate();
		_geometryPassPipelineStates[pipelineSetIndex].terminate();
		_shadingPassPipelineStates[pipelineSetIndex].terminate();
		_geometryPassRootSignatures[pipelineSetIndex].terminate();
		_shadingPassRootSignatures[pipelineSetIndex].terminate();
	}

	updateMaterialParameterOffsets();
	updateMaterialParameters();
}

void GpuMaterialManager::updateMaterialParameters() {
	VramUpdater* vramUpdater = VramUpdater::Get();
	MaterialParameterContainer* materialInstanceScene = MaterialParameterContainer::Get();

	const UpdateInfos<MaterialParameterSet>* updateInfos = materialInstanceScene->getMaterialParameterUpdateInfos();
	u32 updateCount = updateInfos->getUpdateCount();
	auto updateMaterialInstances = updateInfos->getObjects();
	for (u32 i = 0; i < updateCount; ++i) {
		const MaterialParameterSet* materialInstance = updateMaterialInstances[i];
		u32 materialParameterIndex = materialInstanceScene->getMaterialParameterIndex(materialInstance->_parameters);
		u32 materialParameterSize = materialInstance->_sizeInByte;

		u8* gpuMaterialParameters = vramUpdater->enqueueUpdate<u8>(&_parameterGpuBuffer, materialParameterIndex, materialParameterSize);
		memcpy(gpuMaterialParameters, materialInstance->_parameters, materialParameterSize);
	}
}

void GpuMaterialManager::updateMaterialParameterOffsets() {
	VramUpdater* vramUpdater = VramUpdater::Get();
	MaterialParameterContainer* materialInstanceScene = MaterialParameterContainer::Get();
	MaterialScene* materialScene = MaterialScene::Get();
	auto materialCreateInfos = materialScene->getMaterialCreateInfos();
	u32 materialCreateCount = materialCreateInfos->getUpdateCount();
	auto createMaterials = materialCreateInfos->getObjects();
	for (u32 i = 0; i < materialCreateCount; ++i) {
		u32 materialIndex = materialScene->getMaterialIndex(createMaterials[i]);
		u32 materialParameterIndex = materialInstanceScene->getMaterialParameterIndex(createMaterials[i]->getParameters());

		u32* materialParameterOffset = vramUpdater->enqueueUpdate<u32>(&_parameterOffsetGpuBuffer, materialIndex);
		*materialParameterOffset = materialParameterIndex;
	}
}

GpuMaterialManager* GpuMaterialManager::Get() {
	return &g_gpuMaterialManager;
}
}
