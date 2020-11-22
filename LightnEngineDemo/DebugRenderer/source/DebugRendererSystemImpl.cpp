#include <DebugRenderer/impl/DebugRendererSystemImpl.h>
#include <GfxCore/impl/GraphicsSystemImpl.h>
#include <GfxCore/impl/GraphicsApiInterface.h>
#include <GfxCore/impl/ViewSystemImpl.h>

DebugRendererSystemImpl _debugSystem;

struct DispatchMeshIndirectArgument {

};

void DebugRendererSystemImpl::initialize() {
	_lineInstances.initialize(LINE_INSTANCE_COUNT_MAX);
	Device* device = GraphicsSystemImpl::Get()->getDevice();

	// ms
	{
		GraphicsApiInstanceAllocator* allocator = GraphicsApiInstanceAllocator::Get();
		ShaderBlob* vertexShader = allocator->allocateShaderBlob();
		ShaderBlob* pixelShader = allocator->allocateShaderBlob();
		vertexShader->initialize("L:/LightnEngine/resource/common/shader/debug/debug3dLine.vso");
		pixelShader->initialize("L:/LightnEngine/resource/common/shader/debug/debug3dLine.pso");

		_pipelineState = allocator->allocatePipelineState();
		_rootSignature = allocator->allocateRootSignature();

		DescriptorRange cbvDescriptorRange = {};
		cbvDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

		DescriptorRange srvDescriptorRange = {};
		srvDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		RootParameter rootParameters[2] = {};
		rootParameters[0].initializeDescriptorTable(1, &cbvDescriptorRange, SHADER_VISIBILITY_VERTEX);
		rootParameters[1].initializeDescriptorTable(1, &srvDescriptorRange, SHADER_VISIBILITY_VERTEX);

		RootSignatureDesc rootSignatureDesc = {};
		rootSignatureDesc._device = device;
		rootSignatureDesc._numParameters = LTN_COUNTOF(rootParameters);
		rootSignatureDesc._parameters = rootParameters;
		_rootSignature->iniaitlize(rootSignatureDesc);

		GraphicsPipelineStateDesc pipelineStateDesc = {};
		pipelineStateDesc._device = device;
		pipelineStateDesc._vs = vertexShader->getShaderByteCode();
		pipelineStateDesc._ps = pixelShader->getShaderByteCode();
		pipelineStateDesc._numRenderTarget = 1;
		pipelineStateDesc._rtvFormats[0] = FORMAT_R8G8B8A8_UNORM;
		pipelineStateDesc._dsvFormat = FORMAT_D32_FLOAT;
		pipelineStateDesc._topologyType = PRIMITIVE_TOPOLOGY_TYPE_LINE;
		pipelineStateDesc._rootSignature = _rootSignature;
		pipelineStateDesc._sampleDesc._count = 1;
		_pipelineState->iniaitlize(pipelineStateDesc);

		vertexShader->terminate();
		pixelShader->terminate();
	}

	{
		GraphicsApiInstanceAllocator* allocator = GraphicsApiInstanceAllocator::Get();
		_commandSignature = allocator->allocateCommandSignature();

		IndirectArgumentDesc argumentDescs[1] = {};
		argumentDescs[0]._type = INDIRECT_ARGUMENT_TYPE_DRAW;

		CommandSignatureDesc desc = {};
		desc._device = device;
		desc._byteStride = sizeof(DrawArguments);
		desc._argumentDescs = argumentDescs;
		desc._numArgumentDescs = LTN_COUNTOF(argumentDescs);
		_commandSignature->initialize(desc);
	}

	// constant buffer
	{
		GpuBufferDesc desc = {};
		desc._device = device;
		desc._initialState = RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		desc._sizeInByte = sizeof(LineInstance) * LINE_INSTANCE_COUNT_MAX;
		_lineInstanceBuffer.initialize(desc);

		desc._flags = RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		_lineInstanceGpuBuffer.initialize(desc);
	}

	// indirect argument buffer
	{
		GpuBufferDesc desc = {};
		desc._device = device;
		desc._initialState = RESOURCE_STATE_INDIRECT_ARGUMENT;
		desc._flags = RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		desc._sizeInByte = sizeof(DrawArguments);
		_lineInstanceIndirectArgumentBuffer.initialize(desc);
	}

	// line instance descriptor
	DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
	DescriptorHeapAllocator* cpuAllocator = GraphicsSystemImpl::Get()->getSrvCbvUavCpuDescriptorAllocator();
	{
		_lineInstanceHandle = allocator->allocateDescriptors(1);
		_lineInstanceGpuSrv = allocator->allocateDescriptors(1);

		ShaderResourceViewDesc desc = {};
		desc._format = FORMAT_UNKNOWN;
		desc._viewDimension = SRV_DIMENSION_BUFFER;
		desc._buffer._firstElement = 0;
		desc._buffer._flags = BUFFER_SRV_FLAG_NONE;
		desc._buffer._numElements = LINE_INSTANCE_COUNT_MAX;
		desc._buffer._structureByteStride = sizeof(LineInstance);
		device->createShaderResourceView(_lineInstanceBuffer.getResource(), &desc, _lineInstanceHandle._cpuHandle);
		device->createShaderResourceView(_lineInstanceGpuBuffer.getResource(), &desc, _lineInstanceGpuSrv._cpuHandle);
	}

	{
		u64 incrimentSize = u64(allocator->getIncrimentSize());
		_lineInstanceGpuUav = allocator->allocateDescriptors(2);
		CpuDescriptorHandle lineInstanceUav = _lineInstanceGpuUav._cpuHandle;
		CpuDescriptorHandle countUav = _lineInstanceGpuUav._cpuHandle + incrimentSize;

		UnorderedAccessViewDesc desc = {};
		desc._format = FORMAT_UNKNOWN;
		desc._viewDimension = UAV_DIMENSION_BUFFER;
		desc._buffer._firstElement = 0;
		desc._buffer._numElements = LINE_INSTANCE_COUNT_MAX;
		desc._buffer._structureByteStride = sizeof(LineInstance);
		device->createUnorderedAccessView(_lineInstanceGpuBuffer.getResource(), nullptr, &desc, lineInstanceUav);

		desc._buffer._flags = BUFFER_UAV_FLAG_RAW;
		desc._buffer._structureByteStride = 0;
		desc._buffer._numElements = sizeof(DrawArguments) / sizeof(u32);
		desc._format = FORMAT_R32_TYPELESS;
		device->createUnorderedAccessView(_lineInstanceIndirectArgumentBuffer.getResource(), nullptr, &desc, countUav);
	}
}

