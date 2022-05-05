#pragma once

#define RHI_D3D12 1
#if RHI_D3D12
#include <Renderer/D3D12RHI/D3D12RHI.h>
#define RhiBase(x) x##D3D12
#endif

namespace ltn {
namespace rhi {
struct ShaderBlob :public RhiBase(ShaderBlob) {
	void initialize(const char* filePath) override;
	void terminate() override;
	ShaderByteCode getShaderByteCode() const override;
};

struct HardwareFactory :public RhiBase(HardwareFactory) {
	void initialize(const HardwareFactoryDesc & desc) override;
	void terminate() override;
};
struct HardwareAdapter :public RhiBase(HardwareAdapter) {
	void initialize(const HardwareAdapterDesc & desc) override;
	void terminate() override;
	QueryVideoMemoryInfo queryVideoMemoryInfo() override;
};

struct Device :public RhiBase(Device) {
	void initialize(const DeviceDesc & desc) override;
	void terminate() override;

	u32 getDescriptorHandleIncrementSize(DescriptorHeapType type) const override;
	void createRenderTargetView(Resource * resource, CpuDescriptorHandle destDescriptor) override;
	void createDepthStencilView(Resource * resource, CpuDescriptorHandle destDescriptor) override;
	void createCommittedResource(HeapType heapType, HeapFlags heapFlags, const ResourceDesc & desc,
		ResourceStates initialResourceState, const ClearValue * optimizedClearValue, Resource * dstResource) override;
	void createConstantBufferView(const ConstantBufferViewDesc & desc, CpuDescriptorHandle destDescriptor) override;
	void createShaderResourceView(Resource * resource, const ShaderResourceViewDesc * desc, CpuDescriptorHandle destDescriptor) override;
	void createUnorderedAccessView(Resource * resource, Resource * counterResource, const UnorderedAccessViewDesc * desc, CpuDescriptorHandle destDescriptor) override;
	void getCopyableFootprints(const ResourceDesc * resourceDesc, u32 firstSubresource, u32 numSubresources, u64 baseOffset, PlacedSubresourceFootprint * layouts, u32 * numRows, u64 * rowSizeInBytes, u64 * totalBytes) override;
	u8 getFormatPlaneCount(Format format) override;
};

struct SwapChain :public RhiBase(SwapChain) {
	void initialize(const SwapChainDesc & desc) override;
	void terminate() override;

	u32 getCurrentBackBufferIndex() override;
	void present(u32 syncInterval, u32 flags) override;
	void getBackBuffer(Resource& resource, u32 index) override;
};

struct CommandQueue :public RhiBase(CommandQueue) {
	void initialize(const CommandQueueDesc & desc) override;
	void terminate() override;

	void getTimestampFrequency(u64 * frequency) override;
	void executeCommandLists(u32 count, CommandList * *commandLists) override;
};

struct DescriptorHeap :public RhiBase(DescriptorHeap) {
	void initialize(const DescriptorHeapDesc & desc) override;
	void terminate() override;

	CpuDescriptorHandle getCPUDescriptorHandleForHeapStart() const override;
	GpuDescriptorHandle getGPUDescriptorHandleForHeapStart() const override;
};

struct CommandList :public RhiBase(CommandList) {
	void initialize(const CommandListDesc & desc) override;
	void terminate() override;

