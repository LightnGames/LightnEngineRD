#pragma once
#include <MaterialSystem/ModuleSettings.h>
#include <Core/System.h>
#include <Core/Asset.h>

class Texture;

struct LTN_MATERIAL_SYSTEM_API Material {
	virtual void requestToDelete() = 0;
	virtual void setTexture(Texture* texture, u64 parameterNameHash) = 0;

protected:
	u8* _stateFlags = nullptr;
	u8* _updateFlags = nullptr;
};

struct LTN_MATERIAL_SYSTEM_API ShaderSet {
	virtual void requestToDelete() = 0;
	virtual void setTexture(Texture* texture, u64 parameterNameHash) = 0;

protected:
	u8* _stateFlags = nullptr;
	u8* _updateFlags = nullptr;
};

struct MaterialDesc {
	const char* _filePath = nullptr;
};

struct ShaderSetDesc {
	const char* _filePath = nullptr;
};

class LTN_MATERIAL_SYSTEM_API MaterialSystem {
public:
	virtual ShaderSet* createShaderSet(const ShaderSetDesc& desc) = 0;
	virtual Material* createMaterial(const MaterialDesc& desc) = 0;
	virtual Material* findMaterial(u64 filePathHash) = 0;

	static MaterialSystem* Get();
};