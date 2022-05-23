#pragma once
#include <Core/Type.h>
namespace ltn {
class Material {
public:
};

class MaterialInstance {
public:
};

class MaterialScene {
public:
	static constexpr u32 MATERIAL_COUNT_MAX = 128;
	static constexpr u32 MATERIAL_INSTANCE_COUNT_MAX = 1024;

	void initialize();
	void terminate();
	void lateUpdate();

	static MaterialScene* Get();
private:
	Material* _materials = nullptr;
	MaterialInstance* _materialInstances = nullptr;
};
}