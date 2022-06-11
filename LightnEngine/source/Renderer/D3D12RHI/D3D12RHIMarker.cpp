#include "D3D12RHIMarker.h"
#include "D3D12RHI.h"
#include <ThiredParty/WinPixEventRuntime/pix3.h>

namespace ltn {
namespace rhi {
void BeginMarker(rhi::CommandList* commandList, const Color4& color, const char* name) {
	u8 r = static_cast<u8>(color.x * 255);
	u8 g = static_cast<u8>(color.y * 255);
	u8 b = static_cast<u8>(color.z * 255);
	u64 pixColor = PIX_COLOR(r, g, b);
	PIXBeginEvent(commandList->_commandList, pixColor, name);
}
void EndMarker(rhi::CommandList* commandList) {
	PIXEndEvent(commandList->_commandList);
}
}
}
