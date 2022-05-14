#pragma once
#include <Core/Type.h>

namespace ltn {
class Memory {
public:
	template<class T>
	static T* allocObjects(u32 count){
		return new T[count];
	}

	template<class T>
	static void freeObjects(T* objects){
		delete[] objects;
	}
};
}