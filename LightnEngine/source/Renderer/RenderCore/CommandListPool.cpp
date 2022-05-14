#include "CommandListPool.h"
namespace ltn {
namespace {
CommandListPool g_commandListPool;
}
void CommandListPool::initialize(const Desc& desc) {
	rhi::CommandListDesc commandListDesc = {};
	commandListDesc._device = desc._device;
	commandListDesc._type = desc._type;
	for (u32 commandListIndex = 0; commandListIndex < COMMAND_LIST_COUNT_MAX; ++commandListIndex) {
		_commandLists[commandListIndex].initialize(commandListDesc);
	}
}
void CommandListPool::terminate() {
	for (u32 commandListIndex = 0; commandListIndex < COMMAND_LIST_COUNT_MAX; ++commandListIndex) {
		_commandLists[commandListIndex].terminate();
	}
}

rhi::CommandList* CommandListPool::allocateCommandList(u64 fenceValue) {
	for (u32 commandListIndex = 0; commandListIndex < COMMAND_LIST_COUNT_MAX; ++commandListIndex) {
		rhi::CommandList* commandList = &_commandLists[commandListIndex];
		if (commandList->_fenceValue < fenceValue || commandList->_fenceValue == rhi::INVAILD_FENCE_VALUE) {
			return &_commandLists[commandListIndex];
		}
	}

	return nullptr;
}
CommandListPool* CommandListPool::Get() {
	return &g_commandListPool;
}
}