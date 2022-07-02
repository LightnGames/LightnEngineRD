#pragma once
#include <Core/Type.h>

#define TARGET_WIN64 1
#define ASSET_SHIPPING_ROOT 0

#if ASSET_SHIPPING_ROOT
#else
#include <cstdlib>
#endif

#if TARGET_WIN64
#include <Windows.h>
#include <stdio.h>
#define DEBUG_PRINT(x) OutputDebugStringA(x)
#endif

#ifdef LTN_DEBUG
#include <cassert>
#include <type_traits>
#include <wchar.h>
#define LTN_INFO( str, ... )			 \
      {									 \
        char c[256];				 \
        sprintf_s(c, str, __VA_ARGS__ ); \
        DEBUG_PRINT(c);			 \
        DEBUG_PRINT("\n");		 \
      }

#define LTN_STATIC_ASSERT(x, s) static_assert(x, s)
#define LTN_ASSERT(x) \
if (!(x)) { \
		LTN_INFO("Assertion failed! in %s (%d)", reinterpret_cast<const char*>(__FILE__), __LINE__); \
		DebugBreak(); \
}


#define LTN_SUCCEEDED(hr) LTN_ASSERT(SUCCEEDED(hr))
#else
#define LTN_STATIC_ASSERT(x, s)
#define LTN_ASSERT(x) 
#define LTN_INFO( str, ... )
#define LTN_SUCCEEDED(hr) hr
#endif

#define LTN_COUNTOF(x) _countof(x)
#define LTN_OFFSETOF(x, y) offsetof(x, y)

namespace ltn {
using FilePtr = FILE*;
constexpr u32 FILE_PATH_COUNT_MAX = 128;
u32 StrLength(const char* str);
u64 StrHash(const char* str);
u32 StrHash32(const char* str);
u64 StrHash64(const char* str);
u64 BinHash64(const void* bin, u32 length);

constexpr u32 GetAligned(u32 size, u32 alignSize) {
	return size + (alignSize - (size % alignSize));
}

template <class T>
const T& Max(const T& a, const T& b) {
	return a < b ? b : a;
}

template <class T>
const T& Min(const T& a, const T& b) {
	return a > b ? b : a;
}

template <class T>
T RoundDivUp(const T& value, const T& divideValue) {
	return (value / divideValue) + 1;
}

template <class T>
u32 SizeOfU32(u32 size) { return sizeof(T) * size; }

static void GetEnvPath(char* envPath) {
	constexpr char ENV_NAME[] = "LTN_ROOT";
	size_t size;
	errno_t error = getenv_s(&size, nullptr, 0, ENV_NAME);
	LTN_ASSERT(size > 0);

	getenv_s(&size, envPath, size, ENV_NAME);
}

class AssetPath {
public:
	explicit AssetPath(const char* localPath) {
		char envPath[FILE_PATH_COUNT_MAX];
		GetEnvPath(envPath);
		sprintf_s(_path, FILE_PATH_COUNT_MAX, "%s\\Resource\\%s", envPath, localPath);
	}

	const char* get() {
		return _path;
	}

	void openFile(){
		errno_t error = fopen_s(&_filePtr, _path, "rb");
		LTN_ASSERT(error == 0);
	}

	void seekSet(u32 offset){
		fseek(_filePtr, offset, SEEK_SET);
	}

	void seekCur(u32 offset) {
		fseek(_filePtr, offset, SEEK_CUR);
	}

	void closeFile(){
		fclose(_filePtr);
	}

	void readFile(void* ptr, u32 readSize){
		fread_s(ptr, readSize, readSize, 1, _filePtr);
	}

	FilePtr getFilePtr() { return _filePtr; }

private:
	char _path[FILE_PATH_COUNT_MAX];
	FilePtr _filePtr = nullptr;
};

class ThreeDigiets {
public:
	ThreeDigiets(u64 value) {
		char temp[64];
		sprintf_s(temp, "%lld", value);

		u32 count = 0;
		for (u32 i = 0; i < LTN_COUNTOF(temp); ++i) {
			if (temp[i] == '\0') {
				count = i;
				break;
			}
		}

		u32 currentStrIndex = 0;
		for (u32 i = 0; i < count; ++i) {
			_str[currentStrIndex++] = temp[i];
			u32 strIndex = count - i - 1;
			bool isFirst = strIndex == 0;
			if (strIndex % 3 == 0 && !isFirst) {
				_str[currentStrIndex++] = ',';
			}
		}

		_str[currentStrIndex] = '\0';
	}

	const char* get() const { return _str; }

	char _str[64];
};
}