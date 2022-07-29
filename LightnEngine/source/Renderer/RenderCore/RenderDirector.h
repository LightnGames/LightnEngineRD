#pragma once
#include <Renderer/RHI/Rhi.h>

namespace ltn {
class RenderDirector{
public:
	void initialize();
	void terminate();
	void render(rhi::CommandList* commandList);

	static RenderDirector* Get();
};
}