#include "D3D12RHI.h"
#include <Core/Utility.h>
#include <dxgidebug.h>
#include <D3Dcompiler.h>
#include <stdio.h>

namespace ltn {
namespace rhi {
namespace {
struct CD3DX12_DEFAULT {};
extern const DECLSPEC_SELECTANY CD3DX12_DEFAULT D3D12_DEFAULT;

struct DefaultSampleMask { operator UINT() noexcept { return UINT_MAX; } };
struct DefaultSampleDesc { operator DXGI_SAMPLE_DESC() noexcept { return DXGI_SAMPLE_DESC{ 1, 0 }; } };

struct CD3DX12_DEPTH_STENCIL_DESC : public D3D12_DEPTH_STENCIL_DESC {
	CD3DX12_DEPTH_STENCIL_DESC() = default;
	explicit CD3DX12_DEPTH_STENCIL_DESC(const D3D12_DEPTH_STENCIL_DESC& o) noexcept :
		D3D12_DEPTH_STENCIL_DESC(o) {
	}
	explicit CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT) noexcept {
		DepthEnable = TRUE;
		DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		StencilEnable = FALSE;
		StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
		StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
		const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp =
		{ D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
		FrontFace = defaultStencilOp;
		BackFace = defaultStencilOp;
	}
	explicit CD3DX12_DEPTH_STENCIL_DESC(
		BOOL depthEnable,
		D3D12_DEPTH_WRITE_MASK depthWriteMask,
		D3D12_COMPARISON_FUNC depthFunc,
		BOOL stencilEnable,
		UINT8 stencilReadMask,
		UINT8 stencilWriteMask,
		D3D12_STENCIL_OP frontStencilFailOp,
		D3D12_STENCIL_OP frontStencilDepthFailOp,
		D3D12_STENCIL_OP frontStencilPassOp,
		D3D12_COMPARISON_FUNC frontStencilFunc,
		D3D12_STENCIL_OP backStencilFailOp,
		D3D12_STENCIL_OP backStencilDepthFailOp,
		D3D12_STENCIL_OP backStencilPassOp,
		D3D12_COMPARISON_FUNC backStencilFunc) noexcept {
		DepthEnable = depthEnable;
		DepthWriteMask = depthWriteMask;
		DepthFunc = depthFunc;
		StencilEnable = stencilEnable;
		StencilReadMask = stencilReadMask;
		StencilWriteMask = stencilWriteMask;
		FrontFace.StencilFailOp = frontStencilFailOp;
		FrontFace.StencilDepthFailOp = frontStencilDepthFailOp;
		FrontFace.StencilPassOp = frontStencilPassOp;
		FrontFace.StencilFunc = frontStencilFunc;
		BackFace.StencilFailOp = backStencilFailOp;
		BackFace.StencilDepthFailOp = backStencilDepthFailOp;
		BackFace.StencilPassOp = backStencilPassOp;
		BackFace.StencilFunc = backStencilFunc;
	}
};

struct CD3DX12_BLEND_DESC : public D3D12_BLEND_DESC {
	CD3DX12_BLEND_DESC() = default;
	explicit CD3DX12_BLEND_DESC(const D3D12_BLEND_DESC& o) noexcept :
		D3D12_BLEND_DESC(o) {
	}
	explicit CD3DX12_BLEND_DESC(CD3DX12_DEFAULT) noexcept {
		AlphaToCoverageEnable = FALSE;
		IndependentBlendEnable = FALSE;
		const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
		{
			FALSE,FALSE,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_LOGIC_OP_NOOP,
			D3D12_COLOR_WRITE_ENABLE_ALL,
		};
		for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
			RenderTarget[i] = defaultRenderTargetBlendDesc;
	}
};


//------------------------------------------------------------------------------------------------
struct CD3DX12_RASTERIZER_DESC : public D3D12_RASTERIZER_DESC {
	CD3DX12_RASTERIZER_DESC() = default;
	explicit CD3DX12_RASTERIZER_DESC(const D3D12_RASTERIZER_DESC& o) noexcept :
		D3D12_RASTERIZER_DESC(o) {
	}
	explicit CD3DX12_RASTERIZER_DESC(CD3DX12_DEFAULT) noexcept {
		FillMode = D3D12_FILL_MODE_SOLID;
		CullMode = D3D12_CULL_MODE_BACK;
		FrontCounterClockwise = FALSE;
		DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		DepthClipEnable = TRUE;
		MultisampleEnable = FALSE;
		AntialiasedLineEnable = FALSE;
		ForcedSampleCount = 0;
		ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	}
	explicit CD3DX12_RASTERIZER_DESC(
		D3D12_FILL_MODE fillMode,
		D3D12_CULL_MODE cullMode,
		BOOL frontCounterClockwise,
		INT depthBias,
		FLOAT depthBiasClamp,
		FLOAT slopeScaledDepthBias,
		BOOL depthClipEnable,
		BOOL multisampleEnable,
		BOOL antialiasedLineEnable,
		UINT forcedSampleCount,
		D3D12_CONSERVATIVE_RASTERIZATION_MODE conservativeRaster) noexcept {
		FillMode = fillMode;
		CullMode = cullMode;
		FrontCounterClockwise = frontCounterClockwise;
		DepthBias = depthBias;
		DepthBiasClamp = depthBiasClamp;
		SlopeScaledDepthBias = slopeScaledDepthBias;
		DepthClipEnable = depthClipEnable;
		MultisampleEnable = multisampleEnable;
		AntialiasedLineEnable = antialiasedLineEnable;
		ForcedSampleCount = forcedSampleCount;
		ConservativeRaster = conservativeRaster;
	}
};
}

D3D12_FILL_MODE toD3d12(FillMode mode) {
	return static_cast<D3D12_FILL_MODE>(mode);
}

D3D12_COMPARISON_FUNC toD3d12(ComparisonFunc func) {
	return static_cast<D3D12_COMPARISON_FUNC>(func);
}

D3D12_DEPTH_WRITE_MASK toD3d12(DepthWriteMask mask) {
	return static_cast<D3D12_DEPTH_WRITE_MASK>(mask);
}

ID3D12DescriptorHeap* toD3d12(DescriptorHeap* descriptorHeap) {
	return descriptorHeap->_descriptorHeap;
}

D3D12_CPU_DESCRIPTOR_HANDLE toD3d12(CpuDescriptorHandle handle) {
	D3D12_CPU_DESCRIPTOR_HANDLE result = { handle._ptr };
	return result;
}

D3D12_GPU_DESCRIPTOR_HANDLE toD3d12(GpuDescriptorHandle handle) {
	D3D12_GPU_DESCRIPTOR_HANDLE result = { handle._ptr };
	return result;
}

ID3D12GraphicsCommandList* toD3d12(CommandList* commandList) {
	return commandList->_commandList;
}

ID3D12RootSignature* toD3d12(RootSignature* rootSignature) {
	return rootSignature->_rootSignature;
}

ID3D12Resource* toD3d12(Resource* resource) {
	if (resource == nullptr) {
		return nullptr;
	}
	return resource->_resource;
}

D3D12_SHADER_BYTECODE toD3d12(ShaderByteCode byteCode) {
	D3D12_SHADER_BYTECODE result = {};
	result.BytecodeLength = byteCode._bytecodeLength;
	result.pShaderBytecode = byteCode._shaderBytecode;
	return result;
}

ID3D12Device2* toD3d12(Device* device) {
	return device->_device;
}

D3D12_HEAP_TYPE toD3d12(HeapType type) {
	return static_cast<D3D12_HEAP_TYPE>(type);
}

D3D12_HEAP_FLAGS toD3d12(HeapFlags flags) {
	return static_cast<D3D12_HEAP_FLAGS>(flags);
}

D3D12_CONSTANT_BUFFER_VIEW_DESC toD3d12(ConstantBufferViewDesc desc) {
	D3D12_CONSTANT_BUFFER_VIEW_DESC result = {};
	result.BufferLocation = desc._bufferLocation;
	result.SizeInBytes = desc._sizeInBytes;
	return result;
}

const D3D12_INPUT_ELEMENT_DESC* toD3d12(const InputElementDesc* desc) {
	return reinterpret_cast<const D3D12_INPUT_ELEMENT_DESC*>(desc);
}

D3D12_COMMAND_SIGNATURE_DESC toD3d12(CommandSignatureDesc desc) {
	D3D12_COMMAND_SIGNATURE_DESC result = {};
	result.ByteStride = desc._byteStride;
	result.NodeMask = desc._nodeMask;
	result.NumArgumentDescs = desc._numArgumentDescs;
	result.pArgumentDescs = reinterpret_cast<const D3D12_INDIRECT_ARGUMENT_DESC*>(desc._argumentDescs);
	return result;
}

ID3D12CommandSignature* toD3d12(CommandSignature* commandSignature) {
	return commandSignature->_commandSignature;
}

D3D12_PIPELINE_STATE_FLAGS toD3d12(PipelineStateFlags flags) {
	return static_cast<D3D12_PIPELINE_STATE_FLAGS>(flags);
}

const D3D12_RECT* toD3d12(const Rect* rect) {
	return reinterpret_cast<const D3D12_RECT*>(rect);
}

D3D12_CLEAR_FLAGS toD3d12(ClearFlags flags) {
	return static_cast<D3D12_CLEAR_FLAGS>(flags);
}

const D3D12_BOX* toD3d12(const Box* box) {
	return reinterpret_cast<const D3D12_BOX*>(box);
}

D3D12_PLACED_SUBRESOURCE_FOOTPRINT toD3d12(PlacedSubresourceFootprint layout) {
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT result = {};
	result.Offset = layout._offset;
	result.Footprint.Depth = layout._footprint._depth;
	result.Footprint.Format = toD3d12(layout._footprint._format);
	result.Footprint.Height = layout._footprint._height;
	result.Footprint.RowPitch = layout._footprint._rowPitch;
	result.Footprint.Width = layout._footprint._width;
	return result;
}

D3D12_TEXTURE_COPY_LOCATION toD3d12(TextureCopyLocation location) {
	D3D12_TEXTURE_COPY_LOCATION result = {};
	result.pResource = toD3d12(location._resource);
	result.Type = static_cast<D3D12_TEXTURE_COPY_TYPE>(location._type);
	result.PlacedFootprint = toD3d12(location._placedFootprint);
	return result;
}

const D3D12_RESOURCE_DESC* toD3d12(const ResourceDesc* desc) {
	return reinterpret_cast<const D3D12_RESOURCE_DESC*>(desc);
}

const D3D12_VERTEX_BUFFER_VIEW* toD3d12(const VertexBufferView* view) {
	return reinterpret_cast<const D3D12_VERTEX_BUFFER_VIEW*>(view);
}

const D3D12_INDEX_BUFFER_VIEW* toD3d12(const IndexBufferView* view) {
	return reinterpret_cast<const D3D12_INDEX_BUFFER_VIEW*>(view);
}

D3D12_BLEND toD3d12(const Blend& blend) {
	return static_cast<D3D12_BLEND>(blend);
}

D3D12_BLEND_OP toD3d12(const BlendOp& blend) {
	return static_cast<D3D12_BLEND_OP>(blend);
}

D3D12_LOGIC_OP toD3d12(const LogicOp& blend) {
	return static_cast<D3D12_LOGIC_OP>(blend);
}

ID3D12QueryHeap* toD3d12(QueryHeap* queryHeap) {
	return queryHeap->_queryHeap;
}

D3D12_RENDER_TARGET_BLEND_DESC toD3d12(const RenderTargetBlendDesc& desc) {
	D3D12_RENDER_TARGET_BLEND_DESC result = {};
	result.BlendEnable = desc._blendEnable;
	result.LogicOpEnable = desc._logicOpEnable;
	result.SrcBlend = toD3d12(desc._srcBlend);
	result.DestBlend = toD3d12(desc._destBlend);
	result.BlendOp = toD3d12(desc._blendOp);
	result.SrcBlendAlpha = toD3d12(desc._srcBlendAlpha);
	result.DestBlendAlpha = toD3d12(desc._destBlendAlpha);
	result.BlendOpAlpha = toD3d12(desc._blendOpAlpha);
	result.LogicOp = toD3d12(desc._logicOp);
	result.RenderTargetWriteMask = desc._renderTargetWriteMask;
	return result;
}

D3D12_BLEND_DESC toD3d12(const BlendDesc& desc) {
	D3D12_BLEND_DESC result = {};
	result.AlphaToCoverageEnable = desc._alphaToCoverageEnable;
	result.IndependentBlendEnable = desc._independentBlendEnable;
	for (u32 i = 0; i < LTN_COUNTOF(desc._renderTarget); ++i) {
		result.RenderTarget[i] = toD3d12(desc._renderTarget[i]);
	}
	return result;
}

template<class T>
void SetDebugName(T* resource, const char* name) {
	constexpr u32 SET_NAME_LENGTH_COUNT_MAX = 64;
	WCHAR wName[SET_NAME_LENGTH_COUNT_MAX] = {};
	size_t wLength = 0;
	mbstowcs_s(&wLength, wName, SET_NAME_LENGTH_COUNT_MAX, name, _TRUNCATE);
	resource->SetName(wName);
}

void HardwareFactory::initialize(const HardwareFactoryDesc& desc) {
#define ENABLE_GBV 0
	u32 dxgiFactoryFlags = 0;

	// debug Layer
	if (desc._flags & HardwareFactoryDesc::FACTROY_FLGA_DEVICE_DEBUG) {
		ID3D12Debug* debugController = nullptr;
#if ENABLE_GBV
		ID3D12Debug1* debugController1 = nullptr;
#endif
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
			debugController->EnableDebugLayer();
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

#if ENABLE_GBV
			debugController->QueryInterface(IID_PPV_ARGS(&debugController1));
			debugController1->SetEnableGPUBasedValidation(true);
#endif
		}

		debugController->Release();
#if ENABLE_GBV
		debugController1->Release();
#endif
	}

