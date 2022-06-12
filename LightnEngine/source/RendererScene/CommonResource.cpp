#include "CommonResource.h"
#include <RendererScene/Shader.h>
#include <RendererScene/Material.h>
#include <RendererScene/Mesh.h>
#include <RendererScene/Model.h>
#include <RendererScene/MeshInstance.h>

namespace ltn {
namespace {
CommonResource g_commonResource;
}
void CommonResource::initialize() {
	{
		ShaderScene::CreatationDesc desc;
		desc._assetPath = "EngineComponent\\Shader\\Material\\Default.vso";
		_vertexShader = ShaderScene::Get()->createShader(desc);

		desc._TEST_collectParameter = true;
		desc._assetPath = "EngineComponent\\Shader\\Material\\Default.pso";
		_pixelShader = ShaderScene::Get()->createShader(desc);
	}

	{
		MaterialScene::MaterialCreatationDesc desc;
		desc._assetPath = "EngineComponent\\Material\\Default.mto";
		_material = MaterialScene::Get()->createMaterial(desc);
	}

	{
		MaterialScene::MaterialInstanceCreatationDesc desc;
		desc._assetPath = "EngineComponent\\Material\\White.mti";
		_materialInstance = MaterialScene::Get()->createMaterialInstance(desc);
	}

	{
		MeshScene::CreatationDesc desc;
		desc._assetPath = "EngineComponent\\BasicShape\\Quad.mesh";
		_mesh = MeshScene::Get()->createMesh(desc);
	}
}

void CommonResource::terminate() {
	ShaderScene::Get()->destroyShader(_vertexShader);
	ShaderScene::Get()->destroyShader(_pixelShader);
	MaterialScene::Get()->destroyMaterial(_material);
	MaterialScene::Get()->destroyMaterialInstance(_materialInstance);
	MeshScene::Get()->destroyMesh(_mesh);
}

CommonResource* CommonResource::Get() {
	return &g_commonResource;
}
}
