#include "CommonResource.h"
#include <RendererScene/Shader.h>
#include <RendererScene/Material.h>
#include <RendererScene/Mesh.h>
#include <RendererScene/MeshInstance.h>
#include <RendererScene/MeshPreset.h>
#include <RendererScene/Texture.h>

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
		TextureScene::TextureCreatationDesc desc;
		desc._assetPath = "EngineComponent\\Texture\\Checker.dds";
		_checkerTexture = TextureScene::Get()->createTexture(desc);
	}

	{
		PipelineSetScene::CreatationDesc desc;
		desc._assetPath = "EngineComponent\\Material\\Default.pst";
		_pipelineSet = PipelineSetScene::Get()->createPipelineSet(desc);
	}

	{
		MaterialScene::MaterialCreatationDesc desc;
		desc._assetPath = "EngineComponent\\Material\\White.mto";
		_whiteMaterial = MaterialScene::Get()->createMaterial(desc);
	}

	{
		MaterialScene::MaterialCreatationDesc desc;
		desc._assetPath = "EngineComponent\\Material\\Gray.mto";
		_grayMaterial = MaterialScene::Get()->createMaterial(desc);
	}

	{
		MeshScene::CreatationDesc desc;
		desc._assetPath = "EngineComponent\\BasicShape\\Quad.mesh";
		_mesh = MeshScene::Get()->createMesh(desc);
	}

	{
		MeshPresetScene::MeshPresetCreatationDesc desc;
		desc._assetPath = "EngineComponent\\BasicShape\\Quad.mdl";
		_meshPreset = MeshPresetScene::Get()->createMeshPreset(desc);
	}
}

void CommonResource::terminate() {
	ShaderScene::Get()->destroyShader(_vertexShader);
	ShaderScene::Get()->destroyShader(_pixelShader);
	MaterialScene::Get()->destroyMaterial(_whiteMaterial);
	MaterialScene::Get()->destroyMaterial(_grayMaterial);
	MeshScene::Get()->destroyMesh(_mesh);
	MeshPresetScene::Get()->destroyMeshPreset(_meshPreset);
	TextureScene::Get()->destroyTexture(_checkerTexture);
	PipelineSetScene::Get()->destroyPipelineSet(_pipelineSet);
}

CommonResource* CommonResource::Get() {
	return &g_commonResource;
}
}
