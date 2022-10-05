#include "LodStreamingManager.h"
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

	// GPU バッファ
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

	// デスクリプタ
	{
		DescriptorAllocatorGroup* descriptorAllocator = DescriptorAllocatorGroup::Get();
		_meshLodLevelSrv = descriptorAllocator->allocateSrvCbvUavGpu(2);
		_meshLodLevelUav = descriptorAllocator->allocateSrvCbvUavGpu(2);
		_meshInstanceLodLevelSrv = descriptorAllocator->allocateSrvCbvUavGpu(2);
		_meshInstanceLodLevelUav = descriptorAllocator->allocateSrvCbvUavGpu(2);
		_materialScreenPersentageUav = descriptorAllocator->allocateSrvCbvUavGpu();
		_materialScreenPersentageSrv = descriptorAllocator->allocateSrvCbvUavGpu();
		_meshLodLevelCpuUav = descriptorAllocator->allocateSrvCbvUavCpu(2);
		_materialScreenPersentageCpuUav = descriptorAllocator->allocateSrvCbvUavCpu();

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

	_chunkAllocator.alloc([this](ChunkAllocator::Allocation& allocation) {
		_meshLodMinLevels = allocation.allocateClearedObjects<u32>(MeshGeometryScene::MESH_GEOMETRY_CAPACITY);
		_meshLodMaxLevels = allocation.allocateClearedObjects<u32>(MeshGeometryScene::MESH_GEOMETRY_CAPACITY);
		_meshScreenPersentages = allocation.allocateObjects<u32>(MeshGeometryScene::MESH_GEOMETRY_CAPACITY);
		_materialScreenPersentages = allocation.allocateObjects<u32>(MaterialScene::MATERIAL_CAPACITY);
		_textureStreamingLevels = allocation.allocateObjects<u8>(TextureScene::TEXTURE_CAPACITY);
		_prevTextureStreamingLevels = allocation.allocateClearedObjects<u8>(TextureScene::TEXTURE_CAPACITY);
		});
	memset(_textureStreamingLevels, INVALID_TEXTURE_STREAMING_LEVEL, TextureScene::TEXTURE_CAPACITY);
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

	DescriptorAllocatorGroup* descriptorAllocator = DescriptorAllocatorGroup::Get();
	descriptorAllocator->deallocSrvCbvUavGpu(_meshLodLevelSrv);
	descriptorAllocator->deallocSrvCbvUavGpu(_meshLodLevelUav);
	descriptorAllocator->deallocSrvCbvUavGpu(_meshInstanceLodLevelSrv);
	descriptorAllocator->deallocSrvCbvUavGpu(_meshInstanceLodLevelUav);
	descriptorAllocator->deallocSrvCbvUavGpu(_materialScreenPersentageUav);
	descriptorAllocator->deallocSrvCbvUavGpu(_materialScreenPersentageSrv);
	descriptorAllocator->deallocSrvCbvUavCpu(_meshLodLevelCpuUav);
	descriptorAllocator->deallocSrvCbvUavCpu(_materialScreenPersentageCpuUav);
	_chunkAllocator.freeChunk();
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

	// マテリアルストリーミングステータス
	{
		constexpr char IMGUI_TEXT_FORMAT[] = "[%-3d] %-60s %-1.4f";
		ImGui::Begin("MaterialStatus");

		const u8* materialEnabledFlags = MaterialScene::Get()->getEnabledFlags();
		for (u32 i = 0; i < MaterialScene::MATERIAL_CAPACITY; ++i) {
			if (materialEnabledFlags[i] == 0) {
				continue;
			}

			const Material* material = MaterialScene::Get()->getMaterial(i);
			f32 screenPersentage = _materialScreenPersentages[i] / f32(UINT16_MAX);
			if (_materialScreenPersentages[i] == 0) {
				ImGui::TextDisabled(IMGUI_TEXT_FORMAT, i, material->getAssetPath(), 0.0f);
			} else {
				ImGui::Text(IMGUI_TEXT_FORMAT, i, material->getAssetPath(), screenPersentage);
			}
		}
		ImGui::End();
	}
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
		u32 clearValues[4] = {};
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
#define OUTPUT_STREAM_LOG 0
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
		if (minLodLevel == INVALID_MESH_STREAMING_LEVEL) {
			continue;
		}

		const MeshGeometry* mesh = meshPool->getMeshGeometry(i);
		u16 maxLodLevel = u16(_meshLodMaxLevels[i]);

		// 初回ロード時
		if (lodRange._beginLevel == INVALID_MESH_STREAMING_LEVEL) {
#if OUTPUT_STREAM_LOG
			updated = true;
			LTN_INFO(OUTPUT_LOG_FORMAT, "Init Added.", mesh->getAssetPath(), minLodLevel, maxLodLevel + 1);
#endif
			geometryResourceManager->loadLodMesh(mesh, minLodLevel, maxLodLevel + 1);
			geometryResourceManager->updateMeshLodStreamRange(i, minLodLevel, maxLodLevel);
			continue;
	}

		// 詳細 LOD Level 追加時
		bool minLodAdded = minLodLevel < lodRange._beginLevel;
		if (minLodAdded) {
#if OUTPUT_STREAM_LOG
			updated = true;
			LTN_INFO(OUTPUT_LOG_FORMAT, "MinLod Added.", mesh->getAssetPath(), minLodLevel, lodRange._beginLevel);
#endif
			geometryResourceManager->loadLodMesh(mesh, minLodLevel, lodRange._beginLevel);
			geometryResourceManager->updateMeshLodStreamRange(i, minLodLevel, lodRange._endLevel);
}

		// 粗い LOD Level 追加時
		bool maxLodAdded = maxLodLevel > lodRange._endLevel;
		if (maxLodAdded) {
#if OUTPUT_STREAM_LOG
			LTN_INFO(OUTPUT_LOG_FORMAT, "MaxLod Added.", mesh->getAssetPath(), lodRange._endLevel + 1, maxLodLevel + 1);
			updated = true;
#endif
			geometryResourceManager->loadLodMesh(mesh, lodRange._endLevel + 1, maxLodLevel + 1);
			geometryResourceManager->updateMeshLodStreamRange(i, lodRange._beginLevel, maxLodLevel);
}

		// 詳細 LOD Level 削除時
		bool minLodRemoved = minLodLevel > lodRange._beginLevel;
		if (minLodRemoved) {
#if OUTPUT_STREAM_LOG
			LTN_INFO(OUTPUT_LOG_FORMAT, "MinLod Removed.", mesh->getAssetPath(), lodRange._beginLevel, minLodLevel);
			updated = true;
#endif
			geometryResourceManager->unloadLodMesh(mesh, lodRange._beginLevel, minLodLevel);
			geometryResourceManager->updateMeshLodStreamRange(i, minLodLevel, lodRange._endLevel);
		}

		// 粗い LOD Level 削除時
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
	constexpr char IMGUI_TEXT_FORMAT[] = "%-65s [%2d,%2d] / %d";
	const u8* meshEnabledFlags = meshPool->getEnabledFlags();
	ImGui::Begin("MeshStreamingStatus");
	ImGui::Text("%-65s [range] / lodCount", "");
	for (u32 i = 0; i < MeshGeometryScene::MESH_GEOMETRY_CAPACITY; ++i) {
		if (meshEnabledFlags[i] == 0) {
			continue;
		}

		const GeometryResourceManager::MeshLodStreamRange& lodRange = meshLodStreamRanges[i];
		s16 beginLodLevel = s16(lodRange._beginLevel);
		s16 endLodLevel = s16(lodRange._endLevel);

		const MeshGeometry* mesh = meshPool->getMeshGeometry(i);
		bool meshInvalid = lodRange._beginLevel == INVALID_MESH_STREAMING_LEVEL || lodRange._endLevel == INVALID_MESH_STREAMING_LEVEL;
		if (meshInvalid) {
			ImGui::TextDisabled(IMGUI_TEXT_FORMAT, mesh->getAssetPath(), -1, -1, mesh->getLodMeshCount());
		}
		else {
			ImGui::Text(IMGUI_TEXT_FORMAT, mesh->getAssetPath(), beginLodLevel, endLodLevel, mesh->getLodMeshCount());
		}
	}
	ImGui::End();
}

