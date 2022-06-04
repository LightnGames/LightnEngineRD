#include "GpuMaterialManager.h"
#include <Core/Memory.h>
#include <RendererScene/Material.h>
#include <RendererScene/Shader.h>
#include <Renderer/RenderCore/DescriptorAllocator.h>
#include <Renderer/RenderCore/VramUpdater.h>
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/GpuShader.h>
#include <Renderer/AssetReloader/PipelineStateReloader.h>

namespace ltn {
namespace {
GpuMaterialManager g_gpuMaterialManager;
}

void GpuMaterialManager::initialize() {
	_defaultRootSignatures = Memory::allocObjects<rhi::RootSignature>(MaterialScene::MATERIAL_COUNT_MAX);
	_defaultPipelineStates = Memory::allocObjects<rhi::PipelineState>(MaterialScene::MATERIAL_COUNT_MAX);
	_parameterGpuBuffers = Memory::allocObjects<GpuBuffer>(MaterialScene::MATERIAL_COUNT_MAX);

	_parameterSrv = DescriptorAllocatorGroup::Get()->getSrvCbvUavGpuAllocator()->allocate(MaterialScene::MATERIAL_COUNT_MAX);
}

void GpuMaterialManager::terminate() {
	Memory::freeObjects(_defaultRootSignatures);
	Memory::freeObjects(_defaultPipelineStates);
	Memory::freeObjects(_parameterGpuBuffers);
	DescriptorAllocatorGroup::Get()->getSrvCbvUavGpuAllocator()->free(_parameterSrv);
}

void GpuMaterialManager::update() {
	VramUpdater* vramUpdater = VramUpdater::Get();
	MaterialScene* materialScene = MaterialScene::Get();
	ShaderScene* shaderScene = ShaderScene::Get();
	GpuShaderScene* gpuShaderScene = GpuShaderScene::Get();

	// 新規作成
	const UpdateInfos<Material>* createInfos = materialScene->getCreateInfos();
	u32 createCount = createInfos->getUpdateCount();
	auto createMaterials = createInfos->getObjects();
	for (u32 i = 0; i < createCount; ++i) {
		const Material* material = createMaterials[i];
		u32 materialIndex = materialScene->getMaterialIndex(material);
		rhi::Device* device = DeviceManager::Get()->getDevice();

		// ルートシグネチャ
		{
			rhi::RootSignatureDesc rootSignatureDesc = {};
			rootSignatureDesc._device = device;

			rhi::RootSignature& rootSignature = _defaultRootSignatures[materialIndex];
			rootSignature.iniaitlize(rootSignatureDesc);
			rootSignature.setName("RootSigScreenTriangle");
		}

		// パイプラインステート
		{
			u32 vertexShaderIndex = shaderScene->getShaderIndex(material->getVertexShader());
			u32 pixelShaderIndex = shaderScene->getShaderIndex(material->getPixelShader());

			rhi::GraphicsPipelineStateDesc pipelineStateDesc = {};
			pipelineStateDesc._device = device;
			pipelineStateDesc._vs = gpuShaderScene->getShader(vertexShaderIndex)->getShaderByteCode();
			pipelineStateDesc._ps = gpuShaderScene->getShader(pixelShaderIndex)->getShaderByteCode();
			pipelineStateDesc._numRenderTarget = 1;
			pipelineStateDesc._rtvFormats[0] = rhi::FORMAT_R8G8B8A8_UNORM;
			pipelineStateDesc._topologyType = rhi::PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			pipelineStateDesc._rootSignature = &_defaultRootSignatures[materialIndex];
			pipelineStateDesc._sampleDesc._count = 1;

			rhi::PipelineState& pipelineState = _defaultPipelineStates[materialIndex];
			pipelineState.iniaitlize(pipelineStateDesc);
			pipelineState.setName("PsoScreenTriangle");

			//PipelineStateReloader::GraphicsPipelineStateRegisterDesc reloaderDesc = {};
			//reloaderDesc._desc = pipelineStateDesc;
			//reloaderDesc._shaderPaths[0] = vertexShaderPath.get();
			//reloaderDesc._shaderPaths[1] = pixelShaderPath.get();
			//PipelineStateReloader::Get()->registerPipelineState(&_defaultPipelineStates[materialIndex], reloaderDesc);
		}
	}

	// 破棄
	const UpdateInfos<Material>* destroyInfos = materialScene->getDestroyInfos();
	u32 destroyCount = destroyInfos->getUpdateCount();
	auto destroyMaterials = createInfos->getObjects();
	for (u32 i = 0; i < destroyCount; ++i) {
		const Material* material = createMaterials[i];
		u32 materialIndex = materialScene->getMaterialIndex(material);

		_defaultPipelineStates[materialIndex].terminate();
		_defaultRootSignatures[materialIndex].terminate();
	}
}

GpuMaterialManager* GpuMaterialManager::Get() {
	return &g_gpuMaterialManager;
}
}
