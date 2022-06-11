#pragma once
#include <Core/Type.h>
#include <Core/Memory.h>
#include <functional>
namespace ltn {
struct Alloc {
	u32 _size = 0;
	u8 _state = 0;
};
class ChunkAllocator {
public:
	using AllocateFunc = std::function<void(Alloc&)>;

	void alloc(AllocateFunc func) {
		Alloc al;

		// 確保サイズを計算
		func(al);

		// メモリをまとめて確保
		_ptr = Memory::allocObjects<u8>(al._size);
		
		// 確保メモリをポインタに割り当て
		al._state = 1;
		func(al);
	};

	void free() {
		Memory::freeObjects(_ptr);
	}

	u8* _ptr = nullptr;
};
}