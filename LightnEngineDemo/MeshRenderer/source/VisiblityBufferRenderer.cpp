#include <MeshRenderer/impl/VisiblityBufferRenderer.h>
#include <GfxCore/impl/ViewSystemImpl.h>
#include <GfxCore/impl/GraphicsSystemImpl.h>

void VisiblityBufferRenderer::initialize() {
	GraphicsSystemImpl* graphicsSystem = GraphicsSystemImpl::Get();
	Device* device = graphicsSystem->getDevice();
	ViewInfo* viewInfo = ViewSystemImpl::Get()->getView();

	{
		GpuTextureDesc desc = {};
		desc._device = device;
		desc._format = FORMAT_R32G32_UINT;
		desc._width = static_cast<u64>(viewInfo->_viewPort._width);
		desc._height = static_cast<u64>(viewInfo->_viewPort._height);
		desc._flags = RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		desc._initialState = RESOURCE_STATE_RENDER_TARGET;
		_triangleIdTexture.initialize(desc);
		_triangleIdTexture.setDebugName("Visiblity Buffer Triangle ID");

		desc._format = FORMAT_R8G8_UINT;
		desc._width = static_cast<u64>(viewInfo->_viewPort._width / MATERIAL_RANGE_TILE_SIZE) + 1;
		desc._height = static_cast<u64>(viewInfo->_viewPort._height / MATERIAL_RANGE_TILE_SIZE) + 1;
		desc._flags = RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		desc._initialState = RESOURCE_STATE_UNORDERED_ACCESS;
		_materialRangeTexture.initialize(desc);
		_materialRangeTexture.setDebugName("Visiblity Buffer Material Range");
	}

	{
		GpuTextureDesc desc = {};
		desc._device = device;
		desc._format = FORMAT_D16_UNORM;
		desc._width = static_cast<u64>(viewInfo->_viewPort._width);
		desc._height = static_cast<u64>(viewInfo->_viewPort._height);
		desc._flags = RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		desc._initialState = RESOURCE_STATE_DEPTH_WRITE;
		_materialIdTexture.initialize(desc);
		_materialIdTexture.setDebugName("Visiblity Buffer Material ID Depth");
	}

	// RTV
	{
		DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getRtvGpuDescriptorAllocator();
		_triangleIdRtv = allocator->allocateDescriptors(1);
		device->createRenderTargetView(_triangleIdTexture.getResource(), _triangleIdRtv._cpuHandle);
	}

	// DSV
	{
		DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getDsvGpuDescriptorAllocator();
		_materialIdDsv = allocator->allocateDescriptors(1);
		device->createDepthStencilView(_materialIdTexture.getResource(), _materialIdDsv._cpuHandle);
	}

	// SRV & UAV
	{
		DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
		_triangleIdSrv = allocator->allocateDescriptors(1);
		_materialRangeSrv = allocator->allocateDescriptors(1);
		device->createShaderResourceView(_triangleIdTexture.getResource(), nullptr, _triangleIdSrv._cpuHandle);
		device->createShaderResourceView(_materialRangeTexture.getResource(), nullptr, _materialRangeSrv._cpuHandle);
	}

	{
		//ShaderResourceViewDesc desc = {};
		//desc._format = FORMAT_UNKNOWN;
		//desc._viewDimension = SRV_DIMENSION_BUFFER;
		//desc._buffer._firstElement = 0;
		//desc._buffer._flags = BUFFER_SRV_FLAG_NONE;
		//desc._buffer._numElements = gpu::SHADER_SET_COUNT_MAX;
		//desc._buffer._structureByteStride = sizeof(u32);

		//_indirectArgumentOffsetSrv = allocator->allocateDescriptors(1);
	}

	{
		//UnorderedAccessViewDesc desc = {};
		//desc._format = FORMAT_R32_TYPELESS;
		//desc._viewDimension = UAV_DIMENSION_BUFFER;
		//desc._buffer._firstElement = 0;
		//desc._buffer._numElements = MESH_COUNT_MAX;
		//desc._buffer._flags = BUFFER_UAV_FLAG_RAW;
	}
}

void VisiblityBufferRenderer::terminate() {
	_triangleIdTexture.terminate();
	_materialIdTexture.terminate();
	_materialRangeTexture.terminate();

	{
		DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getRtvGpuDescriptorAllocator();
		allocator->discardDescriptor(_triangleIdRtv);
	}

	{
		DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getDsvGpuDescriptorAllocator();
		allocator->discardDescriptor(_materialIdDsv);
	}

	{
		DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
		allocator->discardDescriptor(_triangleIdSrv);
		allocator->discardDescriptor(_materialRangeSrv);
	}
}
