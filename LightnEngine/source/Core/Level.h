#pragma once
#include <Core/Type.h>
#include <Core/ChunkAllocator.h>
namespace ltn {
class MeshGeometry;
class Mesh;
class MeshInstance;
class Texture;
class Material;
class Level {
public:
	struct LevelHeader {
		u32 _meshGeometryCount = 0;
		u32 _meshCount = 0;
		u32 _textureCount = 0;
		u32 _shaderSetCount = 0;
		u32 _materialCount = 0;
		u32 _meshInstanceCount = 0;
	};

	void initialize(const char* levelFilePath);
	void terminate();

private:
	LevelHeader _levelHeader;
	MeshGeometry const** _meshGeometries = nullptr;
	Mesh const** _meshes = nullptr;
	Texture const** _textures = nullptr;
	Material const** _materials = nullptr;

	u32 _meshCount = 0;
	u32* _meshInstanceCounts = nullptr;
	MeshInstance** _meshInstances = nullptr;
	ChunkAllocator _chunkAllocator;
};
}