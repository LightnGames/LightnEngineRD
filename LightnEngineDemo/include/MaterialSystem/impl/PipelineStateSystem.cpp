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

PipelineStateGroup* PipelineStateSystem::createPipelineStateGroup(const MeshShaderPipelineStateGroupDesc& desc, const RootSignatureDesc& rootSignatureDesc) {
    u64 meshShaderHash = StrHash(desc._meshShaderFilePath);
    u64 amplificationShaderHash = 0;
    if (desc._amplificationShaderFilePath != nullptr) {
        amplificationShaderHash = StrHash(desc._amplificationShaderFilePath);
    }
    u64 pixelShaderHash = 0;
	if (desc._pixelShaderFilePath != nullptr) {
		pixelShaderHash = StrHash(desc._pixelShaderFilePath);
	}

	u64 shaderHash = meshShaderHash + amplificationShaderHash + pixelShaderHash;
    u32 findIndex = findPipelineStateGroup(shaderHash);
    if (findIndex == gpu::INVALID_INDEX) {
        findIndex = _pipelineStates.request();
        PipelineStateGroup* pipelineState = &_pipelineStates[findIndex];
		pipelineState->initialize(desc, rootSignatureDesc);
        pipelineState->setStateFlags(&_stateFlags[findIndex]);
        _pipelineStateHashes[findIndex] = shaderHash;
    }

    PipelineStateGroup* pipelineState = &_pipelineStates[findIndex];
    return pipelineState;
}