	LTN_SUCCEEDED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&_factory)));
}

void HardwareFactory::terminate() {
	_factory->Release();
}

void HardwareAdapter::initialize(const HardwareAdapterDesc& desc) {
	IDXGIFactory4* factory = desc._factory->_factory;
	for (u32 adapterIndex = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters(adapterIndex, reinterpret_cast<IDXGIAdapter**>(&_adapter)); ++adapterIndex) {
		DXGI_ADAPTER_DESC1 desc;
		_adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			continue;
		}

		if (SUCCEEDED(D3D12CreateDevice(_adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr))) {
			break;
		}
	}
}

void HardwareAdapter::terminate() {
	_adapter->Release();

}

QueryVideoMemoryInfo HardwareAdapter::queryVideoMemoryInfo() {
	DXGI_QUERY_VIDEO_MEMORY_INFO videoMemoryInfo;
	_adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &videoMemoryInfo);

	QueryVideoMemoryInfo info;
	info._budget = videoMemoryInfo.Budget;
	info._availableForReservation = videoMemoryInfo.AvailableForReservation;
	info._currentReservation = videoMemoryInfo.CurrentReservation;
	info._currentUsage = videoMemoryInfo.CurrentUsage;
	return info;
}

void Device::initialize(const DeviceDesc& desc) {
	IDXGIAdapter1* adapter = desc._adapter->_adapter;
	LTN_SUCCEEDED(D3D12CreateDevice(
		adapter,
		D3D_FEATURE_LEVEL_12_0,
		IID_PPV_ARGS(&_device)
	));

	// エラー発生時にブレーク
	ID3D12InfoQueue* infoQueue;
	_device->QueryInterface(IID_PPV_ARGS(&infoQueue));

	if (infoQueue != nullptr) {
		D3D12_MESSAGE_ID denyIds[] = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
		};
		D3D12_MESSAGE_SEVERITY severities[] = {
		  D3D12_MESSAGE_SEVERITY_INFO
		};
		D3D12_INFO_QUEUE_FILTER filter = {};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;

		infoQueue->PushStorageFilter(&filter);
		infoQueue->Release();
	}
}

