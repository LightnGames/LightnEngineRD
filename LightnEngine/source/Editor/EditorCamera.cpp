#include "EditorCamera.h"
#include <Application/Application.h>
#include <RendererScene/View.h>
#include <Editor/DebugSerializedStructure.h>
#include <ThiredParty/ImGui/imgui.h>

#include <RendererScene/CommonResource.h>
#include <RendererScene/MeshInstance.h>
#include <RendererScene/Mesh.h>

namespace ltn {
MeshInstance* _meshInstance;
MeshInstance* _meshInstance2;
void EditorCamera::initialize() {
	_view = ViewScene::Get()->createView();

	Application* app = ApplicationSysytem::Get()->getApplication();
	_view->setWidth(app->getScreenWidth());
	_view->setHeight(app->getScreenHeight());

	//{
	//	const MeshPreset* meshPreset = CommonResource::Get()->getQuadMeshPreset();
	//	_meshInstance = meshPreset->createMeshInstance(1);
	//	_meshInstance->setWorldMatrix(Matrix4::translationFromVector(Vector3(1, 2, 3)));
	//	
	//	_meshInstance2 = meshPreset->createMeshInstance(1);
	//	_meshInstance2->setWorldMatrix(Matrix4::translationFromVector(Vector3(-1, 0, 0)));
	//	_meshInstance2->setMaterial(StrHash64("lambert2"), CommonResource::Get()->getGrayMaterial());
	//}
}

void EditorCamera::terminate() {
	//MeshInstanceScene::Get()->destroyMeshInstance(_meshInstance);
	//MeshInstanceScene::Get()->destroyMeshInstance(_meshInstance2);
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
