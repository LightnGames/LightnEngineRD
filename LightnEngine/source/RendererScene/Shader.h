#pragma once
#include <Core/Type.h>
#include <Core/VirturalArray.h>
#include "RenderSceneUtility.h"
namespace ltn {
class Shader {
public:
	enum ParameterType {
		PARAMETER_TYPE_FLOAT,
		PARAMETER_TYPE_FLOAT2,
		PARAMETER_TYPE_FLOAT3,
		PARAMETER_TYPE_FLOAT4,
		PARAMETER_TYPE_UINT,
		PARAMETER_TYPE_TEXTURE,
		PARAMETER_TYPE_COUNT
	};

	bool findParameter(u32 nameHash, u16& outOffsetSizeInByte) const;

	const char* _assetPath = nullptr;
	u64 _assetPathHash = 0;
	u16 _parameterCount = 0;
	u16 _parameterSizeInByte = 0;
	const u32* _parameterNameHashes = nullptr;
	const u16* _parameterOffsets = nullptr;
};

class ShaderScene {
public:
	static constexpr u32 SHADER_CAPACITY = 128;
	static constexpr u32 SHADER_PARAMETER_CAPACITY = 1024;
	
	void initialize();
	void terminate();
	void lateUpdate();

	struct CreatationDesc {
		const char* _assetPath = nullptr;

		// TODO: ShaderInfo から適切にパラメーター情報を読めるようになったら消す
		bool _TEST_collectParameter = false;
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
	VirtualArray _parameterAllocations;
	VirtualArray::AllocationInfo* _shaderAllocationInfos = nullptr;
	VirtualArray::AllocationInfo* _parameterAllocationInfos = nullptr;
	u32* _parameterNameHashes = nullptr;
	u16* _parameterOffsets = nullptr;

	char** _shaderAssetPaths = nullptr;
	UpdateInfos<Shader> _shaderCreateInfos;
	UpdateInfos<Shader> _shaderDestroyInfos;
};
}