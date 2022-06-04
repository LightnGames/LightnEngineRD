#pragma once
#include <Core/Type.h>
#include <Core/VirturalArray.h>
namespace ltn {
class Mesh;
class StaticMesh {
public:
	struct MeshCreatationDesc {
		const char* _assetPath = nullptr;
	};

	void create(const MeshCreatationDesc& desc);
	void destroy();

	Mesh* getMesh() { return _mesh; }

private:
	Mesh* _mesh = nullptr;
};
}