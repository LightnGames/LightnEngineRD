#pragma once
#include <Core/Math.h>
#include <Core/VirturalArray.h>
#include "RenderSceneUtility.h"
namespace ltn {
class Texture;
class SkySphere {
public:
	void setEnvironmentTextuire(const Texture* texture) { _environmentTexture = texture; }
	void setDiffuseTextuire(const Texture* texture) { _diffuseTexture = texture; }
	void setSupecularTextuire(const Texture* texture) { _supecularTexture = texture; }
	void setEnvironmentColor(Color color) { _environmentColor = color; }
	void setDiffuseScale(f32 scale) { _diffuseScale = scale; }
	void setSpecularScale(f32 scale) { _specularScale = scale; }

	const Texture* getEnvironmentTexture() const { return _environmentTexture; }
	const Texture* getDiffuseTexture() const { return _diffuseTexture; }
	const Texture* getSpecularTexture() const { return _supecularTexture; }
	Color getEnvironmentColor() const { return _environmentColor; }
	f32 getDiffuseScale() const { return _diffuseScale; }
	f32 getSpecularScale() const { return _specularScale; }

private:
	Color _environmentColor = Color::White();
	f32 _diffuseScale = 1.0f;
	f32 _specularScale = 1.0f;
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

	SkySphere* createSkySphere(const CreatationDesc& desc);
	void destroySkySphere(const SkySphere* skySphere);

	void postUpdateSkySphere(const SkySphere* skySphere) { _skySphereUpdateInfos.push(skySphere); }

	const UpdateInfos<SkySphere>* getUpdateInfos() const { return &_skySphereUpdateInfos; }
	const UpdateInfos<SkySphere>* getDestroyInfos() const { return &_skySphereDestroyInfos; }

	u32 getSkySphereIndex(const SkySphere* skySphere) const { return u32(skySphere - _skySpheres); }
	SkySphere* getSkySphere(u32 index) { return &_skySpheres[index]; }
	const Texture* getBrdfLutTexture() const { return _brdfLutTexture; }

	static SkySphereScene* Get();

private:
	SkySphere _skySpheres[SKY_SPHERE_CAPACITY] = {};
	VirtualArray _skySphereAllocations;
	VirtualArray::AllocationInfo _skySphereAllocationInfos[SKY_SPHERE_CAPACITY] = {};

	UpdateInfos<SkySphere> _skySphereUpdateInfos;
	UpdateInfos<SkySphere> _skySphereDestroyInfos;

	const Texture* _brdfLutTexture;
};
}