#pragma once
#include <Core/ChunkAllocator.h>
#include <Renderer/RenderCore/GpuBuffer.h>
#include <Renderer/RenderCore/DescriptorAllocator.h>
namespace ltn {
class LodStreamingManager {
public:
	void initialize();
	void terminate();
	void update();

	void copyReadbackBuffers(rhi::CommandList* commandList);
	void clearBuffers(rhi::CommandList* commandList);

	GpuBuffer* getMeshMinLodLevelGpuBuffer() { return &_meshMinLodLevelGpuBuffer; }
	GpuBuffer* getMeshMaxLodLevelGpuBuffer() { return &_meshMaxLodLevelGpuBuffer; }
	GpuBuffer* getMeshInstanceLodLevelGpuBuffer() { return &_meshInstanceLodLevelGpuBuffer; }
	GpuBuffer* getMeshInstanceScreenPersentageGpuBuffer() { return &_meshInstanceScreenPersentageGpuBuffer; }
	GpuBuffer* getMaterialScreenPesentageGpuBuffer() { return &_materialScreenPersentageGpuBuffer; }

	rhi::GpuDescriptorHandle getMeshInstanceLodLevelGpuSrv() const { return _meshInstanceLodLevelSrv._firstHandle._gpuHandle; }
	rhi::GpuDescriptorHandle getMeshInstanceLodLevelGpuUav() const { return _meshInstanceLodLevelUav._firstHandle._gpuHandle; }
	rhi::GpuDescriptorHandle getMeshLodLevelGpuSrv() const { return _meshLodLevelSrv._firstHandle._gpuHandle; }
	rhi::GpuDescriptorHandle getMeshLodLevelGpuUav() const { return _meshLodLevelUav._firstHandle._gpuHandle; }
	rhi::GpuDescriptorHandle getMaterialScreenPersentageGpuUav() const { return _materialScreenPersentageUav._gpuHandle; }
	rhi::GpuDescriptorHandle getMaterialScreenPersentageGpuSrv() const { return _materialScreenPersentageSrv._gpuHandle; }

	static LodStreamingManager* Get();

private:
	void updateMeshStreaming();
	void updateTextureStreaming();
	void computeTextureStreamingMipLevels(u32 materialIndex);

private:
	static constexpr u8 INVALID_TEXTURE_STREAMING_LEVEL = UINT8_MAX;
	static constexpr u16 INVALID_MESH_STREAMING_LEVEL = UINT16_MAX;

	u32* _meshLodMinLevels = nullptr;
	u32* _meshLodMaxLevels = nullptr;
	u32* _meshScreenPersentages = nullptr;
	u32* _materialScreenPersentages = nullptr;
	u8* _prevTextureStreamingLevels = nullptr;
	u8* _textureStreamingLevels = nullptr;
	u32 _frameCounter = 0;

	GpuBuffer _meshMinLodLevelGpuBuffer;
	GpuBuffer _meshMaxLodLevelGpuBuffer;
	GpuBuffer _meshMinLodLevelReadbackBuffer;
	GpuBuffer _meshMaxLodLevelReadbackBuffer;
	GpuBuffer _meshInstanceLodLevelGpuBuffer;
	GpuBuffer _meshInstanceScreenPersentageGpuBuffer;
	GpuBuffer _materialScreenPersentageGpuBuffer;
	GpuBuffer _materialScreenPersentageReadbackBuffer;

	DescriptorHandles _meshLodLevelSrv;
	DescriptorHandles _meshLodLevelUav;
	DescriptorHandles _meshLodLevelCpuUav;
	DescriptorHandles _meshInstanceLodLevelSrv;
	DescriptorHandles _meshInstanceLodLevelUav;
	DescriptorHandle _materialScreenPersentageSrv;
	DescriptorHandle _materialScreenPersentageUav;
	DescriptorHandle _materialScreenPersentageCpuUav;
	ChunkAllocator _chunkAllocator;
};
}