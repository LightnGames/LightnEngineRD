#pragma once
#include <Core/Math.h>
#include <Core/VirturalArray.h>
#include <Core/ChunkAllocator.h>
#include "RenderSceneUtility.h"

namespace ltn {
class DirectionalLight {
public:
	void setIntensity(f32 intensity) { _intensity = intensity; }
	void setDirection(Vector3 direction) { _direction = direction; }
	void setColor(Color color) { _color = color; }

	f32 getIntensity() const { return _intensity; }
	Vector3 getDirection() const { return _direction; }
	Color getColor() const { return _color; }

private:
	f32 _intensity = 1.0f;
	Color _color = Color::White();
	Vector3 _direction = Vector3::Up();
};

class LightScene {
public:
	static constexpr u32 DIRECTIONAL_LIGHT_CAPACITY = 4;

	void initialize();
	void terminate();
	void lateUpdate();

	DirectionalLight* createDirectionalLight();
	void destroyDirectionalLight(const DirectionalLight* light);

	u32 getDirectionalLightIndex(const DirectionalLight* light) { return u32(light - _directionalLights); }

	void postUpdateDirectionalLight(const DirectionalLight* light) { _directionalLightUpdateInfos.push(light); }

	const UpdateInfos<DirectionalLight>* getUpdateInfos() const { return &_directionalLightUpdateInfos; }
	const UpdateInfos<DirectionalLight>* getDestroyInfos() const { return &_directionalLightDestroyInfos; }

	static LightScene* Get();

private:
	VirtualArray _directionalLightAllocations;
	VirtualArray::AllocationInfo _directionalLightAllocationInfos[DIRECTIONAL_LIGHT_CAPACITY] = {};

	DirectionalLight _directionalLights[DIRECTIONAL_LIGHT_CAPACITY] = {};

	UpdateInfos<DirectionalLight> _directionalLightUpdateInfos;
	UpdateInfos<DirectionalLight> _directionalLightDestroyInfos;
};
}