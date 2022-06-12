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

constexpr u32 BACK_BUFFER_COUNT = 3;
constexpr Format BACK_BUFFER_FORMAT = FORMAT_R8G8B8A8_UNORM;

u32 GetConstantBufferAligned(u32 sizeInByte);
u32 GetTextureBufferAligned(u32 sizeInByte);
u8 GetFormatPlaneCount(Device* device, Format format);

static u32 BitsPerPixel(Format format) {
	switch (format) {
	case FORMAT_R32G32B32A32_TYPELESS:
	case FORMAT_R32G32B32A32_FLOAT:
	case FORMAT_R32G32B32A32_UINT:
	case FORMAT_R32G32B32A32_SINT:
		return 128;

	case FORMAT_R32G32B32_TYPELESS:
	case FORMAT_R32G32B32_FLOAT:
	case FORMAT_R32G32B32_UINT:
	case FORMAT_R32G32B32_SINT:
		return 96;

	case FORMAT_R16G16B16A16_TYPELESS:
	case FORMAT_R16G16B16A16_FLOAT:
	case FORMAT_R16G16B16A16_UNORM:
	case FORMAT_R16G16B16A16_UINT:
	case FORMAT_R16G16B16A16_SNORM:
	case FORMAT_R16G16B16A16_SINT:
	case FORMAT_R32G32_TYPELESS:
	case FORMAT_R32G32_FLOAT:
	case FORMAT_R32G32_UINT:
	case FORMAT_R32G32_SINT:
	case FORMAT_R32G8X24_TYPELESS:
	case FORMAT_D32_FLOAT_S8X24_UINT:
	case FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case FORMAT_X32_TYPELESS_G8X24_UINT:
	case FORMAT_Y416:
	case FORMAT_Y210:
	case FORMAT_Y216:
		return 64;

	case FORMAT_R10G10B10A2_TYPELESS:
	case FORMAT_R10G10B10A2_UNORM:
	case FORMAT_R10G10B10A2_UINT:
	case FORMAT_R11G11B10_FLOAT:
	case FORMAT_R8G8B8A8_TYPELESS:
	case FORMAT_R8G8B8A8_UNORM:
	case FORMAT_R8G8B8A8_UNORM_SRGB:
	case FORMAT_R8G8B8A8_UINT:
	case FORMAT_R8G8B8A8_SNORM:
	case FORMAT_R8G8B8A8_SINT:
	case FORMAT_R16G16_TYPELESS:
	case FORMAT_R16G16_FLOAT:
	case FORMAT_R16G16_UNORM:
	case FORMAT_R16G16_UINT:
	case FORMAT_R16G16_SNORM:
	case FORMAT_R16G16_SINT:
	case FORMAT_R32_TYPELESS:
	case FORMAT_D32_FLOAT:
	case FORMAT_R32_FLOAT:
	case FORMAT_R32_UINT:
	case FORMAT_R32_SINT:
	case FORMAT_R24G8_TYPELESS:
	case FORMAT_D24_UNORM_S8_UINT:
	case FORMAT_R24_UNORM_X8_TYPELESS:
	case FORMAT_X24_TYPELESS_G8_UINT:
	case FORMAT_R9G9B9E5_SHAREDEXP:
	case FORMAT_R8G8_B8G8_UNORM:
	case FORMAT_G8R8_G8B8_UNORM:
	case FORMAT_B8G8R8A8_UNORM:
	case FORMAT_B8G8R8X8_UNORM:
	case FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
	case FORMAT_B8G8R8A8_TYPELESS:
	case FORMAT_B8G8R8A8_UNORM_SRGB:
	case FORMAT_B8G8R8X8_TYPELESS:
	case FORMAT_B8G8R8X8_UNORM_SRGB:
	case FORMAT_AYUV:
	case FORMAT_Y410:
	case FORMAT_YUY2:
		return 32;

	case FORMAT_P010:
	case FORMAT_P016:
	case FORMAT_V408:
		return 24;

	case FORMAT_R8G8_TYPELESS:
	case FORMAT_R8G8_UNORM:
	case FORMAT_R8G8_UINT:
	case FORMAT_R8G8_SNORM:
	case FORMAT_R8G8_SINT:
	case FORMAT_R16_TYPELESS:
	case FORMAT_R16_FLOAT:
	case FORMAT_D16_UNORM:
	case FORMAT_R16_UNORM:
	case FORMAT_R16_UINT:
	case FORMAT_R16_SNORM:
	case FORMAT_R16_SINT:
	case FORMAT_B5G6R5_UNORM:
	case FORMAT_B5G5R5A1_UNORM:
	case FORMAT_A8P8:
	case FORMAT_B4G4R4A4_UNORM:
	case FORMAT_P208:
	case FORMAT_V208:
		return 16;

	case FORMAT_NV12:
	case FORMAT_420_OPAQUE:
	case FORMAT_NV11:
		return 12;

	case FORMAT_R8_TYPELESS:
	case FORMAT_R8_UNORM:
	case FORMAT_R8_UINT:
	case FORMAT_R8_SNORM:
	case FORMAT_R8_SINT:
	case FORMAT_A8_UNORM:
	case FORMAT_AI44:
	case FORMAT_IA44:
	case FORMAT_P8:
		return 8;

	case FORMAT_R1_UNORM:
		return 1;

	case FORMAT_BC1_TYPELESS:
	case FORMAT_BC1_UNORM:
	case FORMAT_BC1_UNORM_SRGB:
	case FORMAT_BC4_TYPELESS:
	case FORMAT_BC4_UNORM:
	case FORMAT_BC4_SNORM:
		return 4;

	case FORMAT_BC2_TYPELESS:
	case FORMAT_BC2_UNORM:
	case FORMAT_BC2_UNORM_SRGB:
	case FORMAT_BC3_TYPELESS:
	case FORMAT_BC3_UNORM:
	case FORMAT_BC3_UNORM_SRGB:
	case FORMAT_BC5_TYPELESS:
	case FORMAT_BC5_UNORM:
	case FORMAT_BC5_SNORM:
	case FORMAT_BC6H_TYPELESS:
	case FORMAT_BC6H_UF16:
	case FORMAT_BC6H_SF16:
	case FORMAT_BC7_TYPELESS:
	case FORMAT_BC7_UNORM:
	case FORMAT_BC7_UNORM_SRGB:
		return 8;

	default:
		return 0;
	}
}