void DebugRendererSystemImpl::update() {
	u32 lineCount = _lineInstances.getCount();
	if (lineCount > 0) {
		LineInstance* lineInstances = GraphicsSystemImpl::Get()->getVramUpdater()->enqueueUpdate<LineInstance>(&_lineInstanceBuffer, 0, lineCount);
		memcpy(lineInstances, _lineInstances.get(), sizeof(LineInstance) * lineCount);
	}
}

void DebugRendererSystemImpl::processDeletion() {
}

void DebugRendererSystemImpl::terminate() {
	processDeletion();
	_lineInstances.terminate();
	_lineInstanceBuffer.terminate();
	_lineInstanceGpuBuffer.terminate();
	_lineInstanceIndirectArgumentBuffer.terminate();
	_pipelineState->terminate();
	_rootSignature->terminate();
	_commandSignature->terminate();

	DescriptorHeapAllocator* allocater = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
	allocater->discardDescriptor(_lineInstanceHandle);
	allocater->discardDescriptor(_lineInstanceGpuUav);
	allocater->discardDescriptor(_lineInstanceGpuSrv);
}

void DebugRendererSystemImpl::resetGpuCounter(CommandList* commandList) {
	VramBufferUpdater* vramUpdater = GraphicsSystemImpl::Get()->getVramUpdater();
	DrawArguments* arguments = vramUpdater->enqueueUpdate<DrawArguments>(&_lineInstanceIndirectArgumentBuffer, 0);
	arguments->_instanceCount = 0;
	arguments->_startInstanceLocation = 0;
	arguments->_startVertexLocation = 0;
	arguments->_vertexCountPerInstance = 2;
}

