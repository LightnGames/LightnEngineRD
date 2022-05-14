#pragma once
#include <Core/VirturalArray.h>
#include <Core/ModuleSettings.h>
namespace ltn {
class SubMesh {
public:
};

class LodMesh {
public:
};
class Mesh {
public:
	VirtualArray::AllocationInfo _meshAllocationInfo;
	VirtualArray::AllocationInfo _lodMeshAllocationInfo;
	VirtualArray::AllocationInfo _subMeshAllocationInfo;
	const SubMesh* _subMeshes = nullptr;
	const LodMesh* _lodMeshes = nullptr;
	u32 _lodMeshCount = 0;
	u32 _subMeshCount = 0;
};

class LTN_API MeshContainer {
public:
	struct InitializetionDesc {
		u32 _meshCount = 0;
		u32 _lodMeshCount = 0;
		u32 _subMeshCount = 0;
	};

	struct MeshAllocationDesc {
		u32 _lodMeshCount = 0;
		u32 _subMeshCount = 0;
	};

	void initialize(const InitializetionDesc& desc);
	void terminate();

	Mesh* allocateMesh(const MeshAllocationDesc& desc);
	void freeMeshObjects(Mesh* mesh);

	Mesh* getMesh(u32 index) { return &_meshes[index]; }

private:
	VirtualArray _meshAllocationInfo;
	VirtualArray _lodMeshAllocationInfo;
	VirtualArray _subMeshAllocationInfo;
	Mesh* _meshes = nullptr;
	LodMesh* _lodMeshes = nullptr;
	SubMesh* _subMeshes = nullptr;
};

class LTN_API MeshObserver {
public:
	Mesh** getCreatedMeshes() { return _createdMeshes; }
	u32 getCreatedMeshCount() { return _createdMeshCount; }

	void pushCreatedMesh(Mesh* mesh){
		_createdMeshes[_createdMeshCount++] = mesh;
	}

	void reset(){
		_createdMeshCount = 0;
	}

private:
	static constexpr u32 STACK_COUNT_MAX = 256;
	Mesh* _createdMeshes[STACK_COUNT_MAX];
	u32 _createdMeshCount = 0;
};

class LTN_API MeshScene {
public:
	struct MeshCreatationDesc {
		const char* _assetPath = nullptr;
	};

	void initialize();
	void terminate();

	Mesh* createMesh(const MeshCreatationDesc& desc);
	void destroyMesh(Mesh* mesh);

	static MeshScene* Get();
private:
	MeshContainer _meshContainer;
	MeshObserver _meshObserver;
};
}