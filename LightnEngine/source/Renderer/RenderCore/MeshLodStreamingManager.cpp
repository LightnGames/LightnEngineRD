#include "MeshLodStreamingManager.h"
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/GlobalVideoMemoryAllocator.h>
#include <Renderer/MeshRenderer/GeometryResourceManager.h>
#include <Renderer/MeshRenderer/GpuTextureManager.h>
#include <RendererScene/MeshGeometry.h>
#include <RendererScene/MeshInstance.h>
#include <RendererScene/Material.h>
#include <RendererScene/Texture.h>
#include <RendererScene/Material.h>
#include <RendererScene/Shader.h>
#include <Application/Application.h>
#include <ThiredParty/ImGui/imgui.h>

namespace ltn {
namespace {
LodStreamingManager g_meshLodStreamingManager;
}
void LodStreamingManager::initialize() {
	rhi::Device* device = DeviceManager::Get()->getDevice();

	{
		GpuBufferDesc desc = {};
		desc._device = device;
		desc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
		desc._initialState = rhi::RESOURCE_STATE_COPY_SOURCE;
		desc._flags = rhi::RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		desc._sizeInByte = MeshInstanceScene::MESH_INSTANCE_CAPACITY * sizeof(u32);
		_meshInstanceLodLevelGpuBuffer.initialize(desc);
		_meshInstanceLodLevelGpuBuffer.setName("MeshInstanceLodLevel");

		_meshInstanceScreenPersentageGpuBuffer.initialize(desc);
		_meshInstanceScreenPersentageGpuBuffer.setName("MeshInstanceScreenPersentages");

		desc._sizeInByte = MaterialScene::MATERIAL_CAPACITY * sizeof(u32);
		_materialScreenPersentageGpuBuffer.initialize(desc);
		_materialScreenPersentageGpuBuffer.setName("MaterialScreenPersentages");

		desc._sizeInByte = MeshGeometryScene::MESH_GEOMETRY_CAPACITY * sizeof(u32);
		_meshMinLodLevelGpuBuffer.initialize(desc);
		_meshMinLodLevelGpuBuffer.setName("RequestedMeshMinLod");

		_meshMaxLodLevelGpuBuffer.initialize(desc);
		_meshMaxLodLevelGpuBuffer.setName("RequestedMeshMaxLod");

		desc._initialState = rhi::RESOURCE_STATE_COPY_DEST;
		desc._heapType = rhi::HEAP_TYPE_READBACK;
		desc._flags = rhi::RESOURCE_FLAG_NONE;
		desc._allocator = nullptr;
		_meshMinLodLevelReadbackBuffer.initialize(desc);
		_meshMinLodLevelReadbackBuffer.setName("RequestedMeshMinLodReadback");

		_meshMaxLodLevelReadbackBuffer.initialize(desc);
		_meshMaxLodLevelReadbackBuffer.setName("RequestedMeshMaxLodReadback");

		desc._sizeInByte = MaterialScene::MATERIAL_CAPACITY * sizeof(u32);
		_materialScreenPersentageReadbackBuffer.initialize(desc);
		_materialScreenPersentageReadbackBuffer.setName("MaterialScreenPersentageReadback");
	}

	{
		DescriptorAllocator* descriptorGpuAllocator = DescriptorAllocatorGroup::Get()->getSrvCbvUavGpuAllocator();
		_meshLodLevelSrv = descriptorGpuAllocator->allocate(2);
		_meshLodLevelUav = descriptorGpuAllocator->allocate(2);
		_meshInstanceLodLevelSrv = descriptorGpuAllocator->allocate(2);
		_meshInstanceLodLevelUav = descriptorGpuAllocator->allocate(2);
		_materialScreenPersentageUav = descriptorGpuAllocator->allocate();
		_materialScreenPersentageSrv = descriptorGpuAllocator->allocate();

		DescriptorAllocator* descriptorCpuAllocator = DescriptorAllocatorGroup::Get()->getSrvCbvUavCpuAllocator();
		_meshLodLevelCpuUav = descriptorCpuAllocator->allocate(2);
		_materialScreenPersentageCpuUav = descriptorCpuAllocator->allocate();

		// SRV
		{
			rhi::ShaderResourceViewDesc desc = {};
			desc._format = rhi::FORMAT_R32_TYPELESS;
			desc._viewDimension = rhi::SRV_DIMENSION_BUFFER;
			desc._buffer._flags = rhi::BUFFER_SRV_FLAG_RAW;
			desc._buffer._numElements = _meshMinLodLevelGpuBuffer.getU32ElementCount();
			device->createShaderResourceView(_meshMinLodLevelGpuBuffer.getResource(), &desc, _meshLodLevelSrv.get(0)._cpuHandle);
			device->createShaderResourceView(_meshMaxLodLevelGpuBuffer.getResource(), &desc, _meshLodLevelSrv.get(1)._cpuHandle);

			desc._buffer._numElements = _meshInstanceLodLevelGpuBuffer.getU32ElementCount();
			device->createShaderResourceView(_meshInstanceLodLevelGpuBuffer.getResource(), &desc, _meshInstanceLodLevelSrv.get(0)._cpuHandle);
			device->createShaderResourceView(_meshInstanceScreenPersentageGpuBuffer.getResource(), &desc, _meshInstanceLodLevelSrv.get(1)._cpuHandle);

			desc._buffer._numElements = _materialScreenPersentageGpuBuffer.getU32ElementCount();
			device->createShaderResourceView(_materialScreenPersentageGpuBuffer.getResource(), &desc, _materialScreenPersentageSrv._cpuHandle);
		}

		// UAV
		{
			rhi::UnorderedAccessViewDesc desc = {};
			desc._viewDimension = rhi::UAV_DIMENSION_BUFFER;
			desc._format = rhi::FORMAT_R32_TYPELESS;
			desc._buffer._flags = rhi::BUFFER_UAV_FLAG_RAW;
			desc._buffer._numElements = _meshMinLodLevelGpuBuffer.getU32ElementCount();
			device->createUnorderedAccessView(_meshMinLodLevelGpuBuffer.getResource(), nullptr, &desc, _meshLodLevelUav.get(0)._cpuHandle);
			device->createUnorderedAccessView(_meshMaxLodLevelGpuBuffer.getResource(), nullptr, &desc, _meshLodLevelUav.get(1)._cpuHandle);

			device->createUnorderedAccessView(_meshMinLodLevelGpuBuffer.getResource(), nullptr, &desc, _meshLodLevelCpuUav.get(0)._cpuHandle);
			device->createUnorderedAccessView(_meshMaxLodLevelGpuBuffer.getResource(), nullptr, &desc, _meshLodLevelCpuUav.get(1)._cpuHandle);

			desc._buffer._numElements = _meshInstanceLodLevelGpuBuffer.getU32ElementCount();
			device->createUnorderedAccessView(_meshInstanceLodLevelGpuBuffer.getResource(), nullptr, &desc, _meshInstanceLodLevelUav.get(0)._cpuHandle);
			device->createUnorderedAccessView(_meshInstanceScreenPersentageGpuBuffer.getResource(), nullptr, &desc, _meshInstanceLodLevelUav.get(1)._cpuHandle);

			desc._buffer._numElements = _materialScreenPersentageGpuBuffer.getU32ElementCount();
			device->createUnorderedAccessView(_materialScreenPersentageGpuBuffer.getResource(), nullptr, &desc, _materialScreenPersentageUav._cpuHandle);
			device->createUnorderedAccessView(_materialScreenPersentageGpuBuffer.getResource(), nullptr, &desc, _materialScreenPersentageCpuUav._cpuHandle);
		}
	}

	_chunkAllocator.allocate([this](ChunkAllocator::Allocation& allocation) {
		_meshLodMinLevels = allocation.allocateClearedObjects<u32>(MeshGeometryScene::MESH_GEOMETRY_CAPACITY);
		_meshLodMaxLevels = allocation.allocateClearedObjects<u32>(MeshGeometryScene::MESH_GEOMETRY_CAPACITY);
		_meshScreenPersentages = allocation.allocateObjects<u32>(MeshGeometryScene::MESH_GEOMETRY_CAPACITY);
		_materialScreenPersentages = allocation.allocateObjects<u32>(MaterialScene::MATERIAL_CAPACITY);
		_textureStreamingLevels = allocation.allocateObjects<u8>(TextureScene::TEXTURE_CAPACITY);
		_prevTextureStreamingLevels = allocation.allocateClearedObjects<u8>(TextureScene::TEXTURE_CAPACITY);
	});
	memset(_textureStreamingLevels, 0xff, TextureScene::TEXTURE_CAPACITY);
}

void LodStreamingManager::terminate() {
	_meshMinLodLevelGpuBuffer.terminate();
	_meshMinLodLevelReadbackBuffer.terminate();
	_meshMaxLodLevelGpuBuffer.terminate();
	_meshMaxLodLevelReadbackBuffer.terminate();
	_meshInstanceLodLevelGpuBuffer.terminate();
	_meshInstanceScreenPersentageGpuBuffer.terminate();
	_materialScreenPersentageGpuBuffer.terminate();
	_materialScreenPersentageReadbackBuffer.terminate();

	DescriptorAllocator* descriptorGpuAllocator = DescriptorAllocatorGroup::Get()->getSrvCbvUavGpuAllocator();
	descriptorGpuAllocator->free(_meshLodLevelSrv);
	descriptorGpuAllocator->free(_meshLodLevelUav);
	descriptorGpuAllocator->free(_meshInstanceLodLevelSrv);
	descriptorGpuAllocator->free(_meshInstanceLodLevelUav);
	descriptorGpuAllocator->free(_materialScreenPersentageUav);
	descriptorGpuAllocator->free(_materialScreenPersentageSrv);

	DescriptorAllocator* descriptorCpuAllocator = DescriptorAllocatorGroup::Get()->getSrvCbvUavCpuAllocator();
	descriptorCpuAllocator->free(_meshLodLevelCpuUav);
	descriptorCpuAllocator->free(_materialScreenPersentageCpuUav);
	_chunkAllocator.free();
}

void LodStreamingManager::update() {
	// 3 フレーム経過するまでは GPU で計算した LOD 情報が得られないためスキップ
	if (_frameCounter++ < rhi::BACK_BUFFER_COUNT) {
		return;
	}

	// メッシュ LOD Min 情報
	{
		const u32* mapPtr = _meshMinLodLevelReadbackBuffer.map<u32>();
		u32 copySizeInByte = sizeof(u32) * MeshGeometryScene::MESH_GEOMETRY_CAPACITY;
		memcpy(_meshLodMinLevels, mapPtr, copySizeInByte);
		_meshMinLodLevelReadbackBuffer.unmap();
	}

	// メッシュ LOD Max 情報
	{
		const u32* mapPtr = _meshMaxLodLevelReadbackBuffer.map<u32>();
		u32 copySizeInByte = sizeof(u32) * MeshGeometryScene::MESH_GEOMETRY_CAPACITY;
		memcpy(_meshLodMaxLevels, mapPtr, copySizeInByte);
		_meshMaxLodLevelReadbackBuffer.unmap();
	}

	// マテリアル LOD 情報
	{
		const u32* mapPtr = _materialScreenPersentageReadbackBuffer.map<u32>();
		u32 copySizeInByte = sizeof(u32) * MaterialScene::MATERIAL_CAPACITY;
		memcpy(_materialScreenPersentages, mapPtr, copySizeInByte);
		_materialScreenPersentageReadbackBuffer.unmap();
	}

	updateMeshStreaming();
	updateTextureStreaming();
}

void LodStreamingManager::copyReadbackBuffers(rhi::CommandList* commandList) {
	commandList->copyResource(_meshMinLodLevelReadbackBuffer.getResource(), _meshMinLodLevelGpuBuffer.getResource());
	commandList->copyResource(_meshMaxLodLevelReadbackBuffer.getResource(), _meshMaxLodLevelGpuBuffer.getResource());
	commandList->copyResource(_materialScreenPersentageReadbackBuffer.getResource(), _materialScreenPersentageGpuBuffer.getResource());
}

void LodStreamingManager::clearBuffers(rhi::CommandList* commandList) {
	// Mesh Lod Min
	{
		u32 clearValues[4] = { UINT32_MAX, UINT32_MAX ,UINT32_MAX ,UINT32_MAX };
		rhi::GpuDescriptorHandle gpuDescriptor = _meshLodLevelUav.get(0)._gpuHandle;
		rhi::CpuDescriptorHandle cpuDescriptor = _meshLodLevelCpuUav.get(0)._cpuHandle;
		rhi::Resource* resource = _meshMinLodLevelGpuBuffer.getResource();
		commandList->clearUnorderedAccessViewUint(gpuDescriptor, cpuDescriptor, resource, clearValues, 0, nullptr);
	}

	// Mesh Lod Max
	{
		u32 clearValues[4] = {};
		rhi::GpuDescriptorHandle gpuDescriptor = _meshLodLevelUav.get(1)._gpuHandle;
		rhi::CpuDescriptorHandle cpuDescriptor = _meshLodLevelCpuUav.get(1)._cpuHandle;
		rhi::Resource* resource = _meshMaxLodLevelGpuBuffer.getResource();
		commandList->clearUnorderedAccessViewUint(gpuDescriptor, cpuDescriptor, resource, clearValues, 0, nullptr);
	}

	// Material Screen Persentage
	{
		u32 clearValues[4] = { UINT32_MAX, UINT32_MAX ,UINT32_MAX ,UINT32_MAX };
		rhi::GpuDescriptorHandle gpuDescriptor = _materialScreenPersentageUav._gpuHandle;
		rhi::CpuDescriptorHandle cpuDescriptor = _materialScreenPersentageCpuUav._cpuHandle;
		rhi::Resource* resource = _materialScreenPersentageGpuBuffer.getResource();
		commandList->clearUnorderedAccessViewUint(gpuDescriptor, cpuDescriptor, resource, clearValues, 0, nullptr);
	}
}

LodStreamingManager* LodStreamingManager::Get() {
	return &g_meshLodStreamingManager;
}

void LodStreamingManager::updateMeshStreaming() {
	// ストリーミングロード、アンロードされた時にログを表示するか？
#define OUTPUT_STREAM_LOG 1
#if OUTPUT_STREAM_LOG
	constexpr char OUTPUT_LOG_FORMAT[] = "%-20s %-30s %d -> %d";
	bool updated = false;
#endif

	MeshGeometryScene* meshScene = MeshGeometryScene::Get();
	const MeshGeometryPool* meshPool = meshScene->getMeshGeometryPool();
	GeometryResourceManager* geometryResourceManager = GeometryResourceManager::Get();
	const GeometryResourceManager::MeshLodStreamRange* meshLodStreamRanges = geometryResourceManager->getMeshLodStreamRanges();
	for (u32 i = 0; i < MeshGeometryScene::MESH_GEOMETRY_CAPACITY; ++i) {
		const GeometryResourceManager::MeshLodStreamRange& lodRange = meshLodStreamRanges[i];
		u16 minLodLevel = u16(_meshLodMinLevels[i]);
		if (minLodLevel == UINT16_MAX) {
			continue;
		}

		const MeshGeometry* mesh = meshPool->getMeshGeometry(i);
		u16 maxLodLevel = u16(_meshLodMaxLevels[i]);
		if (lodRange._beginLevel == UINT16_MAX) {
#if OUTPUT_STREAM_LOG
			updated = true;
			LTN_INFO(OUTPUT_LOG_FORMAT, "Init Added.", mesh->getAssetPath(), minLodLevel, maxLodLevel + 1);
#endif
			geometryResourceManager->loadLodMesh(mesh, minLodLevel, maxLodLevel + 1);
			geometryResourceManager->updateMeshLodStreamRange(i, minLodLevel, maxLodLevel);
			continue;
		}

		bool minLodAdded = minLodLevel < lodRange._beginLevel;
		if (minLodAdded) {
#if OUTPUT_STREAM_LOG
			updated = true;
			LTN_INFO(OUTPUT_LOG_FORMAT, "MinLod Added.", mesh->getAssetPath(), minLodLevel, lodRange._beginLevel);
#endif
			geometryResourceManager->loadLodMesh(mesh, minLodLevel, lodRange._beginLevel);
			geometryResourceManager->updateMeshLodStreamRange(i, minLodLevel, lodRange._endLevel);
		}

		bool maxLodAdded = maxLodLevel > lodRange._endLevel;
		if (maxLodAdded) {
#if OUTPUT_STREAM_LOG
			LTN_INFO(OUTPUT_LOG_FORMAT, "MaxLod Added.", mesh->getAssetPath(), lodRange._endLevel + 1, maxLodLevel + 1);
			updated = true;
#endif
			geometryResourceManager->loadLodMesh(mesh, lodRange._endLevel + 1, maxLodLevel + 1);
			geometryResourceManager->updateMeshLodStreamRange(i, lodRange._beginLevel, maxLodLevel);
		}

		bool minLodRemoved = minLodLevel > lodRange._beginLevel;
		if (minLodRemoved) {
#if OUTPUT_STREAM_LOG
			LTN_INFO(OUTPUT_LOG_FORMAT, "MinLod Removed.", mesh->getAssetPath(), lodRange._beginLevel, minLodLevel);
			updated = true;
#endif
			geometryResourceManager->unloadLodMesh(mesh, lodRange._beginLevel, minLodLevel);
			geometryResourceManager->updateMeshLodStreamRange(i, minLodLevel, lodRange._endLevel);
		}

		bool maxLodRemoved = maxLodLevel < lodRange._endLevel;
		if (maxLodRemoved) {
#if OUTPUT_STREAM_LOG
			LTN_INFO(OUTPUT_LOG_FORMAT, "MaxLod Removed.", mesh->getAssetPath(), maxLodLevel + 1, lodRange._endLevel + 1);
			updated = true;
#endif
			geometryResourceManager->unloadLodMesh(mesh, maxLodLevel + 1, lodRange._endLevel + 1);
			geometryResourceManager->updateMeshLodStreamRange(i, lodRange._beginLevel, maxLodLevel);
		}
	}

#if OUTPUT_STREAM_LOG
	if (updated) {
		LTN_INFO("-----------------------------------------------------------------------");
	}
#endif

	// ストリーミング状況をリスト形式でデバッグ表示
	ImGui::Begin("MeshStreamingStatus");
	for (u32 i = 0; i < MeshGeometryScene::MESH_GEOMETRY_CAPACITY; ++i) {
		const GeometryResourceManager::MeshLodStreamRange& lodRange = meshLodStreamRanges[i];
		u16 minLodLevel = u16(_meshLodMinLevels[i]);
		if (minLodLevel == UINT16_MAX) {
			continue;
		}

		const MeshGeometry* mesh = meshPool->getMeshGeometry(i);
		ImGui::Text("%-30s [%d, %d] / %d", mesh->getAssetPath(), lodRange._beginLevel, lodRange._endLevel, mesh->getLodMeshCount());
	}
	ImGui::End();
}

void LodStreamingManager::updateTextureStreaming() {
	const u8* materialEnabledFlags = MaterialScene::Get()->getEnabledFlags();
	for (u32 i = 0; i < MaterialScene::MATERIAL_CAPACITY; ++i) {
		if (materialEnabledFlags[i] == 0) {
			continue;
		}

		streamTexture(i);
	}

	TextureScene* textureScene = TextureScene::Get();
	GpuTextureManager* gpuTextureManager = GpuTextureManager::Get();
	const u8* textureEnableFlags = textureScene->getTextureEnabledFlags();
#if 0
	for (u32 i = 0; i < TextureScene::TEXTURE_CAPACITY; ++i) {
		if (textureEnableFlags[i] == 0) {
			continue;
		}

		if (_prevTextureStreamingLevels[i] == _textureStreamingLevels[i]) {
			continue;
		}

		if (_textureStreamingLevels[i] == UINT8_MAX) {
			continue;
		}

		const Texture* texture = textureScene->getTexture(i);
		gpuTextureManager->streamTexture(texture, _prevTextureStreamingLevels[i], _textureStreamingLevels[i]);
		_prevTextureStreamingLevels[i] = _textureStreamingLevels[i];
	}
#endif

	ImGui::Begin("TextureStreamingStatus");
	for (u32 i = 0; i < TextureScene::TEXTURE_CAPACITY; ++i) {
		if (textureEnableFlags[i] == 0) {
			continue;
		}
		const Texture* texture = textureScene->getTexture(i);
		ImGui::Text("%-30s %d / %d", texture->getAssetPath(), _textureStreamingLevels[i], texture->getDdsHeader()->_mipMapCount);
	}
	ImGui::End();
}

void LodStreamingManager::streamTexture(u32 materialIndex) {
	if (_materialScreenPersentages[materialIndex] >= UINT16_MAX) {
		return;
	}

	TextureScene* textureScene = TextureScene::Get();
	Application* app = ApplicationSysytem::Get()->getApplication();
	f32 screenPersentage = _materialScreenPersentages[materialIndex] / f32(UINT16_MAX);

	const Material* material = MaterialScene::Get()->getMaterial(materialIndex);
	u16 textureOffsets[16];
	u16 textureCount = material->getPipelineSet()->findMaterialParameters(Shader::PARAMETER_TYPE_TEXTURE, textureOffsets);
	LTN_ASSERT(textureCount < LTN_COUNTOF(textureOffsets));
	for (u32 i = 0; i < textureCount; ++i) {
		u32 textureIndex = *material->getParameter<u32>(textureOffsets[i]);
		const Texture* texture = textureScene->getTexture(textureIndex);
		u32 screenWidth = app->getScreenWidth();
		u64 sourceWidth = texture->getDdsHeader()->_width;
		u32 targetWidth = u32(screenPersentage * screenWidth);
		u8 requestMipLevel = 0;
		for (u32 i = 1; i < targetWidth; i *= 2) {
			requestMipLevel++;
		}

		u8 currentStreamMipLevel = _textureStreamingLevels[i];
		u8 targetStreamMipLevel = Min(_textureStreamingLevels[i], requestMipLevel);
		_textureStreamingLevels[textureIndex] = requestMipLevel;
	}
}
}
