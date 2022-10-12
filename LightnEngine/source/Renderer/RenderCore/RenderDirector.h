#pragma once
#include <Renderer/RHI/Rhi.h>

namespace ltn {
class RenderDirector{
public:
	void initialize();
	void terminate();
	void update();
	void render(rhi::CommandList* commandList);

	static RenderDirector* Get();
private:
	s32 _debugGeometryVisualizeType = 0;
	s32 _debugMaterialVisualizeType = 0;
};
}