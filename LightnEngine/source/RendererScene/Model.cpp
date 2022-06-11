#include "Model.h"
#include <Core/Memory.h>
#include <Core/Utility.h>
#include <RendererScene/Mesh.h>
#include <RendererScene/MeshInstance.h>
#include <RendererScene/Material.h>

namespace ltn {
void Model::create(const MeshCreatationDesc& desc) {
	u64 materialSlotNameHashes[Mesh::MATERIAL_SLOT_COUNT_MAX];
	u64 materialInstancePathHashes[Mesh::MATERIAL_SLOT_COUNT_MAX];
	u64 meshPathHash = 0;
	u32 materialSlotCount = 0;

	{
		AssetPath meshAsset(desc._assetPath);
		meshAsset.openFile();
		meshAsset.readFile(&meshPathHash, sizeof(u64));
		meshAsset.readFile(&materialSlotCount, sizeof(u32));
		meshAsset.readFile(&materialSlotNameHashes, sizeof(u64) * materialSlotCount);
		meshAsset.readFile(&materialInstancePathHashes, sizeof(u64) * materialSlotCount);
		meshAsset.closeFile();
	}
	
	LTN_ASSERT(materialSlotCount < Mesh::MATERIAL_SLOT_COUNT_MAX);

	_mesh = MeshScene::Get()->findMesh(meshPathHash);
	MaterialScene* materialScene = MaterialScene::Get();
	for (u32 i = 0; i < materialSlotCount; ++i) {
		u16 materialSlotIndex = _mesh->findMaterialSlotIndex(materialSlotNameHashes[i]);
		LTN_ASSERT(materialSlotIndex != Mesh::INVALID_MATERIAL_SLOT_INDEX);
		_materialInstances[materialSlotIndex] = materialScene->findMaterialInstance(materialInstancePathHashes[i]);
	}
}

void Model::destroy() {
}

MeshInstance* Model::createMeshInstances(u32 instanceCount) {
	MeshInstanceScene::MeshInstanceCreatationDesc desc;
	desc._mesh = _mesh;
	MeshInstance* meshInstance = MeshInstanceScene::Get()->createMeshInstance(desc);

	u16 materialSlotCount = _mesh->getMaterialSlotCount();
	for (u16 i = 0; i < materialSlotCount; ++i) {
		meshInstance->setMaterialInstance(i, _materialInstances[i]);
	}

	return meshInstance;
}
}
