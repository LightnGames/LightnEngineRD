#pragma once
#include <Core/Type.h>
#include <Core/Memory.h>
#include <functional>
namespace ltn {
class ChunkAllocator {
public:
	struct Allocation {
		template<class T>
		T* allocateObjects(u32 count) {
			if (_state == 0) {
				_size += sizeof(T) * count;
				return nullptr;
			}

			T* result = reinterpret_cast<T*>(_ptr + _offset);
			_offset += sizeof(T) * count;
			return result;
		}

		template<class T>
		T* allocateClearedObjects(u32 count) {
			if (_state == 0) {
				allocateObjects<T>(count);
				return nullptr;
			}

			T* ptr = allocateObjects<T>(count);
			for (u32 i = 0; i < count; ++i) {
				new (ptr + i) T();
			}
			return ptr;
		}

		u8* _ptr = nullptr;
		u32 _size = 0;
		u32 _offset = 0;
		u8 _state = 0;
	};

	using AllocateFunc = std::function<void(Allocation&)>;

	void alloc(AllocateFunc func) {
		Allocation al;

		// 確保サイズを計算
		func(al);

		// メモリをまとめて確保
		_ptr = Memory::allocObjects<u8>(al._size);
		
		// 確保メモリをポインタに割り当て
		al._ptr = _ptr;
		al._state = 1;
		func(al);
	};

	void freeChunk() {
		Memory::deallocObjects(_ptr);
	}

	u8* _ptr = nullptr;
};
}