void Device::terminate() {
	_device->Release();

	IDXGIDebug1* debug = nullptr;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
		debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		debug->Release();
	}
}

u32 Device::getDescriptorHandleIncrementSize(DescriptorHeapType type) const {
	return _device->GetDescriptorHandleIncrementSize(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(type));
}

void Device::createRenderTargetView(Resource* resource, CpuDescriptorHandle destDescriptor) {
	_device->CreateRenderTargetView(toD3d12(resource), nullptr, toD3d12(destDescriptor));
}

void Device::createDepthStencilView(Resource* resource, CpuDescriptorHandle destDescriptor) {
	_device->CreateDepthStencilView(toD3d12(resource), nullptr, toD3d12(destDescriptor));
}

void Device::createCommittedResource(HeapType heapType, HeapFlags heapFlags, const ResourceDesc& desc,
	ResourceStates initialResourceState, const ClearValue* optimizedClearValue, Resource* dstResource) {
	ID3D12Resource* resource = nullptr;
	D3D12_RESOURCE_DESC resourceDesc = toD3d12(desc);
	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = toD3d12(heapType);

	HRESULT hr = _device->CreateCommittedResource(&heapProperties, toD3d12(heapFlags), &resourceDesc, toD3d12(initialResourceState), toD3d12(optimizedClearValue), IID_PPV_ARGS(&resource));
	LTN_SUCCEEDED(hr);
	dstResource->initialize(resource);
}

