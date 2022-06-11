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

	_defaultRootSignatures = Memory::allocObjects<rhi::RootSignature>(MaterialScene::MATERIAL_CAPACITY);
	_defaultPipelineStates = Memory::allocObjects<rhi::PipelineState>(MaterialScene::MATERIAL_CAPACITY);
}

void GpuMaterialManager::terminate() {
	_parameterGpuBuffer.terminate();

	Memory::freeObjects(_defaultRootSignatures);
	Memory::freeObjects(_defaultPipelineStates);
	DescriptorAllocatorGroup::Get()->getSrvCbvUavGpuAllocator()->free(_parameterSrv);
}

void GpuMaterialManager::update() {
	VramUpdater* vramUpdater = VramUpdater::Get();
	MaterialScene* materialScene = MaterialScene::Get();
	ShaderScene* shaderScene = ShaderScene::Get();
	GpuShaderScene* gpuShaderScene = GpuShaderScene::Get();

	// 新規作成
	const UpdateInfos<Material>* createInfos = materialScene->getMaterialCreateInfos();
	u32 createCount = createInfos->getUpdateCount();
	auto createMaterials = createInfos->getObjects();
	for (u32 i = 0; i < createCount; ++i) {
		const Material* material = createMaterials[i];
		u32 materialIndex = materialScene->getMaterialIndex(material);
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

			rhi::RootSignature& rootSignature = _defaultRootSignatures[materialIndex];
			rootSignature.iniaitlize(rootSignatureDesc);
			rootSignature.setName("RootSigScreenTriangle");
		}

		// パイプラインステート
		{
			const Shader* vertexShader = material->getVertexShader();
			const Shader* pixelShader = material->getPixelShader();
			u32 vertexShaderIndex = shaderScene->getShaderIndex(vertexShader);
			u32 pixelShaderIndex = shaderScene->getShaderIndex(pixelShader);

			rhi::GraphicsPipelineStateDesc pipelineStateDesc = {};
			pipelineStateDesc._device = device;
			pipelineStateDesc._vs = gpuShaderScene->getShader(vertexShaderIndex)->getShaderByteCode();
			pipelineStateDesc._ps = gpuShaderScene->getShader(pixelShaderIndex)->getShaderByteCode();
			pipelineStateDesc._numRenderTarget = 1;
			pipelineStateDesc._rtvFormats[0] = rhi::FORMAT_R8G8B8A8_UNORM;
			pipelineStateDesc._topologyType = rhi::PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			pipelineStateDesc._rootSignature = &_defaultRootSignatures[materialIndex];
			pipelineStateDesc._sampleDesc._count = 1;
			pipelineStateDesc._inputElements = inputElements;
			pipelineStateDesc._inputElementCount = LTN_COUNTOF(inputElements);
			pipelineStateDesc._depthComparisonFunc = rhi::COMPARISON_FUNC_LESS_EQUAL;

			rhi::PipelineState& pipelineState = _defaultPipelineStates[materialIndex];
			pipelineState.iniaitlize(pipelineStateDesc);
			pipelineState.setName("PsoScreenTriangle");

			PipelineStateReloader::GraphicsPipelineStateRegisterDesc reloaderDesc = {};
			reloaderDesc._desc = pipelineStateDesc;
			reloaderDesc._shaderPathHashes[0] = vertexShader->_assetPathHash;
			reloaderDesc._shaderPathHashes[1] = pixelShader->_assetPathHash;
			PipelineStateReloader::Get()->registerPipelineState(&_defaultPipelineStates[materialIndex], reloaderDesc);
		}
	}

	// 破棄
	const UpdateInfos<Material>* destroyInfos = materialScene->getMaterialDestroyInfos();
	u32 destroyCount = destroyInfos->getUpdateCount();
	auto destroyMaterials = createInfos->getObjects();
	for (u32 i = 0; i < destroyCount; ++i) {
		const Material* material = createMaterials[i];
		u32 materialIndex = materialScene->getMaterialIndex(material);

		PipelineStateReloader::Get()->unregisterPipelineState(&_defaultPipelineStates[materialIndex]);

		_defaultPipelineStates[materialIndex].terminate();
		_defaultRootSignatures[materialIndex].terminate();
	}

	updateMaterialInstances();
}

void GpuMaterialManager::updateMaterialInstances() {
	VramUpdater* vramUpdater = VramUpdater::Get();
	MaterialScene* materialScene = MaterialScene::Get();

	const UpdateInfos<MaterialInstance>* updateInfos = materialScene->getMaterialInstanceUpdateInfos();
	u32 updateCount = updateInfos->getUpdateCount();
	auto updateMaterialInstances = updateInfos->getObjects();
	for (u32 i = 0; i < updateCount; ++i) {
		const MaterialInstance* materialInstance = updateMaterialInstances[i];
		u32 materialParameterIndex = materialScene->getMaterialParameterIndex(materialInstance->getMaterialParameters());
		u32 materialParameterSize = materialInstance->getParentMaterial()->getParameterSizeInByte();

		u8* gpuMaterialParameters = vramUpdater->enqueueUpdate<u8>(&_parameterGpuBuffer, materialParameterIndex, materialParameterSize);
		memcpy(gpuMaterialParameters, materialInstance->getMaterialParameters(), materialParameterSize);
	}
}

GpuMaterialManager* GpuMaterialManager::Get() {
	return &g_gpuMaterialManager;
}
}