void LodStreamingManager::updateTextureStreaming() {
	const u8* materialEnabledFlags = MaterialScene::Get()->getEnabledFlags();
	for (u32 i = 0; i < MaterialScene::MATERIAL_CAPACITY; ++i) {
		if (materialEnabledFlags[i] == 0) {
			continue;
		}

		computeTextureStreamingMipLevels(i);
	}

	TextureScene* textureScene = TextureScene::Get();
	GpuTextureManager* gpuTextureManager = GpuTextureManager::Get();
	const u8* textureEnableFlags = textureScene->getTextureEnabledFlags();
#define OUTPUT_STREAM_LOG 0
#if OUTPUT_STREAM_LOG
	bool updated = false;
#endif
	for (u32 i = 0; i < TextureScene::TEXTURE_CAPACITY; ++i) {
		if (textureEnableFlags[i] == 0) {
			continue;
		}

		if (_prevTextureStreamingLevels[i] == _textureStreamingLevels[i]) {
			continue;
		}

		if (_textureStreamingLevels[i] == INVALID_TEXTURE_STREAMING_LEVEL) {
			continue;
		}

		const Texture* texture = textureScene->getTexture(i);
		u32 width = u32(gpuTextureManager->getTexture(i)->getResourceDesc()._width);
#if OUTPUT_STREAM_LOG
		updated = true;
		LTN_INFO("Texture streaming %-30s %u -> %u　(%4d x %4d)", texture->getAssetPath(), _prevTextureStreamingLevels[i], _textureStreamingLevels[i], width, width);
#endif
		gpuTextureManager->streamTexture(texture, _prevTextureStreamingLevels[i], _textureStreamingLevels[i]);
		gpuTextureManager->requestCreateSrv(i);
		_prevTextureStreamingLevels[i] = _textureStreamingLevels[i];

	}

#if OUTPUT_STREAM_LOG
	if (updated) {
		LTN_INFO("-----------------------------------------------------------------------");
	}
#endif

	constexpr char IMGUI_TEXT_FORMAT[] = "[%-3d] %-65s %2d / %2d (%4d x %4d)";
	ImGui::Begin("TextureStreamingStatus");
	for (u32 i = 0; i < TextureScene::TEXTURE_CAPACITY; ++i) {
		if (textureEnableFlags[i] == 0) {
			continue;
		}

		const Texture* texture = textureScene->getTexture(i);
		u32 width = u32(gpuTextureManager->getTexture(i)->getResourceDesc()._width);
		u32 mipCount = texture->getDdsHeader()->_mipMapCount;
		bool textureInvalid = _textureStreamingLevels[i] == INVALID_TEXTURE_STREAMING_LEVEL;
		if (textureInvalid) {
			ImGui::TextDisabled(IMGUI_TEXT_FORMAT, i, texture->getAssetPath(), -1, mipCount, width, width);
		}
		else {
			ImGui::Text(IMGUI_TEXT_FORMAT, i, texture->getAssetPath(), _textureStreamingLevels[i], mipCount, width, width);
		}
	}
	ImGui::End();
}