void Device::createConstantBufferView(const ConstantBufferViewDesc& desc, CpuDescriptorHandle destDescriptor) {
	D3D12_CONSTANT_BUFFER_VIEW_DESC descD3d12 = toD3d12(desc);
	_device->CreateConstantBufferView(&descD3d12, toD3d12(destDescriptor));
}

void Device::createShaderResourceView(Resource* resource, const ShaderResourceViewDesc* desc, CpuDescriptorHandle destDescriptor) {
	const D3D12_SHADER_RESOURCE_VIEW_DESC* descD3d12 = nullptr;
	if (desc != nullptr) {
		descD3d12 = reinterpret_cast<const D3D12_SHADER_RESOURCE_VIEW_DESC*>(desc);
	}
	_device->CreateShaderResourceView(toD3d12(resource), descD3d12, toD3d12(destDescriptor));
}

void Device::createUnorderedAccessView(Resource* resource, Resource* counterResource, const UnorderedAccessViewDesc* desc, CpuDescriptorHandle destDescriptor) {
	const D3D12_UNORDERED_ACCESS_VIEW_DESC* descD3d12 = nullptr;
	ID3D12Resource* counterResourceD3d12 = nullptr;
	if (desc != nullptr) {
		descD3d12 = reinterpret_cast<const D3D12_UNORDERED_ACCESS_VIEW_DESC*>(desc);
	}

	if (counterResource != nullptr) {
		counterResourceD3d12 = toD3d12(counterResource);
	}
	_device->CreateUnorderedAccessView(toD3d12(resource), counterResourceD3d12, descD3d12, toD3d12(destDescriptor));
}

void Device::getCopyableFootprints(const ResourceDesc* resourceDesc, u32 firstSubresource, u32 numSubresources, u64 baseOffset, PlacedSubresourceFootprint* layouts, u32* numRows, u64* rowSizeInBytes, u64* totalBytes) {
	_device->GetCopyableFootprints(toD3d12(resourceDesc), firstSubresource, numSubresources, baseOffset, reinterpret_cast<D3D12_PLACED_SUBRESOURCE_FOOTPRINT*>(layouts), numRows, rowSizeInBytes, totalBytes);
}

u8 Device::getFormatPlaneCount(Format format) {
	D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = { toD3d12(format), 0 };
	if (FAILED(_device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &formatInfo, sizeof(formatInfo)))) {
		return 0;
	}
	return formatInfo.PlaneCount;
}

void SwapChain::initialize(const SwapChainDesc& desc) {
	IDXGIFactory4* factory = desc._factory->_factory;
	ID3D12CommandQueue* commandQueue = desc._commandQueue->_commandQueue;

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = desc._bufferingCount;
	swapChainDesc.Width = desc._width;
	swapChainDesc.Height = desc._height;
	swapChainDesc.Format = toD3d12(desc._format);
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	HWND hwnd = reinterpret_cast<HWND>(desc._hWnd);
	IDXGISwapChain1* swapChain = nullptr;
	LTN_SUCCEEDED(factory->CreateSwapChainForHwnd(
		commandQueue,
		hwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain
	));

	LTN_SUCCEEDED(swapChain->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&_swapChain)));

	factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
	swapChain->Release();
}

void SwapChain::terminate() {
	_swapChain->Release();
}

u32 SwapChain::getCurrentBackBufferIndex() {
	return _swapChain->GetCurrentBackBufferIndex();
}

void SwapChain::present(u32 syncInterval, u32 flags) {
	_swapChain->Present(syncInterval, flags);
}

void SwapChain::getBackBuffer(Resource& resource, u32 index) {
	ID3D12Resource** dest = &resource._resource;
	_swapChain->GetBuffer(index, IID_PPV_ARGS(dest));
}

void CommandQueue::initialize(const CommandQueueDesc& desc) {
	ID3D12Device2* device = toD3d12(desc._device);
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = static_cast<D3D12_COMMAND_LIST_TYPE>(desc._type);
	LTN_SUCCEEDED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&_commandQueue)));

	//フェンス生成
	device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));
	_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (_fenceEvent == nullptr) {
		HRESULT_FROM_WIN32(GetLastError());
	}
}

void CommandQueue::terminate() {
	CloseHandle(_fenceEvent);
	_fence->Release();
	_commandQueue->Release();
}

void CommandQueue::executeCommandLists(u32 count, CommandList** commandLists) {
	constexpr u32 COMMAND_LIST_COUNT_MAX = 16;
	ID3D12CommandList* lists[COMMAND_LIST_COUNT_MAX] = {};
	LTN_ASSERT(count <= COMMAND_LIST_COUNT_MAX);

	// D3D12 コマンドリストに変換
	for (u32 commandListIndex = 0; commandListIndex < count; ++commandListIndex) {
		lists[commandListIndex] = commandLists[commandListIndex]->_commandList;
	}

	// コマンドリストクローズ
	for (u32 commandListIndex = 0; commandListIndex < count; ++commandListIndex) {
		commandLists[commandListIndex]->_commandList->Close();
	}

	_commandQueue->ExecuteCommandLists(count, lists);

	// フェンス値を更新
	u64 fenceValue = incrimentFence();
	for (u32 commandListIndex = 0; commandListIndex < count; ++commandListIndex) {
		commandLists[commandListIndex]->_fenceValue = fenceValue;
	}
}

