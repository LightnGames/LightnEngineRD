#pragma once
#include <Renderer/RHI/Rhi.h>

namespace ltn {
class CommandListPool {
public:
	static constexpr u32 COMMAND_LIST_COUNT_MAX = 16;

	struct Desc {
		rhi::Device* _device = nullptr;
		rhi::CommandListType _type = rhi::COMMAND_LIST_TYPE_DIRECT;
	};

	void initialize(const Desc& desc);
	void terminate();
	rhi::CommandList* allocateCommandList(u64 fenceValue);

	static CommandListPool* Get();

private:
	rhi::CommandList _commandLists[COMMAND_LIST_COUNT_MAX] = {};
};
}