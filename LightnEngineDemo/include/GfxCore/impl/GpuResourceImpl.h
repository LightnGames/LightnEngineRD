#pragma once
#include <Core/System.h>
#include <GfxCore/GfxModuleSettings.h>
#include <GfxCore/GpuResource.h>
#include <GfxCore/impl/GraphicsApiInterface.h>

struct GpuBufferDesc {
	Device* _device = nullptr;
	ResourceStates _initialState;
	u32 _sizeInByte = 0;
	ResourceFlags _flags = RESOURCE_FLAG_NONE;
	HeapType _heapType = HEAP_TYPE_DEFAULT;
};

struct GpuTextureDesc {
	Device* _device = nullptr;
	ResourceStates _initialState;
	ClearValue* _optimizedClearValue = nullptr;
	Format _format = FORMAT_UNKNOWN;
	u64 _width = 0;
	u32 _height = 0;
	u16 _arraySize = 1;
	u16 _mipLevels = 1;
	u32 _sampleCount = 1;
	u32 _sampleQuality = 0;
	ResourceFlags _flags = RESOURCE_FLAG_NONE;
};

struct GpuDynamicBufferDesc {
	Device* _device = nullptr;
	u32 _sizeInByte = 0;
};

class LTN_GFX_CORE_API GpuResource {
public:
	void initialize();
	void terminate();
	void transitionResource(CommandList* commandList, ResourceStates stateAfter);
	void setDebugName(const char* name);
	ResourceTransitionBarrier getTransitionBarrier(ResourceStates stateAfter);
	ResourceTransitionBarrier getAndUpdateTransitionBarrier(ResourceStates stateAfter);
	Resource* getResource() { return _resource; }
	u32 getSizeInByte() const { return _sizeInByte; }
	ResourceStates getCurrentResourceState() const { return _currentState; }
	ConstantBufferViewDesc getConstantBufferViewDesc() const;
	u64 getGpuVirtualAddress() const { return _gpuVirtualAddress; }
	ResourceDesc getResourceDesc() const { return _desc; }

protected:
	Resource* _resource = nullptr;
	u32 _sizeInByte = 0;
	u64 _gpuVirtualAddress = 0;
	ResourceStates _currentState;
	ResourceDesc _desc;
};

class LTN_GFX_CORE_API GpuBuffer :public GpuResource {
public:
	void initialize(const GpuBufferDesc& desc);
	void initialize(const GpuDynamicBufferDesc& desc);
	void terminate();

	template<class T>
	T* map(const MemoryRange* range = nullptr) {
		return reinterpret_cast<T*>(_resource->map(range));
	}
};

class LTN_GFX_CORE_API GpuTexture :public GpuResource {
public:
	void initializeFromBackbuffer(SwapChain* swapChain, u32 backBufferIndex);
	void initialize(const GpuTextureDesc& desc);
};

class LTN_GFX_CORE_API Shader {
public:
	void initialize(const char* filePath);
	void terminate();
	
	ShaderByteCode getShaderByteCode() const;

private:
	ShaderBlob* _blob = nullptr;
};