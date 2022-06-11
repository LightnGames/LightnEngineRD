#include "EditorCamera.h"
#include <Application/Application.h>
#include <RendererScene/View.h>
#include <Editor/DebugSerializedStructure.h>
#include <ThiredParty/ImGui/imgui.h>

#include <RendererScene/Shader.h>
#include <RendererScene/Material.h>
#include <RendererScene/Mesh.h>
#include <RendererScene/Model.h>
#include <RendererScene/MeshInstance.h>

namespace ltn {
Shader* _vertexShader;
Shader* _pixelShader;
Material* _material;
MaterialInstance* _materialInstance;
MaterialInstance* _materialInstance2;
Mesh* _mesh;

Model _model;
MeshInstance* _meshInstance;
MeshInstance* _meshInstance2;
void EditorCamera::initialize() {
	_view = ViewScene::Get()->createView();

	Application* app = ApplicationSysytem::Get()->getApplication();
	_view->setWidth(app->getScreenWidth());
	_view->setHeight(app->getScreenHeight());


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
		_materialInstance2 = MaterialScene::Get()->createMaterialInstance(desc);

		_materialInstance2->setParameter<Float4>(StrHash32("BaseColor"), Float4(0.2f, 0.2f, 0.2f, 1.0f));
		MaterialScene::Get()->getMaterialInstanceUpdateInfos()->push(_materialInstance2);
	}

	{
		MeshScene::CreatationDesc desc;
		desc._assetPath = "EngineComponent\\BasicShape\\Quad.mesh";
		_mesh = MeshScene::Get()->createMesh(desc);
	}

	{
		Model::MeshCreatationDesc desc;
		desc._assetPath = "EngineComponent\\BasicShape\\Quad.mdl";
		_model.create(desc);
	}

	{
		MeshInstanceScene::MeshInstanceCreatationDesc desc;
		desc._mesh = _model.getMesh();
		_meshInstance = MeshInstanceScene::Get()->createMeshInstance(desc);
		_meshInstance->setWorldMatrix(Matrix4::translationFromVector(Vector3(1, 2, 3)));
		_meshInstance->getSubMeshInstance(0)->setMaterialInstance(_materialInstance);

		_meshInstance2 = MeshInstanceScene::Get()->createMeshInstance(desc);
		_meshInstance2->setWorldMatrix(Matrix4::translationFromVector(Vector3(-1, 0, 0)));
		_meshInstance2->getSubMeshInstance(0)->setMaterialInstance(_materialInstance2);
	}
}

void EditorCamera::terminate() {
	ShaderScene::Get()->destroyShader(_vertexShader);
	ShaderScene::Get()->destroyShader(_pixelShader);
	MaterialScene::Get()->destroyMaterial(_material);
	MaterialScene::Get()->destroyMaterialInstance(_materialInstance);
	MaterialScene::Get()->destroyMaterialInstance(_materialInstance2);
	MeshScene::Get()->destroyMesh(_mesh);

	_model.destroy();
	MeshInstanceScene::Get()->destroyMeshInstance(_meshInstance);
	MeshInstanceScene::Get()->destroyMeshInstance(_meshInstance2);
	ViewScene::Get()->destroyView(_view);
}

void EditorCamera::update() {
	struct CameraInfo {
		Float3 _cameraPosition;
		Float3 _cameraRotation;
	};

	auto info = DebugSerializedStructure<CameraInfo>("EditorCameraInfo");
	Float3& cameraPosition = info._cameraPosition;
	Float3& cameraRotation = info._cameraRotation;
	ImGui::Begin("EditorCamera");
	ImGui::DragFloat3("Position", reinterpret_cast<f32*>(&cameraPosition), 0.5f);
	ImGui::SliderAngle("RotationPitch", reinterpret_cast<f32*>(&cameraRotation.x));
	ImGui::SliderAngle("RotationYaw", reinterpret_cast<f32*>(&cameraRotation.y));
	ImGui::SliderAngle("RotationRoll", reinterpret_cast<f32*>(&cameraRotation.z));
	ImGui::End();

	Camera* camera = _view->getCamera();
	camera->_position = cameraPosition;
	camera->_rotation = cameraRotation;
	camera->_worldMatrix = Matrix4::rotationXYZ(cameraRotation.x, cameraRotation.y, cameraRotation.z) * Matrix4::translationFromVector(Vector3(cameraPosition));
	_view->postUpdate();
}
}
