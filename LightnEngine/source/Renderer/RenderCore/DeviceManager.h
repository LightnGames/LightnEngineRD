#pragma once
#include <Renderer/RHI/Rhi.h>
namespace ltn {
class DeviceManager{
public:
	void initialize();
	void terminate();

	rhi::Device* getDevice() { return &_device; }
	rhi::HardwareFactory* getHardwareFactory() { return &_factory; }
	rhi::HardwareAdapter* getHardwareAdapter() { return &_adapter; }

	static DeviceManager* Get();
private:
	rhi::Device _device;
	rhi::HardwareFactory _factory;
	rhi::HardwareAdapter _adapter;
};
}