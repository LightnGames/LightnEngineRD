#pragma once
#include <Core/Math.h>
#include <Renderer/RHI/RhiDef.h>

namespace ltn {
namespace rhi {
struct ShaderBlobBase {
	virtual void initialize(const char* filePath) = 0;
	virtual void terminate() = 0;
	virtual ShaderByteCode getShaderByteCode() const = 0;
};

struct HardwareFactoryBase {
	virtual void initialize(const HardwareFactoryDesc& desc) = 0;
	virtual void terminate() = 0;
};
struct HardwareAdapterBase {
	virtual void initialize(const HardwareAdapterDesc& desc) = 0;
	virtual void terminate() = 0;
	virtual QueryVideoMemoryInfo queryVideoMemoryInfo() = 0;
};

struct DeviceBase {
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
};

struct SwapChainBase {
	virtual void initialize(const SwapChainDesc& desc) = 0;
	virtual void terminate() = 0;

	virtual u32 getCurrentBackBufferIndex() = 0;
	virtual void present(u32 syncInterval, u32 flags) = 0;
	virtual void getBackBuffer(Resource& resource, u32 index) = 0;
};

struct CommandQueueBase {
	virtual void initialize(const CommandQueueDesc& desc) = 0;
	virtual void terminate() = 0;

	virtual void getTimestampFrequency(u64* frequency) = 0;
	virtual void executeCommandLists(u32 count, CommandList** commandLists) = 0;
};

struct DescriptorHeapBase {
	virtual void initialize(const DescriptorHeapDesc& desc) = 0;
	virtual void terminate() = 0;
	virtual void setName(const char* name) = 0;

	virtual CpuDescriptorHandle getCPUDescriptorHandleForHeapStart() const = 0;
	virtual GpuDescriptorHandle getGPUDescriptorHandleForHeapStart() const = 0;
};

struct CommandListBase {
	virtual void initialize(const CommandListDesc& desc) = 0;
	virtual void terminate() = 0;

	virtual void reset() = 0;
	virtual void transitionBarrierSimple(Resource* resource, ResourceStates currentState, ResourceStates nextState) = 0;
	virtual void transitionBarriers(ResourceTransitionBarrier* barriers, u32 count) = 0;
	virtual void copyBufferRegion(Resource* dstBuffer, u64 dstOffset, Resource* srcBuffer, u64 srcOffset, u64 numBytes) = 0;
	virtual void copyResource(Resource* dstResource, Resource* srcResource) = 0;
	virtual void copyTextureRegion(const TextureCopyLocation* dst, u32 dstX, u32 dstY, u32 dstZ, const TextureCopyLocation* src, const Box* srcBox) = 0;
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
	//virtual void clearDepthStencilView(CpuDescriptorHandle dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
};

struct CommandSignatureBase {
	virtual void initialize(const CommandSignatureDesc& desc) = 0;
	virtual void terminate() = 0;
	virtual void setName(const char* name) = 0;
};

struct ResourceBase {
	virtual void terminate() = 0;
	virtual void* map(const MemoryRange* range) = 0;
	virtual void unmap(const MemoryRange* range = nullptr) = 0;
	virtual void setName(const char* name) = 0;
	virtual u64 getGpuVirtualAddress() const = 0;
};

struct PipelineStateBase {
	virtual void iniaitlize(const GraphicsPipelineStateDesc& desc) = 0;
	virtual void iniaitlize(const ComputePipelineStateDesc& desc) = 0;
	virtual void terminate() = 0;
	virtual void setName(const char* name) = 0;
};
struct RootSignatureBase {
	virtual void iniaitlize(const RootSignatureDesc& desc) = 0;
	virtual void terminate() = 0;
	virtual void setName(const char* name) = 0;
};

struct QueryHeapBase {
public:
	virtual void initialize(const QueryHeapDesc& desc) = 0;
	virtual void terminate() = 0;
};

u32 GetConstantBufferAligned(u32 sizeInByte);
u32 GetTextureBufferAligned(u32 sizeInByte);
u8 GetFormatPlaneCount(Device* device, Format format);

constexpr u32 BACK_BUFFER_COUNT = 3;
constexpr Format BACK_BUFFER_FORMAT = FORMAT_R8G8B8A8_UNORM;

//class ScopedEvent {
//public:
//	ScopedEvent() {}
//	ScopedEvent(CommandList* commandList, const Color4& color, const char* name, ...);
//	~ScopedEvent();
//
//	virtual void setEvent(CommandList* commandList, const Color4& color, const char* name, va_list va);
//private:
//	CommandList* _commandList = nullptr;
//};
//
//struct DebugMarker {
//	static constexpr u32 SET_NAME_LENGTH_COUNT_MAX = 128;
//	virtual void setMarker(CommandList* commandList, const Color4& color, const char* name, ...);
//	virtual void pushMarker(CommandList* commandList, const Color4& color, const char* name, ...);
//	virtual void popMarker(CommandList* commandList);
//};
}
}