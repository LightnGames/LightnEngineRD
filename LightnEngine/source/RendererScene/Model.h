#pragma once
#include <Core/Type.h>
#include <Core/VirturalArray.h>
namespace ltn {
class Mesh;
class MaterialInstance;
class MeshInstance;
class Model {
public:
	struct MeshCreatationDesc {
		const char* _assetPath = nullptr;
	};

	void create(const MeshCreatationDesc& desc);
	void destroy();

	MeshInstance* createMeshInstances(u32 instanceCount);

	Mesh* getMesh() { return _mesh; }

private:
	static constexpr u32 MATERIAL_SLOT_COUNT_MAX = 16;
	Mesh* _mesh = nullptr;
	MaterialInstance* _materialInstances[MATERIAL_SLOT_COUNT_MAX];
};
}