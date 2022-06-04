#include "StaticMesh.h"
#include <Core/Memory.h>
#include <Core/Utility.h>
#include <RendererScene/Mesh.h>

namespace ltn {
void StaticMesh::create(const MeshCreatationDesc& desc) {
	u64 materialSlotNameHashes[16];
	u64 materialPathHashes[16];
	u64 meshPathHash = 0;
	u32 materialSlotCount = 0;

	{
		AssetPath meshAsset(desc._assetPath);
		meshAsset.openFile();
		meshAsset.readFile(&meshPathHash, sizeof(u64));
		meshAsset.readFile(&materialSlotCount, sizeof(u32));
		meshAsset.readFile(&materialSlotNameHashes, sizeof(u64) * materialSlotCount);
		meshAsset.readFile(&materialPathHashes, sizeof(u64) * materialSlotCount);
		meshAsset.closeFile();
	}

	_mesh = MeshScene::Get()->findMesh(meshPathHash);
}

void StaticMesh::destroy() {
	MeshScene::Get()->destroyMesh(_mesh);
}
}
