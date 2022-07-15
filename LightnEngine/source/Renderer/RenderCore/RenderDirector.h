#pragma once
#include <Renderer/RHI/Rhi.h>
#include <Renderer/MeshRenderer/IndirectArgumentResource.h>

namespace ltn {
class RenderDirector{
public:
	void initialize();
	void terminate();
	void render(rhi::CommandList* commandList);

	static RenderDirector* Get();
private:
	IndirectArgumentResource _indirectArgumentResource;
};
}