static HRESULT GetSurfaceInfo(
	u32 width,
	u32 height,
	Format fmt,
	u32* outNumBytes,
	u32* outRowBytes,
	u32* outNumRows) noexcept {
	uint64_t numBytes = 0;
	uint64_t rowBytes = 0;
	uint64_t numRows = 0;

	bool bc = false;
	bool packed = false;
	bool planar = false;
	size_t bpe = 0;
	switch (fmt) {
	case FORMAT_BC1_TYPELESS:
	case FORMAT_BC1_UNORM:
	case FORMAT_BC1_UNORM_SRGB:
	case FORMAT_BC4_TYPELESS:
	case FORMAT_BC4_UNORM:
	case FORMAT_BC4_SNORM:
		bc = true;
		bpe = 8;
		break;

	case FORMAT_BC2_TYPELESS:
	case FORMAT_BC2_UNORM:
	case FORMAT_BC2_UNORM_SRGB:
	case FORMAT_BC3_TYPELESS:
	case FORMAT_BC3_UNORM:
	case FORMAT_BC3_UNORM_SRGB:
	case FORMAT_BC5_TYPELESS:
	case FORMAT_BC5_UNORM:
	case FORMAT_BC5_SNORM:
	case FORMAT_BC6H_TYPELESS:
	case FORMAT_BC6H_UF16:
	case FORMAT_BC6H_SF16:
	case FORMAT_BC7_TYPELESS:
	case FORMAT_BC7_UNORM:
	case FORMAT_BC7_UNORM_SRGB:
		bc = true;
		bpe = 16;
		break;

	case FORMAT_R8G8_B8G8_UNORM:
	case FORMAT_G8R8_G8B8_UNORM:
	case FORMAT_YUY2:
		packed = true;
		bpe = 4;
		break;

	case FORMAT_Y210:
	case FORMAT_Y216:
		packed = true;
		bpe = 8;
		break;

	case FORMAT_NV12:
	case FORMAT_420_OPAQUE:
	case FORMAT_P208:
		planar = true;
		bpe = 2;
		break;

	case FORMAT_P010:
	case FORMAT_P016:
		planar = true;
		bpe = 4;
		break;

	default:
		break;
	}

	if (bc) {
		uint64_t numBlocksWide = 0;
		if (width > 0) {
			numBlocksWide = Max(1u, (width + 3u) / 4u);
		}
		uint64_t numBlocksHigh = 0;
		if (height > 0) {
			numBlocksHigh = Max(1u, (height + 3u) / 4u);
		}
		rowBytes = numBlocksWide * bpe;
		numRows = numBlocksHigh;
		numBytes = rowBytes * numBlocksHigh;
	}
	else if (packed) {
		rowBytes = ((uint64_t(width) + 1u) >> 1) * bpe;
		numRows = uint64_t(height);
		numBytes = rowBytes * height;
	}
	else if (fmt == FORMAT_NV11) {
		rowBytes = ((uint64_t(width) + 3u) >> 2) * 4u;
		numRows = uint64_t(height) * 2u; // Direct3D makes this simplifying assumption, although it is larger than the 4:1:1 data
		numBytes = rowBytes * numRows;
	}
	else if (planar) {
		rowBytes = ((uint64_t(width) + 1u) >> 1) * bpe;
		numBytes = (rowBytes * uint64_t(height)) + ((rowBytes * uint64_t(height) + 1u) >> 1);
		numRows = height + ((uint64_t(height) + 1u) >> 1);
	}
	else {
		size_t bpp = BitsPerPixel(fmt);
		if (!bpp)
			return E_INVALIDARG;

		rowBytes = (uint64_t(width) * bpp + 7u) / 8u; // round up to nearest byte
		numRows = uint64_t(height);
		numBytes = rowBytes * height;
	}

#if defined(_M_IX86) || defined(_M_ARM) || defined(_M_HYBRID_X86_ARM64)
	static_assert(sizeof(size_t) == 4, "Not a 32-bit platform!");
	if (numBytes > UINT32_MAX || rowBytes > UINT32_MAX || numRows > UINT32_MAX)
		return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
#else
	static_assert(sizeof(size_t) == 8, "Not a 64-bit platform!");
#endif

	if (outNumBytes) {
		*outNumBytes = static_cast<u32>(numBytes);
	}
	if (outRowBytes) {
		*outRowBytes = static_cast<u32>(rowBytes);
	}
	if (outNumRows) {
		*outNumRows = static_cast<u32>(numRows);
	}

	return S_OK;
}
}
}