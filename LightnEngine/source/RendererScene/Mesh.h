#pragma once
#include <Core/Type.h>
#include <Core/VirturalArray.h>
#include <Core/ChunkAllocator.h>
namespace ltn {
class MeshGeometry;
class MeshInstance;
class Material;
class Mesh {
public:
	void setMeshGeometry(const MeshGeometry* meshGeometry) { _meshGeometry = meshGeometry; }
	void setMaterials(Material** materials) { _materials = materials; }
	const MeshGeometry* getMeshGeometry() const { return _meshGeometry; }

	MeshInstance* createMeshInstances(u32 instanceCount) const;

private:
	const MeshGeometry* _meshGeometry = nullptr;
	Material** _materials = nullptr;
};

class MeshScene {
public:
	static constexpr u32 MESH_CAPACITY = 256;

	struct CreatationDesc {
		const char* _assetPath = nullptr;
	};

	void initialize();
	void terminate();

	const Mesh* createMesh(const CreatationDesc& desc);
	void createMeshes(const CreatationDesc* descs, Mesh const** meshes, u32 instanceCount);
	void destroyMesh(const Mesh* mesh);
	void destroyMeshes(Mesh const** meshes, u32 instanceCount);

	u32 getMeshIndex(const Mesh* mesh) const { return u32(mesh - _meshes); }
	const Mesh* findMesh(u64 assetPathHash) const;

	static MeshScene* Get();

private:
	u64* _assetPathHashes = nullptr;
	Mesh* _meshes = nullptr;
	Material** _materials = nullptr;
	VirtualArray _meshAllocations;
	VirtualArray _materialAllocations;
	VirtualArray::AllocationInfo* _meshAllocationInfos = nullptr;
	VirtualArray::AllocationInfo* _materialAllocationInfos = nullptr;
	ChunkAllocator _chunkAllocator;
};
}