#include "EditorCamera.h"
#include <Application/Application.h>
#include <RendererScene/View.h>

#include <RendererScene/Shader.h>
#include <RendererScene/Material.h>
#include <RendererScene/Mesh.h>
#include <RendererScene/StaticMesh.h>
#include <RendererScene/MeshInstance.h>

namespace ltn {
Shader* _vertexShader;
Shader* _pixelShader;
Material* _material;
Mesh* _mesh;

StaticMesh _staticMesh;
MeshInstance* _meshInstance;
void EditorCamera::initialize() {
	_view = ViewScene::Get()->createView();

	Application* app = ApplicationSysytem::Get()->getApplication();
	_view->setWidth(app->getScreenWidth());
	_view->setHeight(app->getScreenHeight());


	{
		ShaderScene::CreatationDesc desc;
		desc._assetPath = "EngineComponent\\Shader\\Material\\Default.vso";
		_vertexShader = ShaderScene::Get()->createShader(desc);

		desc._assetPath = "EngineComponent\\Shader\\Material\\Default.pso";
		_pixelShader = ShaderScene::Get()->createShader(desc);
	}

	{
		MaterialScene::CreatationDesc desc;
		desc._assetPath = "EngineComponent\\Material\\Default.mto";
		_material = MaterialScene::Get()->createMaterial(desc);
	}

	{
		MeshScene::CreatationDesc desc;
		desc._assetPath = "EngineComponent\\BasicShape\\Quad.mesh";
		_mesh = MeshScene::Get()->createMesh(desc);
	}

	{
		StaticMesh::MeshCreatationDesc desc;
		desc._assetPath = "EngineComponent\\BasicShape\\Quad.smesh";
		_staticMesh.create(desc);
	}

	{
		MeshInstanceScene::MeshInstanceCreatationDesc desc;
		desc._mesh = _staticMesh.getMesh();
		_meshInstance = MeshInstanceScene::Get()->createMeshInstance(desc);
	}
}

void EditorCamera::terminate() {
	ShaderScene::Get()->destroyShader(_vertexShader);
	ShaderScene::Get()->destroyShader(_pixelShader);
	MaterialScene::Get()->destroyMaterial(_material);
	MeshScene::Get()->destroyMesh(_mesh);

	_staticMesh.destroy();
	MeshInstanceScene::Get()->destroyMeshInstance(_meshInstance);
	ViewScene::Get()->destroyView(_view);
}

void EditorCamera::update() {
}
}
