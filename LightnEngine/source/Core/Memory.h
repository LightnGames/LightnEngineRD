#pragma once
#include <Core/Type.h>

#define LTN_DEBUG_TRACE_MEMORY 0
#if LTN_DEBUG_RACE_MEMORY
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
// Replace _NORMAL_BLOCK with _CLIENT_BLOCK if you want the
// allocations to be of _CLIENT_BLOCK type
#else
#define DBG_NEW new
#endif

namespace ltn {
class Memory {
public:
	template<class T>
	static T* allocObjects(u32 count){
		T* ptr = DBG_NEW T[count];
		return ptr;
	}

	template<class T>
	static void deallocObjects(T* objects){
		delete[] objects;
	}
};
}