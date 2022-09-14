#pragma once
#include <Core/Type.h>
#include <Core/Memory.h>
#include <Core/Utility.h>

namespace ltn {
template<class T>
class LinerAllocater {
public:
	void initialize(u32 numElements) {
		_dataPtr = Memory::allocObjects<T>(numElements);
		_numElements = numElements;
	}

	void terminate() {
		Memory::deallocObjects(_dataPtr);
		_dataPtr = nullptr;
		_numElements = 0;
		_allocatedNumElements = 0;
	}

	T* allocate(u32 numElements = 1) {
		LTN_ASSERT(_allocatedNumElements + numElements < _numElements);
		u32 currentSizeInByte = _allocatedNumElements;
		_allocatedNumElements += numElements;
		return _dataPtr + currentSizeInByte;
	}

	T* get(u32 index = 0) {
		return &_dataPtr[index];
	}

	u32 getCount() const {
		return _allocatedNumElements;
	}

	void reset() {
		_allocatedNumElements = 0;
	}

	T* _dataPtr = nullptr;
	u32 _numElements = 0;
	u32 _allocatedNumElements = 0;
};
}