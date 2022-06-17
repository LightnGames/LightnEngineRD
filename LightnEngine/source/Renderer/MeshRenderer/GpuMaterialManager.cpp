#include "GpuMaterialManager.h"
#include <Core/Memory.h>
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

rhi::InputElementDesc inputElements[2] = {
	{"POSITION", 0, rhi::FORMAT_R32G32B32_FLOAT, 0, 0, rhi::INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	{"INSTANCE_INDEX", 0, rhi::FORMAT_R32_UINT, 1, 0, rhi::INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
};
}

void GpuMaterialManager::initialize() {
	rhi::Device* device = DeviceManager::Get()->getDevice();
	{
		GpuBufferDesc desc = {};
		desc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
		desc._device = device;
		desc._sizeInByte = MaterialScene::MATERIAL_PARAMETER_SIZE_IN_BYTE;
		desc._initialState = rhi::RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		_parameterGpuBuffer.initialize(desc);
		_parameterGpuBuffer.setName("MaterialParameters");
	}

	{
		_parameterSrv = DescriptorAllocatorGroup::Get()->getSrvCbvUavGpuAllocator()->allocate();

		rhi::ShaderResourceViewDesc desc = {};
		desc._format = rhi::FORMAT_R32_TYPELESS;
		desc._viewDimension = rhi::SRV_DIMENSION_BUFFER;
		desc._buffer._flags = rhi::BUFFER_SRV_FLAG_RAW;
		desc._buffer._numElements = _parameterGpuBuffer.getU32ElementCount();
		device->createShaderResourceView(_parameterGpuBuffer.getResource(), &desc, _parameterSrv._cpuHandle);
	}

	_chunkAllocator.allocate([this](ChunkAllocator::Allocation& allocation) {
		_defaultRootSignatures = allocation.allocateClearedObjects<rhi::RootSignature>(PipelineSetScene::PIPELINE_SET_CAPACITY);
		_defaultPipelineStates = allocation.allocateClearedObjects<rhi::PipelineState>(PipelineSetScene::PIPELINE_SET_CAPACITY);
		});
}

void GpuMaterialManager::terminate() {
	_parameterGpuBuffer.terminate();
	_chunkAllocator.free();
	DescriptorAllocatorGroup::Get()->getSrvCbvUavGpuAllocator()->free(_parameterSrv);
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

		// ルートシグネチャ
		{
			rhi::DescriptorRange viewInfoCbvRange(rhi::DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
			rhi::DescriptorRange meshInstanceSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
			rhi::DescriptorRange materialParameterSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
			rhi::DescriptorRange indirectArgumentSubInfoSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
			rhi::DescriptorRange textureSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, TextureScene::TEXTURE_CAPACITY, 0, 1);

			rhi::RootParameter rootParameters[DefaultRootParam::COUNT] = {};
			rootParameters[DefaultRootParam::VIEW_INFO].initializeDescriptorTable(1, &viewInfoCbvRange, rhi::SHADER_VISIBILITY_ALL);
			rootParameters[DefaultRootParam::MESH_INSTANCE].initializeDescriptorTable(1, &meshInstanceSrvRange, rhi::SHADER_VISIBILITY_ALL);
			rootParameters[DefaultRootParam::MATERIAL_PARAMETER].initializeDescriptorTable(1, &materialParameterSrvRange, rhi::SHADER_VISIBILITY_ALL);
			rootParameters[DefaultRootParam::INDIRECT_ARGUMENT_SUB_INFO].initializeDescriptorTable(1, &indirectArgumentSubInfoSrvRange, rhi::SHADER_VISIBILITY_ALL);
			rootParameters[DefaultRootParam::TEXTURE].initializeDescriptorTable(1, &textureSrvRange, rhi::SHADER_VISIBILITY_ALL);

			rhi::RootSignatureDesc rootSignatureDesc = {};
			rootSignatureDesc._device = device;
			rootSignatureDesc._numParameters = LTN_COUNTOF(rootParameters);
			rootSignatureDesc._parameters = rootParameters;
			rootSignatureDesc._flags = rhi::ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			rhi::RootSignature& rootSignature = _defaultRootSignatures[pipelineSetIndex];
			rootSignature.iniaitlize(rootSignatureDesc);
			rootSignature.setName("RootSigScreenTriangle");
		}

		// パイプラインステート
		{
			const Shader* vertexShader = pipelineSet->getVertexShader();
			const Shader* pixelShader = pipelineSet->getPixelShader();
			u32 vertexShaderIndex = shaderScene->getShaderIndex(vertexShader);
			u32 pixelShaderIndex = shaderScene->getShaderIndex(pixelShader);

			rhi::GraphicsPipelineStateDesc pipelineStateDesc = {};
			pipelineStateDesc._device = device;
			pipelineStateDesc._vs = gpuShaderScene->getShader(vertexShaderIndex)->getShaderByteCode();
			pipelineStateDesc._ps = gpuShaderScene->getShader(pixelShaderIndex)->getShaderByteCode();
			pipelineStateDesc._numRenderTarget = 1;
			pipelineStateDesc._rtvFormats[0] = rhi::FORMAT_R8G8B8A8_UNORM;
			pipelineStateDesc._topologyType = rhi::PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			pipelineStateDesc._rootSignature = &_defaultRootSignatures[pipelineSetIndex];
			pipelineStateDesc._sampleDesc._count = 1;
			pipelineStateDesc._inputElements = inputElements;
			pipelineStateDesc._inputElementCount = LTN_COUNTOF(inputElements);
			pipelineStateDesc._depthComparisonFunc = rhi::COMPARISON_FUNC_LESS_EQUAL;

			rhi::PipelineState& pipelineState = _defaultPipelineStates[pipelineSetIndex];
			pipelineState.iniaitlize(pipelineStateDesc);
			pipelineState.setName("PsoScreenTriangle");

			PipelineStateReloader::GraphicsPipelineStateRegisterDesc reloaderDesc = {};
			reloaderDesc._desc = pipelineStateDesc;
			reloaderDesc._shaderPathHashes[0] = vertexShader->_assetPathHash;
			reloaderDesc._shaderPathHashes[1] = pixelShader->_assetPathHash;
			PipelineStateReloader::Get()->registerPipelineState(&_defaultPipelineStates[pipelineSetIndex], reloaderDesc);
		}
	}

	// 破棄
	const UpdateInfos<PipelineSet>* destroyInfos = pipelineSetScene->getDestroyInfos();
	u32 destroyCount = destroyInfos->getUpdateCount();
	auto destroyPipelineSets = destroyInfos->getObjects();
	for (u32 i = 0; i < destroyCount; ++i) {
		const PipelineSet* pipelineSet = destroyPipelineSets[i];
		u32 pipelineSetIndex = pipelineSetScene->getPipelineSetIndex(pipelineSet);

		PipelineStateReloader::Get()->unregisterPipelineState(&_defaultPipelineStates[pipelineSetIndex]);

		_defaultPipelineStates[pipelineSetIndex].terminate();
		_defaultRootSignatures[pipelineSetIndex].terminate();
	}

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

GpuMaterialManager* GpuMaterialManager::Get() {
	return &g_gpuMaterialManager;
}
}
