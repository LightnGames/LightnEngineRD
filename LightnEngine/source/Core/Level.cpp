#include "Level.h"
#include <Core/Utility.h>
#include <Core/Memory.h>
#include <Core/CpuTimerManager.h>
#include <RendererScene/MeshGeometry.h>
#include <RendererScene/Material.h>
#include <RendererScene/Mesh.h>
#include <RendererScene/Texture.h>
#include <RendererScene/MeshInstance.h>
#include <RendererScene/SkySphere.h>

namespace ltn {
void Level::initialize(const char* levelFilePath) {
	CpuScopedPerf scopedPerf("Level");
	AssetPath assetPath(levelFilePath);
	assetPath.openFile();

	constexpr u32 BIN_HEADER_LENGTH = 4;
	char meshHeader[BIN_HEADER_LENGTH + 1];
	meshHeader[BIN_HEADER_LENGTH] = '\0';

	// ヘッダー
	{
		assetPath.readFile(meshHeader, BIN_HEADER_LENGTH);
		assetPath.readFile(&_levelHeader, sizeof(LevelHeader));
		LTN_ASSERT(strcmp(meshHeader, "RESH") == 0);
	}

	_chunkAllocator.allocate([this](ChunkAllocator::Allocation& allocation) {
		_meshGeometries = allocation.allocateObjects<const MeshGeometry*>(_levelHeader._meshGeometryCount);
		_meshes = allocation.allocateObjects<const Mesh*>(_levelHeader._meshCount);
		_textures = allocation.allocateObjects<const Texture*>(_levelHeader._textureCount);
		_materials = allocation.allocateObjects<const Material*>(_levelHeader._materialCount);
		});

	// メッシュジオメトリ
	{
		assetPath.readFile(meshHeader, BIN_HEADER_LENGTH);
		LTN_ASSERT(strcmp(meshHeader, "MSHG") == 0);

		u32 meshGeometryCount = _levelHeader._meshGeometryCount;
		u32 filePathTotalCounts = 0;
		u32* filePathCounts = Memory::allocObjects<u32>(meshGeometryCount);
		assetPath.readFile(filePathCounts, sizeof(u32) * meshGeometryCount);
		for (u32 i = 0; i < meshGeometryCount; ++i) {
			filePathTotalCounts += filePathCounts[i];
		}

		MeshGeometryScene::CreatationDesc* descs = Memory::allocObjects<MeshGeometryScene::CreatationDesc>(meshGeometryCount);
		char* filePathChunks = Memory::allocObjects<char>(filePathTotalCounts);
		char* filePathReadPtr = filePathChunks;
		assetPath.readFile(filePathChunks, filePathTotalCounts);
		for (u32 i = 0; i < meshGeometryCount; ++i) {
			descs[i]._assetPath = filePathReadPtr;
			filePathReadPtr += filePathCounts[i];
		}

		MeshGeometryScene::Get()->createMeshGeometries(descs, _meshGeometries, meshGeometryCount);

		Memory::deallocObjects(filePathCounts);
		Memory::deallocObjects(descs);
		Memory::deallocObjects(filePathChunks);
	}

	// テクスチャ
	{
		assetPath.readFile(meshHeader, BIN_HEADER_LENGTH);
		LTN_ASSERT(strcmp(meshHeader, "TEX ") == 0);

		u32 textureCount = _levelHeader._textureCount;
		u32 filePathTotalCounts = 0;
		u32* filePathCounts = Memory::allocObjects<u32>(textureCount);
		assetPath.readFile(filePathCounts, sizeof(u32) * textureCount);
		for (u32 i = 0; i < textureCount; ++i) {
			filePathTotalCounts += filePathCounts[i];
		}

		TextureScene::TextureCreatationDesc* descs = Memory::allocObjects<TextureScene::TextureCreatationDesc>(textureCount);
		char* filePathChunks = Memory::allocObjects<char>(filePathTotalCounts);
		char* filePathReadPtr = filePathChunks;
		assetPath.readFile(filePathChunks, filePathTotalCounts);
		for (u32 i = 0; i < textureCount; ++i) {
			descs[i]._assetPath = filePathReadPtr;
			filePathReadPtr += filePathCounts[i];
		}

		TextureScene::Get()->createTextures(descs, _textures, textureCount);

		Memory::deallocObjects(filePathCounts);
		Memory::deallocObjects(descs);
		Memory::deallocObjects(filePathChunks);
	}

	// マテリアル
	{
		assetPath.readFile(meshHeader, BIN_HEADER_LENGTH);
		LTN_ASSERT(strcmp(meshHeader, "MAT ") == 0);

		u32 materialCount = _levelHeader._materialCount;
		u32 filePathTotalCounts = 0;
		u32* filePathCounts = Memory::allocObjects<u32>(materialCount);
		assetPath.readFile(filePathCounts, sizeof(u32) * materialCount);
		for (u32 i = 0; i < materialCount; ++i) {
			filePathTotalCounts += filePathCounts[i];
		}

		MaterialScene::MaterialCreatationDesc* descs = Memory::allocObjects<MaterialScene::MaterialCreatationDesc>(materialCount);
		char* filePathChunks = Memory::allocObjects<char>(filePathTotalCounts);
		char* filePathReadPtr = filePathChunks;
		assetPath.readFile(filePathChunks, filePathTotalCounts);
		for (u32 i = 0; i < materialCount; ++i) {
			descs[i]._assetPath = filePathReadPtr;
			filePathReadPtr += filePathCounts[i];
		}

		MaterialScene::Get()->createMaterials(descs, _materials, materialCount);

		Memory::deallocObjects(filePathCounts);
		Memory::deallocObjects(descs);
		Memory::deallocObjects(filePathChunks);
	}

	// メッシュ
	{
		assetPath.readFile(meshHeader, BIN_HEADER_LENGTH);
		LTN_ASSERT(strcmp(meshHeader, "MESH") == 0);

		u32 meshCount = _levelHeader._meshCount;
		u32 filePathTotalCounts = 0;
		u32* filePathCounts = Memory::allocObjects<u32>(meshCount);
		assetPath.readFile(filePathCounts, sizeof(u32)* meshCount);
		for (u32 i = 0; i < meshCount; ++i) {
			filePathTotalCounts += filePathCounts[i];
		}

		MeshScene::CreatationDesc* descs = Memory::allocObjects<MeshScene::CreatationDesc>(meshCount);
		char* filePathChunks = Memory::allocObjects<char>(filePathTotalCounts);
		char* filePathReadPtr = filePathChunks;
		assetPath.readFile(filePathChunks, filePathTotalCounts);
		for (u32 i = 0; i < meshCount; ++i) {
			descs[i]._assetPath = filePathReadPtr;
			filePathReadPtr += filePathCounts[i];
		}

		MeshScene::Get()->createMeshes(descs, _meshes, meshCount);

		Memory::deallocObjects(filePathCounts);
		Memory::deallocObjects(descs);
		Memory::deallocObjects(filePathChunks);
	}

	// メッシュインスタンス
	{
		assetPath.readFile(meshHeader, BIN_HEADER_LENGTH);
		LTN_ASSERT(strcmp(meshHeader, "MESI") == 0);

		u32 meshInstanceCount = _levelHeader._meshGeometryCount;
		u32 meshCount = 0;
		assetPath.readFile(&meshCount, sizeof(u32));

		_meshCount = meshCount;
		_meshInstances = Memory::allocObjects<MeshInstance*>(meshCount);
		_meshInstanceCounts = Memory::allocObjects<u32>(meshCount);

		for (u32 i = 0; i < meshCount; ++i) {
			u64 meshPathHash = 0;
			u32 instanceCount = 0;
			assetPath.readFile(&meshPathHash, sizeof(u64));
			assetPath.readFile(&instanceCount, sizeof(u32));

			const Mesh* mesh = MeshScene::Get()->findMesh(meshPathHash);
			LTN_ASSERT(mesh != nullptr);
			MeshInstance* meshInstances = mesh->createMeshInstances(instanceCount);
			for (u32 meshInstanceIndex = 0; meshInstanceIndex < instanceCount; ++meshInstanceIndex) {
				Float4 wv[3];
				assetPath.readFile(&wv, sizeof(Float4) * 3);

				Matrix4 worldMatrix = Matrix4(
					wv[0].x, wv[1].x, wv[2].x, 0.0f,
					wv[0].y, wv[1].y, wv[2].y, 0.0f,
					wv[0].z, wv[1].z, wv[2].z, 0.0f,
					wv[0].w, wv[1].w, wv[2].w, 1.0f);

				meshInstances[meshInstanceIndex].setWorldMatrix(worldMatrix);
			}

			_meshInstances[i] = meshInstances;
			_meshInstanceCounts[i] = instanceCount;
		}
	}

	assetPath.closeFile();

	SkySphereScene::CreatationDesc desc;
	desc._environmentTexturePath = "GohicHorror//Texture//CubeMap//EnvEnvHDR_CubeMap.dds";
	desc._diffuseTexturePath = "GohicHorror//Texture//CubeMap//EnvDiffuseHDR_CubeMap.dds";
	desc._supecularTexturePath = "GohicHorror//Texture//CubeMap//EnvSpecularHDR_CubeMap.dds";
	_skySphere = SkySphereScene::Get()->createSkySphere(desc);
}

void Level::terminate() {
	for (u32 i = 0; i < _meshCount; ++i) {
		MeshInstanceScene::Get()->destroyMeshInstances(_meshInstances[i]);
	}

	MeshGeometryScene::Get()->destroyMeshGeometries(_meshGeometries, _levelHeader._meshGeometryCount);
	TextureScene::Get()->destroyTextures(_textures, _levelHeader._textureCount);
	MaterialScene::Get()->destroyMaterials(_materials, _levelHeader._materialCount);
	MeshScene::Get()->destroyMeshes(_meshes, _levelHeader._meshCount);
	SkySphereScene::Get()->destroySkySphere(_skySphere);

	Memory::deallocObjects(_meshInstanceCounts);
	Memory::deallocObjects(_meshInstances);

	_chunkAllocator.free();
}
}
