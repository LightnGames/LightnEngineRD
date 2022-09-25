#pragma once
#include <Core/Type.h>
#include <Core/VirturalArray.h>
#include <Core/ChunkAllocator.h>
#include "RenderSceneUtility.h"
namespace ltn {
class Texture;
class SkySphere {
public:
	void setEnvironmentTextuire(const Texture* texture) { _environmentTexture = texture; }
	void setDiffuseTextuire(const Texture* texture) { _diffuseTexture = texture; }
	void setSupecularTextuire(const Texture* texture) { _supecularTexture = texture; }

	const Texture* getEnvironmentTexture() const { return _environmentTexture; }
	const Texture* getDiffuseTexture() const { return _diffuseTexture; }
	const Texture* getSupecularTexture() const { return _supecularTexture; }

private:
	const Texture* _environmentTexture = nullptr;
	const Texture* _diffuseTexture = nullptr;
	const Texture* _supecularTexture = nullptr;
};

class SkySphereScene {
public:
	static constexpr u32 SKY_SPHERE_CAPACITY = 4;

	void initialize();
	void terminate();
	void lateUpdate();

	struct CreatationDesc {
		const char* _environmentTexturePath = nullptr;
		const char* _diffuseTexturePath = nullptr;
		const char* _supecularTexturePath = nullptr;
	};

	const SkySphere* createSkySphere(const CreatationDesc& desc);
	void destroySkySphere(const SkySphere* skySphere);

	const UpdateInfos<SkySphere>* getCreateInfos() const { return &_skySphereCreateInfos; }
	const UpdateInfos<SkySphere>* getDestroyInfos() const { return &_skySphereDestroyInfos; }

	u32 getSkySphereIndex(const SkySphere* skySphere) const { return u32(skySphere - _skySpheres); }

	static SkySphereScene* Get();

private:
	SkySphere _skySpheres[SKY_SPHERE_CAPACITY] = {};
	VirtualArray _skySphereAllocations;
	VirtualArray::AllocationInfo* _skySphereAllocationInfos = nullptr;

	UpdateInfos<SkySphere> _skySphereCreateInfos;
	UpdateInfos<SkySphere> _skySphereDestroyInfos;
	ChunkAllocator _chunkAllocator;
};
}