void CommandQueue::getTimestampFrequency(u64* frequency) {
	_commandQueue->GetTimestampFrequency(frequency);
}

bool CommandQueue::isFenceComplete(u64 fenceValue) {
	if (fenceValue > _lastCompletedFenceValue) {
		_lastCompletedFenceValue = Max(_lastCompletedFenceValue, _fence->GetCompletedValue());
	}

	return fenceValue <= _lastCompletedFenceValue;
}

void CommandQueue::waitForIdle() {
	u64 fenceValue = incrimentFence();
	waitForFence(fenceValue);
}

void CommandQueue::waitForFence(u64 fenceValue) {
	//引数のフェンス値が最新のフェンス値以下であればすでに終了している
	if (isFenceComplete(fenceValue)) {
		return;
	}

	//フェンスが完了するまで待機
	_fence->SetEventOnCompletion(fenceValue, _fenceEvent);
	WaitForSingleObject(_fenceEvent, INFINITE);
	_lastCompletedFenceValue = fenceValue;
}

u64 CommandQueue::incrimentFence() {
	_commandQueue->Signal(_fence, _nextFenceValue);
	return _nextFenceValue++;
}

u64 CommandQueue::getCompletedValue() const {
	return _fence->GetCompletedValue();
}

void DescriptorHeap::initialize(const DescriptorHeapDesc& desc) {
	ID3D12Device* device = desc._device->_device;
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = desc._numDescriptors;
	heapDesc.Type = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(desc._type);
	heapDesc.Flags = static_cast<D3D12_DESCRIPTOR_HEAP_FLAGS>(desc._flags);
	LTN_SUCCEEDED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&_descriptorHeap)));
}

void DescriptorHeap::terminate() {
	_descriptorHeap->Release();
}

void DescriptorHeap::setName(const char* name) {
	SetDebugName(_descriptorHeap, name);
}

CpuDescriptorHandle DescriptorHeap::getCPUDescriptorHandleForHeapStart() const {
	CpuDescriptorHandle handle;
	handle._ptr = _descriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr;
	return handle;
}

GpuDescriptorHandle DescriptorHeap::getGPUDescriptorHandleForHeapStart() const {
	GpuDescriptorHandle handle;
	handle._ptr = _descriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr;
	return handle;
}

void CommandList::initialize(const CommandListDesc& desc) {
	ID3D12Device2* device = toD3d12(desc._device);
	D3D12_COMMAND_LIST_TYPE type = static_cast<D3D12_COMMAND_LIST_TYPE>(desc._type);

	LTN_ASSERT(_allocator == nullptr);
	LTN_ASSERT(_commandList == nullptr);
	LTN_SUCCEEDED(device->CreateCommandAllocator(type, IID_PPV_ARGS(&_allocator)));
	LTN_SUCCEEDED(device->CreateCommandList(0, type, _allocator, nullptr, IID_PPV_ARGS(&_commandList)));
	_commandList->Close();
}

void CommandList::terminate() {
	_allocator->Release();
	_commandList->Release();
}

void CommandList::reset() {
	_allocator->Reset();
	_commandList->Reset(_allocator, nullptr);
}

void CommandList::transitionBarrierSimple(Resource* resource, ResourceStates currentState, ResourceStates nextState) {
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = toD3d12(resource);
	barrier.Transition.StateBefore = toD3d12(currentState);
	barrier.Transition.StateAfter = toD3d12(nextState);
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	_commandList->ResourceBarrier(1, &barrier);
}

void CommandList::transitionBarriers(ResourceTransitionBarrier* barriers, u32 count) {
	constexpr u32 BARRIER_COUNT_MAX = 128;
	LTN_ASSERT(count > 0);
	LTN_ASSERT(count < BARRIER_COUNT_MAX);
	D3D12_RESOURCE_BARRIER barriersD3d12[BARRIER_COUNT_MAX] = {};
	for (u32 barrierIndex = 0; barrierIndex < count; ++barrierIndex) {
		ResourceTransitionBarrier& sourceBarrier = barriers[barrierIndex];
		D3D12_RESOURCE_BARRIER& barrier = barriersD3d12[barrierIndex];
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = toD3d12(sourceBarrier._resource);
		barrier.Transition.StateBefore = toD3d12(sourceBarrier._stateBefore);
		barrier.Transition.StateAfter = toD3d12(sourceBarrier._stateAfter);
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	}
	_commandList->ResourceBarrier(count, barriersD3d12);
}

void CommandList::copyBufferRegion(Resource* dstBuffer, u64 dstOffset, Resource* srcBuffer, u64 srcOffset, u64 numBytes) {
	_commandList->CopyBufferRegion(toD3d12(dstBuffer), dstOffset, toD3d12(srcBuffer), srcOffset, numBytes);
}

void CommandList::copyTextureRegion(const TextureCopyLocation* dst, u32 dstX, u32 dstY, u32 dstZ, const TextureCopyLocation* src, const Box* srcBox) {
	D3D12_TEXTURE_COPY_LOCATION dstD3d12 = toD3d12(*dst);
	D3D12_TEXTURE_COPY_LOCATION srcD3d12 = toD3d12(*src);
	_commandList->CopyTextureRegion(&dstD3d12, dstX, dstY, dstZ, &srcD3d12, toD3d12(srcBox));
}

void CommandList::copyResource(Resource* dstResource, Resource* srcResource) {
	_commandList->CopyResource(toD3d12(dstResource), toD3d12(srcResource));
}

