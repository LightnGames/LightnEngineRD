#include "SkySphereRenderer.h"
#include <RendererScene/SkySphere.h>
#include <RendererScene/Texture.h>
#include <Renderer/MeshRenderer/GpuTextureManager.h>
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/RendererUtility.h>
#include <Renderer/RenderCore/GpuTexture.h>

namespace ltn {
namespace {
SkySphereRenderer g_skySphereRenderer;
}

void SkySphereRenderer::initialize() {
	rhi::Device* device = DeviceManager::Get()->getDevice();

	// root signature
	{
		rhi::DescriptorRange viewInfoCbvRange(rhi::DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		rhi::DescriptorRange cubeMapSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		rhi::RootParameter rootParameters[SkySphereRootParam::COUNT] = {};
		rootParameters[SkySphereRootParam::VIEW_INFO].initializeDescriptorTable(1, &viewInfoCbvRange, rhi::SHADER_VISIBILITY_VERTEX);
		rootParameters[SkySphereRootParam::CUBE_MAP].initializeDescriptorTable(1, &cubeMapSrvRange, rhi::SHADER_VISIBILITY_PIXEL);

		rhi::StaticSamplerDesc samplerDesc = {};
		samplerDesc._filter = rhi::FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc._addressU = rhi::TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc._addressV = rhi::TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc._addressW = rhi::TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc._mipLODBias = 0;
		samplerDesc._maxAnisotropy = 0;
		samplerDesc._comparisonFunc = rhi::COMPARISON_FUNC_NEVER;
		samplerDesc._borderColor = rhi::STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		samplerDesc._minLOD = 0.0f;
		samplerDesc._maxLOD = FLT_MAX;
		samplerDesc._shaderRegister = 0;
		samplerDesc._registerSpace = 0;
		samplerDesc._shaderVisibility = rhi::SHADER_VISIBILITY_ALL;

		rhi::RootSignatureDesc rootSignatureDesc = {};
		rootSignatureDesc._device = device;
		rootSignatureDesc._numParameters = LTN_COUNTOF(rootParameters);
		rootSignatureDesc._parameters = rootParameters;
		rootSignatureDesc._staticSamplers = &samplerDesc;
		rootSignatureDesc._numStaticSamplers = 1;

		_rootSignature.iniaitlize(rootSignatureDesc);
		_rootSignature.setName("SkySphereRootSignature");
	}

	// pipeline state
	{
		AssetPath vertexShaderPath("EngineComponent\\Shader\\SkySphere\\SkyScreenTriangle.vso");
		rhi::ShaderBlob vertexShader;
		vertexShader.initialize(vertexShaderPath.get());

		AssetPath pixelShaderPath("EngineComponent\\Shader\\SkySphere\\CubeMap.pso");
		rhi::ShaderBlob pixelShader;
		pixelShader.initialize(pixelShaderPath.get());

		rhi::GraphicsPipelineStateDesc pipelineStateDesc = {};
		pipelineStateDesc._device = device;
		pipelineStateDesc._vs = vertexShader.getShaderByteCode();
		pipelineStateDesc._ps = pixelShader.getShaderByteCode();
		pipelineStateDesc._dsvFormat = rhi::FORMAT_D32_FLOAT;
		pipelineStateDesc._depthComparisonFunc = rhi::COMPARISON_FUNC_EQUAL;
		pipelineStateDesc._depthWriteMask = rhi::DEPTH_WRITE_MASK_ZERO;
		pipelineStateDesc._topologyType = rhi::PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateDesc._rootSignature = &_rootSignature;
		pipelineStateDesc._sampleDesc._count = 1;
		pipelineStateDesc._numRenderTarget = 1;
		pipelineStateDesc._rtvFormats[0] = rhi::FORMAT_R11G11B10_FLOAT;

		_pipelineState.iniaitlize(pipelineStateDesc);
		_pipelineState.setName("SkySpherePipelineState");

		vertexShader.terminate();
		pixelShader.terminate();
	}
}

void SkySphereRenderer::terminate() {
	_rootSignature.terminate();
	_pipelineState.terminate();
}

void SkySphereRenderer::render(const RenderDesc& desc) {
	rhi::CommandList* commandList = desc._commandList;
	DEBUG_MARKER_CPU_GPU_SCOPED_TIMER(commandList, Color4(), "SkySphere");

	ScopedBarrierDesc barriers[] = {
		ScopedBarrierDesc(desc._viewDepthGpuTexture, rhi::RESOURCE_STATE_DEPTH_READ),
	};
	ScopedBarrier scopedBarriers(commandList, barriers, LTN_COUNTOF(barriers));

	const Texture* cubeMapTexture = SkySphereScene::Get()->getSkySphere(0)->getEnvironmentTexture();
	u32 cubeMapTextureIndex = TextureScene::Get()->getTextureIndex(cubeMapTexture);
	rhi::GpuDescriptorHandle cubeMapSrv = GpuTextureManager::Get()->getTextureGpuSrv(cubeMapTextureIndex);
	rhi::CpuDescriptorHandle rtv = desc._viewRtv;
	rhi::CpuDescriptorHandle dsv = desc._viewDsv;
	commandList->setGraphicsRootSignature(&_rootSignature);
	commandList->setPipelineState(&_pipelineState);
	commandList->setRenderTargets(1, &rtv, &dsv);
	commandList->setPrimitiveTopology(rhi::PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->setGraphicsRootDescriptorTable(SkySphereRootParam::CUBE_MAP, cubeMapSrv);
	commandList->setGraphicsRootDescriptorTable(SkySphereRootParam::VIEW_INFO, desc._viewCbv);
	commandList->drawInstanced(3, 1, 0, 0);
}

SkySphereRenderer* SkySphereRenderer::Get() {
	return &g_skySphereRenderer;
}
}
