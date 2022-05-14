#include "DeviceManager.h"

namespace ltn {
namespace {
DeviceManager g_deviceManager;
}
void DeviceManager::initialize() {
	rhi::HardwareFactoryDesc factoryDesc = {};
	factoryDesc._flags = rhi::HardwareFactoryDesc::FACTROY_FLGA_DEVICE_DEBUG;
	_factory.initialize(factoryDesc);

	rhi::HardwareAdapterDesc adapterDesc = {};
	adapterDesc._factory = &_factory;
	_adapter.initialize(adapterDesc);

	rhi::DeviceDesc deviceDesc = {};
	deviceDesc._adapter = &_adapter;
	_device.initialize(deviceDesc);
}
void DeviceManager::terminate() {
	_adapter.terminate();
	_factory.terminate();
	_device.terminate();
}
DeviceManager* DeviceManager::Get() {
	return &g_deviceManager;
}
}