void CommandList::setDescriptorHeaps(u32 count, DescriptorHeap** descriptorHeaps) {
	constexpr u32 DESCRIPTOR_HEAP_COUNT_MAX = 3;
	ID3D12DescriptorHeap* heaps[DESCRIPTOR_HEAP_COUNT_MAX] = {};
	LTN_ASSERT(count <= DESCRIPTOR_HEAP_COUNT_MAX);
	for (u32 descriptorHeapIndex = 0; descriptorHeapIndex < count; ++descriptorHeapIndex) {
		heaps[descriptorHeapIndex] = descriptorHeaps[descriptorHeapIndex]->_descriptorHeap;
	}

	_commandList->SetDescriptorHeaps(count, heaps);
}

void CommandList::setViewports(u32 count, const ViewPort* viewPorts) {
	constexpr u32 VIEWPORT_COUNT_MAX = 8;
	D3D12_VIEWPORT views[VIEWPORT_COUNT_MAX] = {};
	LTN_ASSERT(count <= VIEWPORT_COUNT_MAX);
	for (u32 viewIndex = 0; viewIndex < count; ++viewIndex) {
		const ViewPort& src = viewPorts[viewIndex];
		D3D12_VIEWPORT& view = views[viewIndex];
		view.Width = src._width;
		view.Height = src._height;
		view.TopLeftX = src._topLeftX;
		view.TopLeftY = src._topLeftY;
		view.MinDepth = src._minDepth;
		view.MaxDepth = src._maxDepth;
	}

	_commandList->RSSetViewports(count, views);
}

void CommandList::setScissorRects(u32 count, const Rect* scissorRects) {
	constexpr u32 RECT_COUNT_MAX = 8;
	D3D12_RECT rects[RECT_COUNT_MAX] = {};
	LTN_ASSERT(count <= RECT_COUNT_MAX);
	for (u32 rectIndex = 0; rectIndex < count; ++rectIndex) {
		const Rect& src = scissorRects[rectIndex];
		D3D12_RECT& rect = rects[rectIndex];
		rect.top = src._top;
		rect.bottom = src._bottom;
		rect.right = src._right;
		rect.left = src._left;
	}

	_commandList->RSSetScissorRects(count, rects);
}