void LodStreamingManager::computeTextureStreamingMipLevels(u32 materialIndex) {
	if (_materialScreenPersentages[materialIndex] == 0) {
		return;
	}

	TextureScene* textureScene = TextureScene::Get();
	Application* app = ApplicationSysytem::Get()->getApplication();
	f32 screenPersentage = Min(_materialScreenPersentages[materialIndex] / f32(UINT16_MAX), 1.0f);

	// FIXME: 毎回マテリアルが参照しているテクスチャを検索するのは重いのでキャッシュするなりする
	const Material* material = MaterialScene::Get()->getMaterial(materialIndex);
	u16 textureOffsets[16];
	u16 textureCount = material->getPipelineSet()->findMaterialParameters(Shader::PARAMETER_TYPE_TEXTURE, textureOffsets);

	// Material が参照しているテクスチャのストリーミングレベルを更新します
	LTN_ASSERT(textureCount < LTN_COUNTOF(textureOffsets));
	for (u32 i = 0; i < textureCount; ++i) {
		u32 textureIndex = *material->getParameter<u32>(textureOffsets[i]);
		const Texture* texture = textureScene->getTexture(textureIndex);
		u32 screenWidth = app->getScreenWidth();
		u64 sourceWidth = texture->getDdsHeader()->_width;
		u32 targetWidth = u32(screenPersentage * screenWidth);

		constexpr u8 MIP_STREAMING_OFFSET = 0;
		u8 requestMipLevel = 1 + MIP_STREAMING_OFFSET;
		for (u32 i = 1; i < targetWidth; i *= 2) {
			requestMipLevel++;
		}

		// 小さすぎるテクスチャを作成するとリソース作成できない場合があるため最小ミップレベルを指定
		constexpr u8 MIN_STREAMING_MIP_LEVEL = 4;
		_textureStreamingLevels[textureIndex] = Max(requestMipLevel, MIN_STREAMING_MIP_LEVEL);
	}
}
}