	void reset() override;
	void transitionBarrierSimple(Resource * resource, ResourceStates currentState, ResourceStates nextState) override;
	void transitionBarriers(ResourceTransitionBarrier * barriers, u32 count) override;
	void copyBufferRegion(Resource * dstBuffer, u64 dstOffset, Resource * srcBuffer, u64 srcOffset, u64 numBytes) override;
	void copyResource(Resource * dstResource, Resource * srcResource) override;
	void copyTextureRegion(const TextureCopyLocation * dst, u32 dstX, u32 dstY, u32 dstZ, const TextureCopyLocation * src, const Box * srcBox) override;
	void setDescriptorHeaps(u32 count, DescriptorHeap * *descriptorHeaps) override;
	void setViewports(u32 count, const ViewPort * viewPorts) override;
	void setScissorRects(u32 count, const Rect * scissorRects) override;
	void setRenderTargets(u32 count, CpuDescriptorHandle * rtvHandles, CpuDescriptorHandle * dsvHandle) override;
	void clearRenderTargetView(CpuDescriptorHandle rtvHandle, f32 clearColor[4]) override;
	void clearDepthStencilView(CpuDescriptorHandle depthStencilView, ClearFlags clearFlags, f32 depth, u8 stencil, u32 numRects, const Rect * rects) override;
	void setPipelineState(PipelineState * pipelineState) override;
	void setGraphicsRootSignature(RootSignature * rootSignature) override;
	void setComputeRootSignature(RootSignature * rootSignature) override;
	void setPrimitiveTopology(PrimitiveTopology primitiveTopology) override;
	void setGraphicsRootDescriptorTable(u32 rootParameterIndex, GpuDescriptorHandle baseDescriptor) override;
	void setComputeRootDescriptorTable(u32 rootParameterIndex, GpuDescriptorHandle baseDescriptor) override;
	void setGraphicsRoot32BitConstants(u32 rootParameterIndex, u32 num32BitValuesToSet, const void* srcData, u32 destOffsetIn32BitValues) override;
	void drawInstanced(u32 vertexCountPerInstance, u32 instanceCount, u32 startVertexLocation, u32 startInstanceLocation) override;
	void drawIndexedInstanced(u32 indexCountPerInstance, u32 instanceCount, u32 startIndexLocation, s32 baseVertexLocation, u32 startInstanceLocation) override;
	void dispatch(u32 threadGroupCountX, u32 threadGroupCountY, u32 threadGroupCountZ) override;
	void executeIndirect(CommandSignature * commandSignature, u32 maxCommandCount, Resource * argumentBuffer, u64 argumentBufferOffset, Resource * countBuffer, u64 countBufferOffset) override;
	void clearUnorderedAccessViewUint(GpuDescriptorHandle viewGPUHandleInCurrentHeap, CpuDescriptorHandle viewCPUHandle, Resource * resource, const u32 values[4], u32 numRects, const Rect * rects) override;
	void setVertexBuffers(u32 startSlot, u32 numViews, const VertexBufferView * views) override;
	void setIndexBuffer(const IndexBufferView * view) override;
	void endQuery(QueryHeap * queryHeap, QueryType type, u32 index) override;
	void resolveQueryData(QueryHeap * queryHeap, QueryType type, u32 startIndex, u32 numQueries, Resource * destinationBuffer, u64 alignedDestinationBufferOffset) override;
	//void clearDepthStencilView(CpuDescriptorHandle dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
};

struct CommandSignature :public RhiBase(CommandSignature) {
	void initialize(const CommandSignatureDesc & desc) override;
	void terminate() override;
};

struct Resource :public RhiBase(Resource) {
	void terminate() override;
	void* map(const MemoryRange * range) override;
	void unmap(const MemoryRange * range = nullptr) override;
	u64 getGpuVirtualAddress() const override;
};

struct PipelineState :public RhiBase(PipelineState) {
	void iniaitlize(const GraphicsPipelineStateDesc & desc) override;
	void iniaitlize(const ComputePipelineStateDesc & desc) override;
	void terminate() override;
};
struct RootSignature :public RhiBase(RootSignature) {
	void iniaitlize(const RootSignatureDesc & desc) override;
	void terminate() override;
};

class QueryHeap :public RhiBase(QueryHeap) {
public:
	void initialize(const QueryHeapDesc & desc) override;
	void terminate() override;
};

u32 GetConstantBufferAligned(u32 sizeInByte);
u32 GetTextureBufferAligned(u32 sizeInByte);
u8 GetFormatPlaneCount(Device* device, Format format);

constexpr u32 BACK_BUFFER_COUNT = 3;
constexpr Format BACK_BUFFER_FORMAT = FORMAT_R8G8B8A8_UNORM;

//namespace DebugMarker {
//constexpr u32 SET_NAME_LENGTH_COUNT_MAX = 128;
//class ScopedEvent {
//public:
//	ScopedEvent() {}
//	ScopedEvent(CommandList* commandList, const Color4& color, const char* name, ...);
//	~ScopedEvent();
//
//	void setEvent(CommandList* commandList, const Color4& color, const char* name, va_list va);
//private:
//	CommandList* _commandList = nullptr;
//};
//
//void setMarker(CommandList* commandList, const Color4& color, const char* name, ...);
//void pushMarker(CommandList* commandList, const Color4& color, const char* name, ...);
//void popMarker(CommandList* commandList);
//}
}
}