void CommandList::setRenderTargets(u32 count, CpuDescriptorHandle* rtvHandles, CpuDescriptorHandle* dsvHandle) {
	constexpr u32 RTV_COUNT_MAX = 16;
	D3D12_CPU_DESCRIPTOR_HANDLE handles[RTV_COUNT_MAX] = {};
	LTN_ASSERT(count <= RTV_COUNT_MAX);
	for (u32 rtvIndex = 0; rtvIndex < count; ++rtvIndex) {
		handles[rtvIndex].ptr = rtvHandles[rtvIndex]._ptr;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE dsv;
	D3D12_CPU_DESCRIPTOR_HANDLE* dsvPtr = nullptr;
	if (dsvHandle != nullptr) {
		dsv.ptr = dsvHandle->_ptr;
		dsvPtr = &dsv;
	}
	_commandList->OMSetRenderTargets(count, handles, FALSE, dsvPtr);
}

void CommandList::clearRenderTargetView(CpuDescriptorHandle rtvHandle, f32 clearColor[4]) {
	_commandList->ClearRenderTargetView(toD3d12(rtvHandle), clearColor, 0, nullptr);
}

void CommandList::clearDepthStencilView(CpuDescriptorHandle depthStencilView, ClearFlags clearFlags, f32 depth, u8 stencil, u32 numRects, const Rect* rects) {
	_commandList->ClearDepthStencilView(toD3d12(depthStencilView), toD3d12(clearFlags), depth, stencil, numRects, toD3d12(rects));
}

void CommandList::setPipelineState(PipelineState* pipelineState) {
	ID3D12PipelineState* pipelineStateD3d12 = pipelineState->_pipelineState;
	_commandList->SetPipelineState(pipelineStateD3d12);
}

void CommandList::setGraphicsRootSignature(RootSignature* rootSignature) {
	_commandList->SetGraphicsRootSignature(toD3d12(rootSignature));
}

void CommandList::setComputeRootSignature(RootSignature* rootSignature) {
	_commandList->SetComputeRootSignature(toD3d12(rootSignature));
}

void CommandList::setPrimitiveTopology(PrimitiveTopology primitiveTopology) {
	D3D12_PRIMITIVE_TOPOLOGY topology = static_cast<D3D12_PRIMITIVE_TOPOLOGY>(primitiveTopology);
	_commandList->IASetPrimitiveTopology(topology);
}

void CommandList::setGraphicsRootDescriptorTable(u32 rootParameterIndex, GpuDescriptorHandle baseDescriptor) {
	_commandList->SetGraphicsRootDescriptorTable(rootParameterIndex, toD3d12(baseDescriptor));
}

void CommandList::setComputeRootDescriptorTable(u32 rootParameterIndex, GpuDescriptorHandle baseDescriptor) {
	_commandList->SetComputeRootDescriptorTable(rootParameterIndex, toD3d12(baseDescriptor));
}

void CommandList::setGraphicsRoot32BitConstants(u32 rootParameterIndex, u32 num32BitValuesToSet, const void* srcData, u32 destOffsetIn32BitValues) {
	_commandList->SetGraphicsRoot32BitConstants(rootParameterIndex, num32BitValuesToSet, srcData, destOffsetIn32BitValues);
}

void CommandList::drawInstanced(u32 vertexCountPerInstance, u32 instanceCount, u32 startVertexLocation, u32 startInstanceLocation) {
	_commandList->DrawInstanced(vertexCountPerInstance, instanceCount, startVertexLocation, startInstanceLocation);
}

void CommandList::drawIndexedInstanced(u32 indexCountPerInstance, u32 instanceCount, u32 startIndexLocation, s32 baseVertexLocation, u32 startInstanceLocation) {
	_commandList->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
}

void CommandList::dispatch(u32 threadGroupCountX, u32 threadGroupCountY, u32 threadGroupCountZ) {
	_commandList->Dispatch(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
}

void CommandList::executeIndirect(CommandSignature* commandSignature, u32 maxCommandCount, Resource* argumentBuffer, u64 argumentBufferOffset, Resource* countBuffer, u64 countBufferOffset) {
	_commandList->ExecuteIndirect(toD3d12(commandSignature), maxCommandCount, toD3d12(argumentBuffer), argumentBufferOffset, toD3d12(countBuffer), countBufferOffset);
}

void CommandList::clearUnorderedAccessViewUint(GpuDescriptorHandle viewGPUHandleInCurrentHeap, CpuDescriptorHandle viewCPUHandle, Resource* resource, const u32 values[4], u32 numRects, const Rect* rects) {
	_commandList->ClearUnorderedAccessViewUint(toD3d12(viewGPUHandleInCurrentHeap), toD3d12(viewCPUHandle), toD3d12(resource), values, numRects, toD3d12(rects));
}

void CommandList::setVertexBuffers(u32 startSlot, u32 numViews, const VertexBufferView* views) {
	_commandList->IASetVertexBuffers(startSlot, numViews, toD3d12(views));
}

void CommandList::setIndexBuffer(const IndexBufferView* view) {
	_commandList->IASetIndexBuffer(toD3d12(view));
}

void CommandList::endQuery(QueryHeap* queryHeap, QueryType type, u32 index) {
	_commandList->EndQuery(toD3d12(queryHeap), static_cast<D3D12_QUERY_TYPE>(type), index);
}

void CommandList::resolveQueryData(QueryHeap* queryHeap, QueryType type, u32 startIndex, u32 numQueries, Resource* destinationBuffer, u64 alignedDestinationBufferOffset) {
	_commandList->ResolveQueryData(toD3d12(queryHeap), static_cast<D3D12_QUERY_TYPE>(type), startIndex, numQueries, toD3d12(destinationBuffer), alignedDestinationBufferOffset);
}

void CommandSignature::initialize(const CommandSignatureDesc& desc) {
	ID3D12Device2* device = toD3d12(desc._device);
	ID3D12RootSignature* rootSignature = nullptr;
	if (desc._rootSignature != nullptr) {
		rootSignature = toD3d12(desc._rootSignature);
	}
	D3D12_COMMAND_SIGNATURE_DESC descD3d12 = toD3d12(desc);
	device->CreateCommandSignature(&descD3d12, rootSignature, IID_PPV_ARGS(&_commandSignature));
}

void CommandSignature::terminate() {
	_commandSignature->Release();
}

void CommandSignature::setName(const char* name) {
	SetDebugName(_commandSignature, name);
}

void Resource::terminate() {
	_resource->Release();
}

void Resource::unmap(const MemoryRange* range) {
	const D3D12_RANGE* memoryRange = reinterpret_cast<const D3D12_RANGE*>(range);
	_resource->Unmap(0, memoryRange);
}

u64 Resource::getGpuVirtualAddress() const {
	return _resource->GetGPUVirtualAddress();
}

void* Resource::map(const MemoryRange* range) {
	const D3D12_RANGE* memoryRange = reinterpret_cast<const D3D12_RANGE*>(range);
	void* ptr = nullptr;
	HRESULT hr = _resource->Map(0, memoryRange, &ptr);
	LTN_SUCCEEDED(hr);
	return ptr;
}

void Resource::setName(const char* name) {
	SetDebugName(_resource, name);
}

u32 GetConstantBufferAligned(u32 sizeInByte) {
	return GetAligned(sizeInByte, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
}

u32 GetTextureBufferAligned(u32 sizeInByte) {
	return GetAligned(sizeInByte, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
}

void PipelineState::iniaitlize(const GraphicsPipelineStateDesc& desc) {
	ID3D12Device2* device = toD3d12(desc._device);

	//auto depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	//depthStencilDesc.DepthFunc = toD3d12(desc._depthComparisonFunc);
	//depthStencilDesc.DepthWriteMask = toD3d12(desc._depthWriteMask);
	auto depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout.NumElements = desc._inputElementCount;
	psoDesc.InputLayout.pInputElementDescs = toD3d12(desc._inputElements);
	psoDesc.pRootSignature = toD3d12(desc._rootSignature);
	psoDesc.VS = toD3d12(desc._vs);
	psoDesc.PS = toD3d12(desc._ps);
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(toD3d12(desc._blendDesc));
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = static_cast<D3D12_PRIMITIVE_TOPOLOGY_TYPE>(desc._topologyType);
	psoDesc.DSVFormat = toD3d12(desc._dsvFormat);
	psoDesc.SampleDesc = DefaultSampleDesc();

	LTN_ASSERT(desc._numRenderTarget <= 8);
	psoDesc.NumRenderTargets = desc._numRenderTarget;
	for (u32 renderTargetIndex = 0; renderTargetIndex < desc._numRenderTarget; ++renderTargetIndex) {
		psoDesc.RTVFormats[renderTargetIndex] = toD3d12(desc._rtvFormats[renderTargetIndex]);
	}

	device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_pipelineState));
}

void PipelineState::iniaitlize(const ComputePipelineStateDesc& desc) {
	ID3D12Device2* device = toD3d12(desc._device);
	D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineStateDesc = {};
	pipelineStateDesc.pRootSignature = toD3d12(desc._rootSignature);
	pipelineStateDesc.CS = toD3d12(desc._cs);
	pipelineStateDesc.NodeMask = desc._nodeMask;
	pipelineStateDesc.CachedPSO.CachedBlobSizeInBytes = desc._cachedPSO._cachedBlobSizeInBytes;
	pipelineStateDesc.CachedPSO.pCachedBlob = desc._cachedPSO.cachedBlob;
	pipelineStateDesc.Flags = toD3d12(desc._flags);
	device->CreateComputePipelineState(&pipelineStateDesc, IID_PPV_ARGS(&_pipelineState));
}

void PipelineState::terminate() {
	_pipelineState->Release();
}

void PipelineState::setName(const char* name) {
	SetDebugName(_pipelineState, name);
}

void RootSignature::iniaitlize(const RootSignatureDesc& desc) {
	ID3D12Device2* device = toD3d12(desc._device);

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	rootSignatureDesc.Desc_1_1.Flags = static_cast<D3D12_ROOT_SIGNATURE_FLAGS>(desc._flags);

	constexpr u32 ROOT_PARAMETER_COUNT_MAX = 32;
	D3D12_ROOT_PARAMETER1 rootParameters[ROOT_PARAMETER_COUNT_MAX] = {};
	for (u32 parameterIndex = 0; parameterIndex < desc._numParameters; ++parameterIndex) {
		const RootParameter& parameter = desc._parameters[parameterIndex];
		D3D12_ROOT_PARAMETER1& parameterD3d12 = rootParameters[parameterIndex];
		parameterD3d12.ParameterType = static_cast<D3D12_ROOT_PARAMETER_TYPE>(parameter._parameterType);
		parameterD3d12.ShaderVisibility = static_cast<D3D12_SHADER_VISIBILITY>(parameter._shaderVisibility);
		switch (parameter._parameterType) {
		case ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
			parameterD3d12.DescriptorTable.NumDescriptorRanges = parameter._descriptorTable._numDescriptorRanges;
			parameterD3d12.DescriptorTable.pDescriptorRanges = reinterpret_cast<const D3D12_DESCRIPTOR_RANGE1*>(parameter._descriptorTable._descriptorRanges);
			break;
		case ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
			parameterD3d12.Constants.Num32BitValues = parameter._constants._num32BitValues;
			parameterD3d12.Constants.RegisterSpace = parameter._constants._registerSpace;
			parameterD3d12.Constants.ShaderRegister = parameter._constants._shaderRegister;
			break;
		case ROOT_PARAMETER_TYPE_SRV:
		case ROOT_PARAMETER_TYPE_CBV:
		case ROOT_PARAMETER_TYPE_UAV:
			parameterD3d12.Descriptor.ShaderRegister = parameter._descriptor._shaderRegister;
			parameterD3d12.Descriptor.RegisterSpace = parameter._descriptor._registerSpace;
			parameterD3d12.Descriptor.Flags = static_cast<D3D12_ROOT_DESCRIPTOR_FLAGS>(parameter._descriptor._flags);
			break;
		}
	}

	D3D12_STATIC_SAMPLER_DESC samplerDescs[2] = {};
	{
		D3D12_STATIC_SAMPLER_DESC& samplerDesc = samplerDescs[0];
		samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MaxAnisotropy = D3D12_MAX_MAXANISOTROPY;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		samplerDesc.MinLOD = 0.0f;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.ShaderRegister = 0;
		samplerDesc.RegisterSpace = 0;
		samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	}
	{
		D3D12_STATIC_SAMPLER_DESC& samplerDesc = samplerDescs[1];
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MaxAnisotropy = 0;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		samplerDesc.MinLOD = 0.0f;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.ShaderRegister = 1;
		samplerDesc.RegisterSpace = 0;
		samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	}

	rootSignatureDesc.Desc_1_1.NumParameters = desc._numParameters;
	rootSignatureDesc.Desc_1_1.pParameters = rootParameters;
	rootSignatureDesc.Desc_1_1.pStaticSamplers = samplerDescs;
	rootSignatureDesc.Desc_1_1.NumStaticSamplers = LTN_COUNTOF(samplerDescs);

	ID3DBlob* signature = nullptr;
	ID3DBlob* error = nullptr;
	D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error);
	if (error != nullptr) {
		char errorMessage[512];
		memcpy(errorMessage, error->GetBufferPointer(), LTN_COUNTOF(errorMessage));
		LTN_INFO(errorMessage);
		error->Release();
	}
	LTN_ASSERT(error == nullptr);

	device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&_rootSignature));

	signature->Release();
}

