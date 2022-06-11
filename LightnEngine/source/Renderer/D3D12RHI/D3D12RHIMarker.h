#pragma once
#include <Core/Math.h>
namespace ltn {
namespace rhi {
struct CommandList;
void BeginMarker(rhi::CommandList* commandList, const Color4& color, const char* name);
void EndMarker(rhi::CommandList* commandList);
}
}