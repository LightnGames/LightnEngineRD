#include "MeshRenderer.h"

namespace ltn {
namespace {
MeshRenderer g_meshRenderer;
}

void MeshRenderer::initialize() {
}

void MeshRenderer::terminate() {
}

void MeshRenderer::render(const RenderDesc& desc) {
}

MeshRenderer* MeshRenderer::Get() {
	return &g_meshRenderer;
}
}
