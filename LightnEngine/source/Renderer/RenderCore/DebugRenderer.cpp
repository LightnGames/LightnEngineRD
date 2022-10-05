#include "DebugRenderer.h"
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/VramUpdater.h>
#include <Renderer/RenderCore/RendererUtility.h>

namespace ltn {
namespace {
DebugRenderer _debugSystem;
}

void DebugRenderer::initialize() {
	_lineInstances.initialize(LINE_INSTANCE_CPU_COUNT_MAX);
	_boxInstances.initialize(BOX_INSTANCE_CPU_COUNT_MAX);
	rhi::Device* device = DeviceManager::Get()->getDevice();

	// root signature
	{
		rhi::DescriptorRange viewCbvRange(rhi::DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		rhi::DescriptorRange lineInstanceSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		rhi::DescriptorRange viewDepthSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		rhi::RootParameter rootParameters[3] = {};
		rootParameters[0].initializeDescriptorTable(1, &viewCbvRange, rhi::SHADER_VISIBILITY_ALL);
		rootParameters[1].initializeDescriptorTable(1, &lineInstanceSrvRange, rhi::SHADER_VISIBILITY_VERTEX);
		rootParameters[2].initializeDescriptorTable(1, &viewDepthSrvRange, rhi::SHADER_VISIBILITY_PIXEL);

		rhi::StaticSamplerDesc samplerDesc = {};
		samplerDesc._filter = rhi::FILTER_MIN_MAG_MIP_POINT;
		samplerDesc._addressU = rhi::TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc._addressV = rhi::TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc._addressW = rhi::TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc._mipLODBias = 0;
		samplerDesc._maxAnisotropy = 16;
		samplerDesc._comparisonFunc = rhi::COMPARISON_FUNC_NEVER;
		samplerDesc._borderColor = rhi::STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		samplerDesc._minLOD = 0.0f;
		samplerDesc._maxLOD = FLT_MAX;
		samplerDesc._shaderRegister = 0;
		samplerDesc._registerSpace = 0;
		samplerDesc._shaderVisibility = rhi::SHADER_VISIBILITY_PIXEL;

		rhi::RootSignatureDesc rootSignatureDesc = {};
		rootSignatureDesc._device = device;
		rootSignatureDesc._numParameters = LTN_COUNTOF(rootParameters);
		rootSignatureDesc._parameters = rootParameters;
		rootSignatureDesc._numStaticSamplers = 1;
		rootSignatureDesc._staticSamplers = &samplerDesc;
		_rootSignature.iniaitlize(rootSignatureDesc);
	}

	// pipeline state
	{
		rhi::ShaderBlob pixelShader;
		{
			AssetPath shaderPath("EngineComponent\\Shader\\Debug\\Debug3dLine.pso");
			pixelShader.initialize(shaderPath.get());
		}

		rhi::GraphicsPipelineStateDesc defaultPipelineStateDesc = {};
		defaultPipelineStateDesc._device = device;
		defaultPipelineStateDesc._ps = pixelShader.getShaderByteCode();
		defaultPipelineStateDesc._numRenderTarget = 1;
		defaultPipelineStateDesc._rtvFormats[0] = rhi::FORMAT_R11G11B10_FLOAT;
		defaultPipelineStateDesc._topologyType = rhi::PRIMITIVE_TOPOLOGY_TYPE_LINE;
		defaultPipelineStateDesc._rootSignature = &_rootSignature;
		defaultPipelineStateDesc._depthWriteMask = rhi::DEPTH_WRITE_MASK_ZERO;
		defaultPipelineStateDesc._sampleDesc._count = 1;

		rhi::RenderTargetBlendDesc& debugOcclusionBlendDesc = defaultPipelineStateDesc._blendDesc._renderTarget[0];
		debugOcclusionBlendDesc._blendEnable = true;
		debugOcclusionBlendDesc._srcBlend = rhi::BLEND_SRC_ALPHA;
		debugOcclusionBlendDesc._destBlend = rhi::BLEND_INV_SRC_ALPHA;
		debugOcclusionBlendDesc._blendOp = rhi::BLEND_OP_ADD;
		debugOcclusionBlendDesc._srcBlendAlpha = rhi::BLEND_ONE;
		debugOcclusionBlendDesc._destBlendAlpha = rhi::BLEND_ZERO;
		debugOcclusionBlendDesc._blendOpAlpha = rhi::BLEND_OP_ADD;

		// debug line
		{
			AssetPath shaderPath("EngineComponent\\Shader\\Debug\\Debug3dLine.vso");
			rhi::ShaderBlob vertexShader;
			vertexShader.initialize(shaderPath.get());

			rhi::GraphicsPipelineStateDesc pipelineStateDesc = defaultPipelineStateDesc;
			pipelineStateDesc._vs = vertexShader.getShaderByteCode();
			_debugLinePipelineState.iniaitlize(pipelineStateDesc);
			vertexShader.terminate();
		}

		// debug box
		{
			AssetPath shaderPath("EngineComponent\\Shader\\Debug\\Debug3dBox.vso");
			rhi::ShaderBlob vertexShader;
			vertexShader.initialize(shaderPath.get());

			rhi::GraphicsPipelineStateDesc pipelineStateDesc = defaultPipelineStateDesc;
			pipelineStateDesc._vs = vertexShader.getShaderByteCode();
			_debugBoxPipelineState.iniaitlize(pipelineStateDesc);
			vertexShader.terminate();
		}

		pixelShader.terminate();
	}

	// buffer
	{
		GpuBufferDesc defaultDesc = {};
		defaultDesc._device = device;
		defaultDesc._initialState = rhi::RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		{
			GpuBufferDesc desc = defaultDesc;
			desc._sizeInByte = sizeof(LineInstance) * LINE_INSTANCE_CPU_COUNT_MAX;
			_lineInstanceGpuBuffer.initialize(desc);
			_lineInstanceGpuBuffer.setName("DebugCpuLine");
		}

		{
			GpuBufferDesc desc = defaultDesc;
			desc._sizeInByte = sizeof(BoxInstance) * BOX_INSTANCE_CPU_COUNT_MAX;
			_boxInstanceGpuBuffer.initialize(desc);
			_boxInstanceGpuBuffer.setName("DebugCpuBox");
		}
	}

	// line instance descriptor
	{
		DescriptorAllocatorGroup* allocator = DescriptorAllocatorGroup::Get();
		_lineInstanceSrv = allocator->allocateSrvCbvUavGpu();

		rhi::ShaderResourceViewDesc desc = {};
		desc._format = rhi::FORMAT_UNKNOWN;
		desc._viewDimension = rhi::SRV_DIMENSION_BUFFER;
		desc._buffer._firstElement = 0;
		desc._buffer._flags = rhi::BUFFER_SRV_FLAG_NONE;
		desc._buffer._numElements = LINE_INSTANCE_CPU_COUNT_MAX;
		desc._buffer._structureByteStride = sizeof(LineInstance);
		device->createShaderResourceView(_lineInstanceGpuBuffer.getResource(), &desc, _lineInstanceSrv._cpuHandle);
	}

	// box instance descriptor
	{
		DescriptorAllocatorGroup* allocator = DescriptorAllocatorGroup::Get();
		_boxInstanceSrv = allocator->allocateSrvCbvUavGpu();

		rhi::ShaderResourceViewDesc desc = {};
		desc._format = rhi::FORMAT_UNKNOWN;
		desc._viewDimension = rhi::SRV_DIMENSION_BUFFER;
		desc._buffer._firstElement = 0;
		desc._buffer._flags = rhi::BUFFER_SRV_FLAG_NONE;
		desc._buffer._numElements = BOX_INSTANCE_CPU_COUNT_MAX;
		desc._buffer._structureByteStride = sizeof(BoxInstance);
		device->createShaderResourceView(_boxInstanceGpuBuffer.getResource(), &desc, _boxInstanceSrv._cpuHandle);
	}
}

void DebugRenderer::update() {
	VramUpdater* vramUpdater = VramUpdater::Get();
	u32 lineCount = _lineInstances.getCount();
	if (lineCount > 0) {
		LineInstance* lineInstances = vramUpdater->enqueueUpdate<LineInstance>(&_lineInstanceGpuBuffer, 0, lineCount);
		memcpy(lineInstances, _lineInstances.get(), sizeof(LineInstance) * lineCount);
	}

	u32 boxCount = _boxInstances.getCount();
	if (boxCount > 0) {
		BoxInstance* boxInstances = vramUpdater->enqueueUpdate<BoxInstance>(&_boxInstanceGpuBuffer, 0, boxCount);
		memcpy(boxInstances, _boxInstances.get(), sizeof(BoxInstance) * boxCount);
	}
}

void DebugRenderer::terminate() {
	_lineInstances.terminate();
	_lineInstanceGpuBuffer.terminate();
	_boxInstances.terminate();
	_boxInstanceGpuBuffer.terminate();
	_debugLinePipelineState.terminate();
	_debugBoxPipelineState.terminate();
	_rootSignature.terminate();

	DescriptorAllocatorGroup* allocator = DescriptorAllocatorGroup::Get();
	allocator->deallocSrvCbvUavGpu(_lineInstanceSrv);
	allocator->deallocSrvCbvUavGpu(_boxInstanceSrv);
}

void DebugRenderer::render(const RenderDesc& desc) {
	rhi::CommandList* commandList = desc._commandList;
	DEBUG_MARKER_CPU_GPU_SCOPED_TIMER(commandList, Color4(), "Debug Draw");

	ScopedBarrierDesc barriers[] = {
		ScopedBarrierDesc(desc._viewDepthGpuTexture, rhi::RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
	};
	ScopedBarrier scopedBarriers(commandList, barriers, LTN_COUNTOF(barriers));

	commandList->setGraphicsRootSignature(&_rootSignature);
	commandList->setPrimitiveTopology(rhi::PRIMITIVE_TOPOLOGY_LINELIST);
	commandList->setGraphicsRootDescriptorTable(0, desc._viewCbv);
	commandList->setGraphicsRootDescriptorTable(2, desc._viewDepthSrv);

	// cpu line
	{
		u32 instanceCount = _lineInstances.getCount();
		if (instanceCount > 0) {
			commandList->setPipelineState(&_debugLinePipelineState);
			commandList->setGraphicsRootDescriptorTable(1, _lineInstanceSrv._gpuHandle);
			commandList->drawInstanced(2, instanceCount, 0, 0);
		}
	}

	// cpu box
	{
		u32 instanceCount = _boxInstances.getCount();
		if (instanceCount > 0) {
			commandList->setPipelineState(&_debugBoxPipelineState);
			commandList->setGraphicsRootDescriptorTable(1, _boxInstanceSrv._gpuHandle);
			commandList->drawInstanced(24, instanceCount, 0, 0);
		}
	}

	_lineInstances.reset();
	_boxInstances.reset();
}

void DebugRenderer::drawLine(Vector3 startPosition, Vector3 endPosition, Color color) {
	LineInstance* line = _lineInstances.alloc();
	line->_startPosition = startPosition.getFloat3();
	line->_endPosition = endPosition.getFloat3();
	line->_color = color.getColor4();
}

void DebugRenderer::drawBox(Matrix4 matrix, Color color) {
	BoxInstance* box = _boxInstances.alloc();
	box->_matrixWorld = matrix.getFloat3x4();
	box->_color = color.getColor4();
}

void DebugRenderer::drawAabb(Vector3 boundsMin, Vector3 boundsMax, Color color) {
	Vector3 center = (boundsMin + boundsMax) / 2.0f;
	Vector3 size = boundsMax - boundsMin;
	Matrix4 matrixWorld = Matrix4::scaleXYZ(size) * Matrix4::translateXYZ(center);
	drawBox(matrixWorld, color);
}

void DebugRenderer::drawFrustum(Matrix4 view, Matrix4 projection, Color color) {
	Vector3 viewTranslate = view.getTranslation();
	Vector3 viewForward = view.getCol(2);
	Vector3 viewUp = view.getCol(1);
	Vector3 viewRight = view.getCol(0);

	constexpr f32 VISUALIZE_OFFSET = 0.5f;
	f32 nearClip = -projection.getCol(3)[2] / projection.getCol(2)[2];
	f32 farClip = (nearClip * projection.getCol(2)[2] / (projection.getCol(2)[2] - 1.0f)) * VISUALIZE_OFFSET;
	Vector3 nearProj = viewForward * nearClip;
	Vector3 farProj = viewForward * farClip;
	Vector3 upProj = viewUp * projection.getCol(0)[0];
	Vector3 rightProj = viewRight * (1.0f * projection.getCol(1)[1]);

	Vector3 farCornerRightTop = farProj + rightProj * farClip + upProj * farClip + viewTranslate;
	Vector3 farCornerLeftTop = farProj - rightProj * farClip + upProj * farClip + viewTranslate;
	Vector3 farCornerRightBot = farProj + rightProj * farClip - upProj * farClip + viewTranslate;
	Vector3 farCornerLeftBot = farProj - rightProj * farClip - upProj * farClip + viewTranslate;

	Vector3 nearCornerRightTop = nearProj + rightProj * nearClip + upProj * nearClip + viewTranslate;
	Vector3 nearCornerLeftTop = nearProj - rightProj * nearClip + upProj * nearClip + viewTranslate;
	Vector3 nearCornerRightBot = nearProj + rightProj * nearClip - upProj * nearClip + viewTranslate;
	Vector3 nearCornerLeftBot = nearProj - rightProj * nearClip - upProj * nearClip + viewTranslate;

	drawLine(nearCornerRightTop, farCornerRightTop, color);
	drawLine(nearCornerLeftTop, farCornerLeftTop, color);
	drawLine(nearCornerRightBot, farCornerRightBot, color);
	drawLine(nearCornerLeftBot, farCornerLeftBot, color);

	drawLine(farCornerRightTop, farCornerLeftTop, color);
	drawLine(farCornerLeftTop, farCornerLeftBot, color);
	drawLine(farCornerLeftBot, farCornerRightBot, color);
	drawLine(farCornerRightBot, farCornerRightTop, color);

	drawLine(nearCornerRightTop, nearCornerLeftTop, color);
	drawLine(nearCornerLeftTop, nearCornerLeftBot, color);
	drawLine(nearCornerLeftBot, nearCornerRightBot, color);
	drawLine(nearCornerRightBot, nearCornerRightTop, color);
}

DebugRenderer* DebugRenderer::Get() {
	return &_debugSystem;
}
}