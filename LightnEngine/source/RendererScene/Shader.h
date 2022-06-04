#pragma once
#include <Core/Type.h>
#include <Core/VirturalArray.h>
#include "RenderSceneUtility.h"
namespace ltn {
class Shader {
public:
	const char* _assetPath = nullptr;
};

class ShaderScene {
public:
	static constexpr u32 SHADER_COUNT_MAX = 128;
	
	void initialize();
	void terminate();
	void lateUpdate();

	struct CreatationDesc {
		const char* _assetPath = nullptr;
	};

	Shader* createShader(const CreatationDesc& desc);
	void destroyShader(Shader* shader);

	const UpdateInfos<Shader>* getCreateInfos() const { return &_shaderCreateInfos; }
	const UpdateInfos<Shader>* getDestroyInfos() const { return &_shaderDestroyInfos; }

	u32 getShaderIndex(const Shader* shader) const { return u32(shader - _shaders); }
	Shader* findShader(u64 assetPathHash);

	static ShaderScene* Get();

private:
	Shader* _shaders = nullptr;
	u64* _shaderAssetPathHashes = nullptr;
	VirtualArray _shaderAllocations;
	VirtualArray::AllocationInfo* _shaderAllocationInfos = nullptr;
	char** _shaderAssetPaths = nullptr;
	UpdateInfos<Shader> _shaderCreateInfos;
	UpdateInfos<Shader> _shaderDestroyInfos;
};
}