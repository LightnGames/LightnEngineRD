#pragma once
#include <Core/VirturalArray.h>
#include <Renderer/RHI/Rhi.h>

namespace ltn {
struct DescriptorHandle {
	DescriptorHandle operator + (u64 offset) const {
		DescriptorHandle handle = *this;
		handle += offset;
		return handle;
	}

	DescriptorHandle& operator += (u64 offset) {
		_cpuHandle = _cpuHandle + offset;
		_gpuHandle = _gpuHandle + offset;
		return *this;
	}

	rhi::CpuDescriptorHandle _cpuHandle;
	rhi::GpuDescriptorHandle _gpuHandle;
	VirtualArray::AllocationInfo _allocationInfo;
};

struct DescriptorHandles {
	DescriptorHandle get(u32 index) const {
		return _firstHandle + index * _incrementSize;
	}

	u32 _descriptorCount = 0;
	u32 _incrementSize = 0;
	DescriptorHandle _firstHandle;
};

class DescriptorAllocator {
public:
	void initialize(const rhi::DescriptorHeapDesc& desc);
	void terminate();

	DescriptorHandle allocate();
	DescriptorHandles allocate(u32 count);
	void free(DescriptorHandle& descriptorHandle);
	void free(DescriptorHandles& descriptorHandles);
	void setName(const char* name);

	rhi::DescriptorHeap* getDescriptorHeap() { return &_descriptorHeap; }
	u32 getIncrementSize() const { return _incrementSize; }

private:
	rhi::DescriptorHeap _descriptorHeap;
	rhi::CpuDescriptorHandle _cpuHandleStart;
	rhi::GpuDescriptorHandle _gpuHandleStart;
	VirtualArray _allocationInfo;
	u32 _incrementSize = 0;
};

class DescriptorAllocatorGroup {
public:
	struct Desc {
		rhi::Device* _device = nullptr;
		u32 _srvCbvUavCpuCount = 0;
		u32 _srvCbvUavGpuCount = 0;
		u32 _rtvCount = 0;
		u32 _dsvCount = 0;
	};

	void initialize(const Desc& desc);
	void terminate();

	DescriptorAllocator* getSrvCbvUavCpuAllocator() { return &_srvCbvUavCpuAllocator; }
	DescriptorAllocator* getSrvCbvUavGpuAllocator() { return &_srvCbvUavGpuAllocator; }
	DescriptorAllocator* getRtvAllocator() { return &_rtvAllocator; }
	DescriptorAllocator* getDsvAllocator() { return &_dsvAllocator; }

	static DescriptorAllocatorGroup* Get();

private:
	DescriptorAllocator _srvCbvUavCpuAllocator;
	DescriptorAllocator _srvCbvUavGpuAllocator;
	DescriptorAllocator _rtvAllocator;
	DescriptorAllocator _dsvAllocator;
};
}