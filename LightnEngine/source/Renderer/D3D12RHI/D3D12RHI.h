#pragma once
#include <Core/Utility.h>
#include <Renderer/RHI/RhiDef.h>
#include <d3d12.h>
#include <dxgi1_4.h>

namespace ltn {
namespace rhi {
struct ShaderBlobD3D12 {
	virtual void initialize(const char* filePath) = 0;
	virtual void terminate() = 0;
	virtual ShaderByteCode getShaderByteCode() const = 0;

	ID3DBlob* _blob = nullptr;
};

struct HardwareFactoryD3D12 {
	virtual void initialize(const HardwareFactoryDesc& desc) = 0;
	virtual void terminate() = 0;

	IDXGIFactory4* _factory = nullptr;
};

struct HardwareAdapterD3D12 {
	virtual void initialize(const HardwareAdapterDesc& desc) = 0;
	virtual void terminate() = 0;
	virtual QueryVideoMemoryInfo queryVideoMemoryInfo() = 0;

	IDXGIAdapter3* _adapter = nullptr;
};

struct DeviceD3D12 {
	virtual void initialize(const DeviceDesc& desc) = 0;
	virtual void terminate() = 0;
	virtual u32 getDescriptorHandleIncrementSize(DescriptorHeapType type) const = 0;
	virtual void createRenderTargetView(Resource* resource, CpuDescriptorHandle destDescriptor) = 0;
	virtual void createDepthStencilView(Resource* resource, CpuDescriptorHandle destDescriptor) = 0;
	virtual void createCommittedResource(HeapType heapType, HeapFlags heapFlags, const ResourceDesc& desc,
		ResourceStates initialResourceState, const ClearValue* optimizedClearValue, Resource* dstResource) = 0;
	virtual void createConstantBufferView(const ConstantBufferViewDesc& desc, CpuDescriptorHandle destDescriptor) = 0;
	virtual void createShaderResourceView(Resource* resource, const ShaderResourceViewDesc* desc, CpuDescriptorHandle destDescriptor) = 0;
	virtual void createUnorderedAccessView(Resource* resource, Resource* counterResource, const UnorderedAccessViewDesc* desc, CpuDescriptorHandle destDescriptor) = 0;
	virtual void getCopyableFootprints(const ResourceDesc* resourceDesc, u32 firstSubresource, u32 numSubresources, u64 baseOffset, PlacedSubresourceFootprint* layouts, u32* numRows, u64* rowSizeInBytes, u64* totalBytes) = 0;
	virtual u8 getFormatPlaneCount(Format format) = 0;

	ID3D12Device2* _device = nullptr;
};

struct SwapChainD3D12 {
	virtual void initialize(const SwapChainDesc& desc) = 0;
	virtual void terminate() = 0;

	virtual u32 getCurrentBackBufferIndex() = 0;
	virtual void present(u32 syncInterval, u32 flags) = 0;
	virtual void getBackBuffer(Resource& resource, u32 index) = 0;

	IDXGISwapChain3* _swapChain = nullptr;
};

struct CommandQueueD3D12 {
	virtual void initialize(const CommandQueueDesc& desc) = 0;
	virtual void terminate() = 0;
	virtual void executeCommandLists(u32 count, CommandList** commandLists) = 0;
	virtual void getTimestampFrequency(u64* frequency) = 0;

	bool isFenceComplete(u64 fenceValue);
	void waitForIdle();
	void waitForFence(u64 fenceValue);
	u64 incrimentFence();
	u64 getCompletedValue() const;

	ID3D12CommandQueue* _commandQueue = nullptr;
	ID3D12Fence* _fence = nullptr;
	HANDLE _fenceEvent = nullptr;
	u64 _nextFenceValue = 0;
	u64 _lastFenceValue = 0;
};

struct DescriptorHeapD3D12 {
	virtual void initialize(const DescriptorHeapDesc& desc) = 0;
	virtual void terminate() = 0;
	virtual CpuDescriptorHandle getCPUDescriptorHandleForHeapStart() const = 0;
	virtual GpuDescriptorHandle getGPUDescriptorHandleForHeapStart() const = 0;

	ID3D12DescriptorHeap* _descriptorHeap = nullptr;
};

struct CommandListD3D12 {
	virtual void initialize(const CommandListDesc& desc) = 0;
	virtual void terminate() = 0;

