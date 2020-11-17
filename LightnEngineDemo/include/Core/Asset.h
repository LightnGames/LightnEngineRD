#pragma once
#include <Core/System.h>
#include <cstdio>

enum AssetStateFlags {
	ASSET_STATE_NONE = 0,
	ASSET_STATE_ALLOCATED,
	ASSET_STATE_REQUEST_LOAD,
	ASSET_STATE_LOADING,
	ASSET_STATE_ENABLE,
	ASSET_STATE_UNLOADING
};

class Asset {
public:
	void requestLoad() {
		LTN_ASSERT(*_assetStateFlags == ASSET_STATE_ALLOCATED);
		*_assetStateFlags = ASSET_STATE_REQUEST_LOAD; 
	}
	void setAssetStateFlags(u8* flags) { _assetStateFlags = flags; }
	void setFilePath(const char* filePath) {
		fopen_s(&_file, filePath, "rb");
		LTN_ASSERT(_file != nullptr);
	}

	void freeFile() {
		fclose(_file);
		_file = nullptr;
	}

	FILE* getFile() { return _file; }

protected:
	FILE* _file = nullptr;
	u8* _assetStateFlags = nullptr;
};