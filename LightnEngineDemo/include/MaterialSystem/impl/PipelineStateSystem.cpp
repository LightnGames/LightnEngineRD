#include "PipelineStateSystem.h"
#include <GfxCore/impl/GraphicsSystemImpl.h>
#include <GfxCore/impl/GraphicsApiInterface.h>
#include <MeshRenderer/GpuStruct.h>

PipelineStateSystem _pipelineStateSystem;

void PipelineStateSystem::initialize() {
    _pipelineStates.initialize(PIPELINE_STATE_GROUP_COUNT_MAX);
}

void PipelineStateSystem::update() {
}

void PipelineStateSystem::processDeletion() {
    u32 pipelineStateCount = _pipelineStates.getArrayCountMax();
    for (u32 pipelineStateIndex = 0; pipelineStateIndex < pipelineStateCount; ++pipelineStateIndex) {
        if (_stateFlags[pipelineStateIndex] & PIPELINE_STATE_GROUP_FLAG_REQUEST_DESTROY) {
            _pipelineStateHashes[pipelineStateIndex] = 0;
            _stateFlags[pipelineStateIndex] = PIPELINE_STATE_GROUP_FLAG_NONE;
            _pipelineStates[pipelineStateIndex].terminate();
            _pipelineStates.discard(pipelineStateIndex);
        }
    }
}

void PipelineStateSystem::terminate() {
    LTN_ASSERT(_pipelineStates.getInstanceCount() == 0);
    _pipelineStates.terminate();
}

u32 PipelineStateSystem::getGroupIndex(const PipelineStateGroup* pipelineState) const {
    u32 index = static_cast<u32>(pipelineState - &_pipelineStates[0]);
    LTN_ASSERT(index < _pipelineStates.getArrayCountMax());
    return index;
}

PipelineStateGroup* PipelineStateSystem::createPipelineStateGroup(const PipelineStateGroupDesc& desc, const RootSignatureDesc& rootSignatureDesc) {
    u64 meshShaderHash = StrHash(desc._meshShaderFilePath);
	u64 amplificationShaderHash = StrHash(desc._amplificationShaderFilePath);
    u64 pixelShaderHash = 0;
	if (desc._pixelShaderFilePath != "") {
		pixelShaderHash = StrHash(desc._pixelShaderFilePath);
	}
	u64 shaderHash = meshShaderHash + amplificationShaderHash + pixelShaderHash;

    u32 findIndex = static_cast<u32>(-1);
    u32 pipelineStateCount = _pipelineStates.getArrayCountMax();
    for (u32 pipelineStateIndex = 0; pipelineStateIndex < pipelineStateCount; ++pipelineStateIndex) {
        if (_pipelineStateHashes[pipelineStateIndex] == shaderHash) {
            findIndex = pipelineStateIndex;
            break;
        }
    }

    if (findIndex == static_cast<u32>(-1)) {
        findIndex = _pipelineStates.request();
        PipelineStateGroup* pipelineState = &_pipelineStates[findIndex];
		pipelineState->initialize(desc, rootSignatureDesc);
        pipelineState->setStateFlags(&_stateFlags[findIndex]);
        _pipelineStateHashes[findIndex] = shaderHash;
    }

    PipelineStateGroup* pipelineState = &_pipelineStates[findIndex];
    return pipelineState;
}

PipelineStateSystem* PipelineStateSystem::Get() {
    return &_pipelineStateSystem;
}

void PipelineStateGroup::initialize(const PipelineStateGroupDesc& desc, const RootSignatureDesc& rootSignatureDesc) {
	Device* device = GraphicsSystemImpl::Get()->getDevice();
	GraphicsApiInstanceAllocator* allocator = GraphicsApiInstanceAllocator::Get();
#if ENABLE_MESH_SHADER

    _pipelineState = allocator->allocatePipelineState();
    _rootSignature = allocator->allocateRootSignature();
    _rootSignature->iniaitlize(rootSignatureDesc);

    ShaderBlob* meshShader = allocator->allocateShaderBlob();
    ShaderBlob* amplificationShader = allocator->allocateShaderBlob();
    ShaderBlob* pixelShader = nullptr;
	ShaderByteCode pixelShaderByteCode = {};
    meshShader->initialize(desc._meshShaderFilePath);
    amplificationShader->initialize(desc._amplificationShaderFilePath);

	if (desc._pixelShaderFilePath != "") {
        pixelShader = allocator->allocateShaderBlob();
		pixelShader->initialize(desc._pixelShaderFilePath);
		pixelShaderByteCode = pixelShader->getShaderByteCode();;
	}

    MeshPipelineStateDesc pipelineStateDesc = {};
    pipelineStateDesc._device = device;
    pipelineStateDesc._ms = meshShader->getShaderByteCode();
    pipelineStateDesc._as = amplificationShader->getShaderByteCode();
	pipelineStateDesc._ps = pixelShaderByteCode;
    pipelineStateDesc._numRenderTarget = 1;
    pipelineStateDesc._rtvFormats[0] = FORMAT_R8G8B8A8_UNORM;
    pipelineStateDesc._topologyType = PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateDesc._rootSignature = _rootSignature;
    pipelineStateDesc._sampleDesc._count = 1;
    pipelineStateDesc._dsvFormat = FORMAT_D32_FLOAT;
	pipelineStateDesc._depthComparisonFunc = desc._depthComparisonFunc;
    pipelineStateDesc._blendDesc = desc._blendDesc;
    _pipelineState->iniaitlize(pipelineStateDesc);

    meshShader->terminate();
    amplificationShader->terminate();

	if (pixelShader != nullptr) {
		pixelShader->terminate();
	}

    {
        GraphicsApiInstanceAllocator* allocator = GraphicsApiInstanceAllocator::Get();
        _commandSignature = allocator->allocateCommandSignature();

        IndirectArgumentDesc argumentDescs[2] = {};
        argumentDescs[0]._type = INDIRECT_ARGUMENT_TYPE_CONSTANT;
        argumentDescs[0].Constant._num32BitValuesToSet = 3;
        argumentDescs[0].Constant._rootParameterIndex = ROOT_DEFAULT_MESH_INDIRECT_CONSTANT;
        argumentDescs[1]._type = INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;

        CommandSignatureDesc desc = {};
        desc._device = device;
        desc._byteStride = sizeof(gpu::DispatchMeshIndirectArgument);
        desc._argumentDescs = argumentDescs;
        desc._numArgumentDescs = LTN_COUNTOF(argumentDescs);
        desc._rootSignature = _rootSignature;
        _commandSignature->initialize(desc);
    }
#endif
}

u64 PipelineStateSystem::getPipelineStateGrpupHash(const PipelineStateGroup* group) const {
    u32 index = getGroupIndex(group);
    return _pipelineStateHashes[index];
}

void PipelineStateGroup::terminate() {
#if ENABLE_MESH_SHADER
    _pipelineState->terminate();
    _rootSignature->terminate();
    _commandSignature->terminate();
#endif
}

void PipelineStateGroup::requestToDestroy() {
    *_stateFlags |= PIPELINE_STATE_GROUP_FLAG_REQUEST_DESTROY;
}
