#pragma once
#define DEBUG_WINDOW_ENABLE 1

#include <Core/Type.h>
#include <Core/Utility.h>
#include <fstream>
#include <direct.h>

namespace ltn {
constexpr char DEBUG_TEMP_STRUCTURE_FOLDER_PATH[] = "\\__Intermediate\\DebugWindow\\";
constexpr char DEBUG_TEMP_STRUCTURE_EXT[] = ".dws";
template <typename T>
struct DebugSerializedStructure :public T {
#if DEBUG_WINDOW_ENABLE
	DebugSerializedStructure(const char* fileName) :_fileName(fileName) {
		char envPath[FILE_PATH_COUNT_MAX];
		GetEnvPath(envPath);

		char dirPath[FILE_PATH_COUNT_MAX];
		sprintf_s(dirPath, "%s%s", envPath, DEBUG_TEMP_STRUCTURE_FOLDER_PATH);
		sprintf_s(_fullPath, "%s%s%s", dirPath, fileName, DEBUG_TEMP_STRUCTURE_EXT);

		_mkdir(dirPath);

		std::ifstream fin;
		fin.open(_fullPath, std::ios::in | std::ios::binary);

		// ファイルサイズ取得
		fin.seekg(0, std::ios_base::end);
		s32 fileSize = static_cast<s32>(fin.tellg());
		fin.seekg(0, std::ios_base::beg);

		if (fin.fail()) {
			T initData = T();
			memcpy(reinterpret_cast<T*>(this), &initData, sizeof(T));
		}
		else {
			fin.read(reinterpret_cast<char*>(this), fileSize);
			fin.close();
		}

		// 読み込み時のデータを保持しておく
		_loadData = static_cast<T>(*this);
	}

	~DebugSerializedStructure() {
		// 読み込み時のデータと変化がなければデータの書き込みを行わない
		if (!memcmp(&_loadData, static_cast<const T*>(this), sizeof(T))) {
			return;
		}

		std::ofstream fout;
		fout.open(_fullPath, std::ios::out | std::ios::binary | std::ios::trunc);

		LTN_ASSERT(!fout.fail());
		fout.write(reinterpret_cast<char*>(this), sizeof(T));
		fout.close();
	}

	T _loadData;
	const char* _fileName = nullptr;
	char _fullPath[256];
#else
	DebugWindowStructure(const char* fileName) {}
#endif
};
}