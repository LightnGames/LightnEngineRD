#include "VirturalArray.h"
#include <Core/Utility.h>
#include <ThiredParty/D3D12MemoryAllocator/D3D12MemAlloc.h>

namespace ltn {
void VirtualArray::initialize(const Desc& desc) {
	D3D12MA::VIRTUAL_BLOCK_DESC blockDesc = {};
	blockDesc.Size = desc._size;
	LTN_SUCCEEDED(CreateVirtualBlock(&blockDesc, &_virtualBlock));
}
void VirtualArray::terminate() {
	_virtualBlock->Release();
}
VirtualArray::AllocationInfo VirtualArray::allocation(u32 size) {
	D3D12MA::VIRTUAL_ALLOCATION_DESC allocDesc = {};
	allocDesc.Size = size;

	D3D12MA::VirtualAllocation alloc;
	UINT64 allocOffset;
	LTN_SUCCEEDED(_virtualBlock->Allocate(&allocDesc, &alloc, &allocOffset));

	AllocationInfo info = {};
	info._handle = alloc.AllocHandle;
	info._offset = allocOffset;
	return info;
}
void VirtualArray::freeAllocation(AllocationInfo allocationInfo) {
	D3D12MA::VirtualAllocation alloc = {};
	alloc.AllocHandle = static_cast<D3D12MA::AllocHandle>(allocationInfo._handle);
	_virtualBlock->FreeAllocation(alloc);
}
}
