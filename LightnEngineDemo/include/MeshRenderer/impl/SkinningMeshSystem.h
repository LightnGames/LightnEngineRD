#pragma once

#include <Core/System.h>
#include <GfxCore/impl/GpuResourceImpl.h>
#include <GfxCore/impl/DescriptorHeap.h>

class SkinningMeshSystem {
public:
	void initialize();
	void terminate();

private:
	Matrix4 _boneInstances;
};