void RootSignature::terminate() {
	_rootSignature->Release();
}

void RootSignature::setName(const char* name) {
	SetDebugName(_rootSignature, name);
}

void ShaderBlob::initialize(const char* filePath) {
	constexpr u32 SET_NAME_LENGTH_COUNT_MAX = 256;
	WCHAR wName[SET_NAME_LENGTH_COUNT_MAX] = {};
	size_t wLength = 0;
	mbstowcs_s(&wLength, wName, SET_NAME_LENGTH_COUNT_MAX, filePath, _TRUNCATE);
	D3DReadFileToBlob(wName, &_blob);
}

void ShaderBlob::terminate() {
	_blob->Release();
}

ShaderByteCode ShaderBlob::getShaderByteCode() const {
	ShaderByteCode byteCode = {};
	byteCode._bytecodeLength = _blob->GetBufferSize();
	byteCode._shaderBytecode = _blob->GetBufferPointer();
	return byteCode;
}

void QueryHeap::initialize(const QueryHeapDesc& desc) {
	ID3D12Device* device = toD3d12(desc._device);
	D3D12_QUERY_HEAP_DESC descD3d12 = {};
	descD3d12.Count = desc._count;
	descD3d12.NodeMask = desc._nodeMask;
	descD3d12.Type = static_cast<D3D12_QUERY_HEAP_TYPE>(desc._type);
	LTN_SUCCEEDED(device->CreateQueryHeap(&descD3d12, IID_PPV_ARGS(&_queryHeap)));
}

void QueryHeap::terminate() {
	_queryHeap->Release();
}
}
}
