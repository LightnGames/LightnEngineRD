#pragma once
#include <Core/Type.h>
#include <Core/ModuleSettings.h>

namespace D3D12MA {
	class VirtualBlock;
}

namespace ltn {
// D3D12MA ÇÃ VirtualAllocator ÇÃÉâÉbÉpÅ[
class LTN_API VirtualArray {
public:
	struct Desc {
		u64 _size = 0;
	};

	struct AllocationInfo {
		u64 _handle = 0;
		u64 _offset = 0;
		u64 _count = 0;
	};

	void initialize(const Desc& desc);
	void terminate();

	AllocationInfo allocation(u32 size);
	void freeAllocation(AllocationInfo allocationInfo);

	u32 getAllocateCount() const { return _allocateCount; }

private:
	D3D12MA::VirtualBlock* _virtualBlock = nullptr;
	u32 _allocateCount = 0;
};
}