PipelineStateGroup* PipelineStateSystem::createPipelineStateGroup(const ClassicPipelineStateGroupDesc& desc, const RootSignatureDesc& rootSignatureDesc) {
    u64 vertexShaderHash = StrHash(desc._vertexShaderFilePath);
    u64 pixelShaderHash = 0;
    if (desc._pixelShaderFilePath != nullptr) {
        pixelShaderHash = StrHash(desc._pixelShaderFilePath);
    }

    u64 shaderHash = vertexShaderHash + pixelShaderHash;
    u32 findIndex = findPipelineStateGroup(shaderHash);
    if (findIndex == gpu::INVALID_INDEX) {
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

u32 PipelineStateSystem::findPipelineStateGroup(u64 hash) const {
    u32 findIndex = gpu::INVALID_INDEX;
    u32 pipelineStateCount = _pipelineStates.getArrayCountMax();
    for (u32 pipelineStateIndex = 0; pipelineStateIndex < pipelineStateCount; ++pipelineStateIndex) {
        if (_pipelineStateHashes[pipelineStateIndex] == hash) {
            findIndex = pipelineStateIndex;
            break;
        }
    }

    return findIndex;
}

void PipelineStateGroup::initialize(const MeshShaderPipelineStateGroupDesc& desc, const RootSignatureDesc& rootSignatureDesc) {
#if ENABLE_MESH_SHADER
	Device* device = GraphicsSystemImpl::Get()->getDevice();
	GraphicsApiInstanceAllocator* allocator = GraphicsApiInstanceAllocator::Get();

    _pipelineState = allocator->allocatePipelineState();
    _rootSignature = allocator->allocateRootSignature();
    _rootSignature->iniaitlize(rootSignatureDesc);
    _rootSignature->setDebugName(desc._meshShaderFilePath);

    ShaderBlob* meshShader = allocator->allocateShaderBlob();
    ShaderBlob* amplificationShader = nullptr;
    ShaderBlob* pixelShader = nullptr;
    ShaderByteCode amplificationShaderByteCode = {};
	ShaderByteCode pixelShaderByteCode = {};
    meshShader->initialize(desc._meshShaderFilePath);
    if (desc._amplificationShaderFilePath != nullptr) {
        amplificationShader = allocator->allocateShaderBlob();
        amplificationShader->initialize(desc._amplificationShaderFilePath);
        amplificationShaderByteCode = amplificationShader->getShaderByteCode();
    }

	if (desc._pixelShaderFilePath != nullptr) {
        pixelShader = allocator->allocateShaderBlob();
		pixelShader->initialize(desc._pixelShaderFilePath);
		pixelShaderByteCode = pixelShader->getShaderByteCode();
	}

    MeshPipelineStateDesc pipelineStateDesc = {};
    pipelineStateDesc._device = device;
    pipelineStateDesc._ms = meshShader->getShaderByteCode();
    pipelineStateDesc._as = amplificationShaderByteCode;
	pipelineStateDesc._ps = pixelShaderByteCode;
    pipelineStateDesc._numRenderTarget = 1;
    pipelineStateDesc._rtvFormats[0] = FORMAT_R8G8B8A8_UNORM;
    pipelineStateDesc._topologyType = PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateDesc._rootSignature = _rootSignature;
    pipelineStateDesc._sampleDesc._count = 1;
    pipelineStateDesc._dsvFormat = FORMAT_D32_FLOAT;
	pipelineStateDesc._depthComparisonFunc = desc._depthComparisonFunc;
    pipelineStateDesc._blendDesc = desc._blendDesc;
	pipelineStateDesc._fillMode = desc._fillMode;
    _pipelineState->iniaitlize(pipelineStateDesc);
    _pipelineState->setDebugName(desc._meshShaderFilePath);

    meshShader->terminate();
    if (amplificationShader != nullptr) {
        amplificationShader->terminate();
    }

	if (pixelShader != nullptr) {
		pixelShader->terminate();
	}
#endif
}

void PipelineStateGroup::initialize(const ClassicPipelineStateGroupDesc& desc, const RootSignatureDesc& rootSignatureDesc) {
    Device* device = GraphicsSystemImpl::Get()->getDevice();
    GraphicsApiInstanceAllocator* allocator = GraphicsApiInstanceAllocator::Get();

    ShaderBlob* pixelShader = nullptr;
    ShaderBlob* vertexShader = allocator->allocateShaderBlob();
    vertexShader->initialize(desc._vertexShaderFilePath);

    ShaderByteCode pixelShaderByteCode = {};
    if (desc._pixelShaderFilePath != nullptr) {
        pixelShader = allocator->allocateShaderBlob();
        pixelShader->initialize(desc._pixelShaderFilePath);
        pixelShaderByteCode = pixelShader->getShaderByteCode();;
    }

    _pipelineState = allocator->allocatePipelineState();
    _rootSignature = allocator->allocateRootSignature();
    _rootSignature->iniaitlize(rootSignatureDesc);

    InputElementDesc inputElements[2] = {};
    inputElements[0]._inputSlot = 0;
    inputElements[0]._format = FORMAT_R32G32B32_FLOAT;
    inputElements[0]._semanticName = "POSITION";

    inputElements[1]._inputSlot = 1;
    inputElements[1]._format = FORMAT_R32_UINT;
    inputElements[1]._semanticName = "PACKED_TEX";

    GraphicsPipelineStateDesc pipelineStateDesc = {};
    pipelineStateDesc._device = device;
    pipelineStateDesc._vs = vertexShader->getShaderByteCode();
    pipelineStateDesc._ps = pixelShaderByteCode;
    pipelineStateDesc._numRenderTarget = 1;
    pipelineStateDesc._rtvFormats[0] = FORMAT_R8G8B8A8_UNORM;
    pipelineStateDesc._topologyType = PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateDesc._rootSignature = _rootSignature;
    pipelineStateDesc._sampleDesc._count = 1;
    pipelineStateDesc._dsvFormat = FORMAT_D32_FLOAT;
    pipelineStateDesc._depthComparisonFunc = COMPARISON_FUNC_LESS_EQUAL;
    pipelineStateDesc._inputElements = inputElements;
    pipelineStateDesc._inputElementCount = LTN_COUNTOF(inputElements);
    _pipelineState->iniaitlize(pipelineStateDesc);

    vertexShader->terminate();
    if (pixelShader != nullptr) {
        pixelShader->terminate();
    }
}

u64 PipelineStateSystem::getPipelineStateGrpupHash(const PipelineStateGroup* group) const {
    u32 index = getGroupIndex(group);
    return _pipelineStateHashes[index];
}

void PipelineStateGroup::terminate() {
	if (_pipelineState != nullptr) {
		_pipelineState->terminate();
	}

	if (_rootSignature != nullptr) {
		_rootSignature->terminate();
	}
}

void PipelineStateGroup::requestToDestroy() {
    *_stateFlags |= PIPELINE_STATE_GROUP_FLAG_REQUEST_DESTROY;
}
