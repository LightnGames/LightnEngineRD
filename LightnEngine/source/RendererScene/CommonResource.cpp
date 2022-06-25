#include "CommonResource.h"
#include <Core/CpuTimerManager.h>
#include <RendererScene/Shader.h>
#include <RendererScene/Material.h>
#include <RendererScene/MeshGeometry.h>
#include <RendererScene/MeshInstance.h>
#include <RendererScene/Mesh.h>
#include <RendererScene/Texture.h>

namespace ltn {
namespace {
CommonResource g_commonResource;
}
void CommonResource::initialize() {
	CpuScopedPerf scopedPerf("CommonResource");
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
		MeshGeometryScene::CreatationDesc desc;
		desc._assetPath = "EngineComponent\\BasicShape\\Quad.meshg";
		_meshGeometry = MeshGeometryScene::Get()->createMeshGeometry(desc);
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
	MaterialScene::Get()->destroyMaterial(_whiteMaterial);
	MaterialScene::Get()->destroyMaterial(_grayMaterial);
	MeshGeometryScene::Get()->destroyMeshGeometry(_meshGeometry);
	MeshScene::Get()->destroyMesh(_mesh);
	TextureScene::Get()->destroyTexture(_checkerTexture);
	PipelineSetScene::Get()->destroyPipelineSet(_pipelineSet);
}

CommonResource* CommonResource::Get() {
	return &g_commonResource;
}
}