	virtual void reset() = 0;
	virtual void transitionBarrierSimple(Resource* resource, ResourceStates currentState, ResourceStates nextState) = 0;
	virtual void transitionBarriers(ResourceTransitionBarrier* barriers, u32 count) = 0;
	virtual void copyBufferRegion(Resource* dstBuffer, u64 dstOffset, Resource* srcBuffer, u64 srcOffset, u64 numBytes) = 0;
	virtual void copyTextureRegion(const TextureCopyLocation* dst, u32 dstX, u32 dstY, u32 dstZ, const TextureCopyLocation* src, const Box* srcBox) = 0;
	virtual void copyResource(Resource* dstResource, Resource* srcResource) = 0;
	virtual void setDescriptorHeaps(u32 count, DescriptorHeap** descriptorHeaps) = 0;
	virtual void setViewports(u32 count, const ViewPort* viewPorts) = 0;
	virtual void setScissorRects(u32 count, const Rect* scissorRects) = 0;
	virtual void setRenderTargets(u32 count, CpuDescriptorHandle* rtvHandles, CpuDescriptorHandle* dsvHandle) = 0;
	virtual void clearRenderTargetView(CpuDescriptorHandle rtvHandle, f32 clearColor[4]) = 0;
	virtual void clearDepthStencilView(CpuDescriptorHandle depthStencilView, ClearFlags clearFlags, f32 depth, u8 stencil, u32 numRects, const Rect* rects) = 0;
	virtual void setPipelineState(PipelineState* pipelineState) = 0;
	virtual void setGraphicsRootSignature(RootSignature* rootSignature) = 0;
	virtual void setComputeRootSignature(RootSignature* rootSignature) = 0;
	virtual void setPrimitiveTopology(PrimitiveTopology primitiveTopology) = 0;
	virtual void setGraphicsRootDescriptorTable(u32 rootParameterIndex, GpuDescriptorHandle baseDescriptor) = 0;
	virtual void setComputeRootDescriptorTable(u32 rootParameterIndex, GpuDescriptorHandle baseDescriptor) = 0;
	virtual void setGraphicsRoot32BitConstants(u32 rootParameterIndex, u32 num32BitValuesToSet, const void* srcData, u32 destOffsetIn32BitValues) = 0;
	virtual void drawInstanced(u32 vertexCountPerInstance, u32 instanceCount, u32 startVertexLocation, u32 startInstanceLocation) = 0;
	virtual void drawIndexedInstanced(u32 indexCountPerInstance, u32 instanceCount, u32 startIndexLocation, s32 baseVertexLocation, u32 startInstanceLocation) = 0;
	virtual void dispatch(u32 threadGroupCountX, u32 threadGroupCountY, u32 threadGroupCountZ) = 0;
	virtual void executeIndirect(CommandSignature* commandSignature, u32 maxCommandCount, Resource* argumentBuffer, u64 argumentBufferOffset, Resource* countBuffer, u64 countBufferOffset) = 0;
	virtual void clearUnorderedAccessViewUint(GpuDescriptorHandle viewGPUHandleInCurrentHeap, CpuDescriptorHandle viewCPUHandle, Resource* resource, const u32 values[4], u32 numRects, const Rect* rects) = 0;
	virtual void setVertexBuffers(u32 startSlot, u32 numViews, const VertexBufferView* views) = 0;
	virtual void setIndexBuffer(const IndexBufferView* view) = 0;
	virtual void endQuery(QueryHeap* queryHeap, QueryType type, u32 index) = 0;
	virtual void resolveQueryData(QueryHeap* queryHeap, QueryType type, u32 startIndex, u32 numQueries, Resource* destinationBuffer, u64 alignedDestinationBufferOffset) = 0;

	u64 _fenceValue = INVAILD_FENCE_VALUE;
	ID3D12CommandAllocator* _allocator = nullptr;
	ID3D12GraphicsCommandList* _commandList = nullptr;
};

struct CommandSignatureD3D12 {
	virtual void initialize(const CommandSignatureDesc& desc) = 0;
	virtual void terminate() = 0;

	ID3D12CommandSignature* _commandSignature = nullptr;
};

struct ResourceD3D12 {
	void initialize(ID3D12Resource* resource) { _resource = resource; }
	virtual void terminate() = 0;
	virtual void* map(const MemoryRange* range) = 0;
	virtual void unmap(const MemoryRange* range) = 0;
	virtual u64 getGpuVirtualAddress() const = 0;

	ID3D12Resource* _resource = nullptr;
};

struct PipelineStateD3D12 {
	virtual void iniaitlize(const GraphicsPipelineStateDesc& desc) = 0;
	virtual void iniaitlize(const ComputePipelineStateDesc& desc) = 0;
	virtual void terminate() = 0;

	ID3D12PipelineState* _pipelineState = nullptr;
	const char* _debugName = nullptr;
};

struct RootSignatureD3D12 {
	virtual void iniaitlize(const RootSignatureDesc& desc) = 0;
	virtual void terminate() = 0;

	ID3D12RootSignature* _rootSignature = nullptr;
	const char* _debugName = nullptr;
};

class QueryHeapD3D12 {
public:
	virtual void initialize(const QueryHeapDesc& desc) = 0;
	virtual void terminate() = 0;

	ID3D12QueryHeap* _queryHeap = nullptr;
};

namespace {
DXGI_FORMAT toD3d12(Format format) {
	return static_cast<DXGI_FORMAT>(format);
}
}
}
}