void DebugRendererSystemImpl::render(CommandList* commandList, const ViewInfo* viewInfo) {
	u32 instanceCount = _lineInstances.getCount();
	if (instanceCount > 0) {
		commandList->setViewports(1, &viewInfo->_viewPort);
		commandList->setScissorRects(1, &viewInfo->_scissorRect);

		commandList->setGraphicsRootSignature(_rootSignature);
		commandList->setPipelineState(_pipelineState);
		commandList->setPrimitiveTopology(PRIMITIVE_TOPOLOGY_LINELIST);
		commandList->setGraphicsRootDescriptorTable(0, viewInfo->_cbvHandle._gpuHandle);
		commandList->setGraphicsRootDescriptorTable(1, _lineInstanceHandle._gpuHandle);
		commandList->drawInstanced(2, instanceCount, 0, 0);

		_lineInstances.reset();
	}

	// gpu debug line
	{
		commandList->setViewports(1, &viewInfo->_viewPort);
		commandList->setScissorRects(1, &viewInfo->_scissorRect);

		commandList->setGraphicsRootSignature(_rootSignature);
		commandList->setPipelineState(_pipelineState);
		commandList->setPrimitiveTopology(PRIMITIVE_TOPOLOGY_LINELIST);
		commandList->setGraphicsRootDescriptorTable(0, viewInfo->_cbvHandle._gpuHandle);
		commandList->setGraphicsRootDescriptorTable(1, _lineInstanceGpuSrv._gpuHandle);
		commandList->executeIndirect(_commandSignature, 1, _lineInstanceIndirectArgumentBuffer.getResource(), 0, nullptr, 0);
	}
}

void DebugRendererSystemImpl::drawLine(Vector3 startPosition, Vector3 endPosition, Color4 color) {
	LineInstance* line = _lineInstances.allocate();
	line->_startPosition = startPosition.getFloat3();
	line->_endPosition = endPosition.getFloat3();
	line->_color = color;
}

void DebugRendererSystemImpl::drawAabb(Vector3 boundsMin, Vector3 boundsMax, Color4 color) {
	Vector3 size = boundsMax - boundsMin;
	Vector3 min = boundsMin;
	Vector3 max = boundsMax;
	Vector3 right = Vector3::Right * size._x;
	Vector3 up = Vector3::Up * size._y;
	Vector3 forward = Vector3::Forward * size._z;
	drawLine(min, min + right, color);
	drawLine(min, min + forward, color);
	drawLine(min + forward, min + forward + right, color);
	drawLine(min + right, min + forward + right, color);

	drawLine(min, min + up, color);
	drawLine(max, max - up, color);
	drawLine(min + forward, min + forward + up, color);
	drawLine(max - forward, max - forward - up, color);

	drawLine(max, max - right, color);
	drawLine(max, max - forward, color);
	drawLine(max - forward, max - right - forward, color);
	drawLine(max - right, max - right - forward, color);
}

void DebugRendererSystemImpl::drawFrustum(Matrix4 view, Matrix4 projection, Color4 color) {
	Vector3 viewTranslate = view.getTranslate();
	Vector3 viewForward(view.m[2][0], view.m[2][1], view.m[2][2]);
	Vector3 viewRight(view.m[0][0], view.m[0][1], view.m[0][2]);
	Vector3 viewUp = Vector3::cross(viewForward, viewRight);

	constexpr f32 VISUALIZE_OFFSET = 0.5f;
	f32 nearClip = -projection.m[3][2] / projection.m[2][2];
	f32 farClip = (nearClip * projection.m[2][2] / (projection.m[2][2] - 1.0f)) * VISUALIZE_OFFSET;
	Vector3 nearProj = viewForward * nearClip;
	Vector3 farProj = viewForward * farClip;
	Vector3 upProj = viewUp * projection.m[1][1];
	Vector3 rightProj = viewRight * (1.0f / projection.m[0][0]);

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

DebugRendererSystem* DebugRendererSystem::Get() {
	return &_debugSystem;
}

DebugRendererSystemImpl* DebugRendererSystemImpl::Get() {
	return &_debugSystem;
}
