#pragma once
#include <Core/Type.h>
#include <Core/VirturalArray.h>
#include <Core/ChunkAllocator.h>
namespace ltn {
class Mesh;
class MeshInstance;
class Material;
class MeshPreset {
public:
	void setMesh(const Mesh* mesh) { _mesh = mesh; }
	void setMaterials(Material** materials) { _materials = materials; }
	const Mesh* getMesh() const { return _mesh; }

	MeshInstance* createMeshInstance(u32 instanceCount) const;

private:
	const Mesh* _mesh = nullptr;
	Material** _materials = nullptr;
};

class MeshPresetScene {
public:
	static constexpr u32 MESH_PRESET_CAPACITY = 256;

	struct MeshPresetCreatationDesc {
		const char* _assetPath = nullptr;
	};

	void initialize();
	void terminate();

	const MeshPreset* createMeshPreset(const MeshPresetCreatationDesc& desc);
	void destroyMeshPreset(const MeshPreset* meshPreset);

	u32 getMeshPresetIndex(const MeshPreset* meshPreset) const { return u32(meshPreset - _meshPresets); }
	const MeshPreset* findMeshPreset(u64 assetPathHash) const;

	static MeshPresetScene* Get();

private:
	u64* _assetPathHashes = nullptr;
	MeshPreset* _meshPresets = nullptr;
	Material** _materials = nullptr;
	VirtualArray _meshPresetAllocations;
	VirtualArray _materialAllocations;
	VirtualArray::AllocationInfo* _meshPresetAllocationInfos = nullptr;
	VirtualArray::AllocationInfo* _materialAllocationInfos = nullptr;
	ChunkAllocator _chunkAllocator;
};
}