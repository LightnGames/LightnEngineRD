#include "EditorCamera.h"
#include <Application/Application.h>
#include <RendererScene/View.h>
#include <Editor/DebugSerializedStructure.h>
#include <ThiredParty/ImGui/imgui.h>

#include <RendererScene/CommonResource.h>
#include <RendererScene/MeshInstance.h>
#include <RendererScene/Mesh.h>
#include <RendererScene/Light.h>

#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/GpuTimerManager.h>
#include <Core/CpuTimerManager.h>
#include <Core/InputSystem.h>
#include <Renderer/RHI/Rhi.h>

namespace ltn {
MeshInstance* _meshInstance;
MeshInstance* _meshInstance2;
void EditorCamera::initialize() {
	_view = ViewScene::Get()->createView();

	Application* app = ApplicationSysytem::Get()->getApplication();
	_view->setWidth(app->getScreenWidth());
	_view->setHeight(app->getScreenHeight());

	_directionalLight = LightScene::Get()->createDirectionalLight();
}

void EditorCamera::terminate() {
	ViewScene::Get()->destroyView(_view);
	LightScene::Get()->destroyDirectionalLight(_directionalLight);
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

	InputSystem* inputSystem = InputSystem::Get();
	
	// マウス右クリックでの画面回転
	{
		static Vector2 prevMousePosition = Vector2(0, 0);
		Vector2 currentMousePosition = inputSystem->getMousePosition();
		if (inputSystem->getKey(InputSystem::KEY_CODE_RBUTTON)) {
			constexpr f32 SCALE = 0.005f;
			Vector2 distance = currentMousePosition - prevMousePosition;
			cameraRotation.x += distance.getY() * SCALE;
			cameraRotation.y += distance.getX() * SCALE;
		}
		prevMousePosition = currentMousePosition;
	}

	// W/A/S/D キーボードによる移動
	Matrix4 cameraRotate = Matrix4::rotationXYZ(cameraRotation.x, cameraRotation.y, cameraRotation.z);
	{
		Vector3 rightDirection = cameraRotate.getCol(0).getVector3();
		Vector3 upDirection = cameraRotate.getCol(1).getVector3();
		Vector3 forwardDirection = cameraRotate.getCol(2).getVector3();

		Vector3 moveDirection = Vector3::Zero();
		if (inputSystem->getKey(InputSystem::KEY_CODE_W)) {
			moveDirection += forwardDirection;
		}

		if (inputSystem->getKey(InputSystem::KEY_CODE_S)) {
			moveDirection -= forwardDirection;
		}

		if (inputSystem->getKey(InputSystem::KEY_CODE_D)) {
			moveDirection += rightDirection;
		}

		if (inputSystem->getKey(InputSystem::KEY_CODE_A)) {
			moveDirection -= rightDirection;
		}

		if (inputSystem->getKey(InputSystem::KEY_CODE_E)) {
			moveDirection += upDirection;
		}

		if (inputSystem->getKey(InputSystem::KEY_CODE_Q)) {
			moveDirection -= upDirection;
		}

		constexpr f32 DEBUG_CAMERA_MOVE_SPEED = 0.15f;
		moveDirection = moveDirection.normalize() * DEBUG_CAMERA_MOVE_SPEED;
		cameraPosition.x += moveDirection.getX();
		cameraPosition.y += moveDirection.getY();
		cameraPosition.z += moveDirection.getZ();
	}

	Camera* camera = _view->getCamera();
	camera->_position = cameraPosition;
	camera->_rotation = cameraRotation;
	camera->_worldMatrix = cameraRotate * Matrix4::translationFromVector(Vector3(cameraPosition));
	camera->_fov = DegToRad(60.0f);
	_view->postUpdate();

	ImGui::Begin("TestInfo");
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

	rhi::QueryVideoMemoryInfo videoMemoryInfo = DeviceManager::Get()->getHardwareAdapter()->queryVideoMemoryInfo();
	ImGui::Text("[VMEM info] %-14s / %-14s byte", ThreeDigiets(videoMemoryInfo._currentUsage).get(), ThreeDigiets(videoMemoryInfo._budget).get());

	// GPU Perf 表示
	{
		ImGui::Separator();
		GpuTimerManager* timerManager = GpuTimerManager::Get();
		const u64* timeStamps = GpuTimeStampManager::Get()->getGpuTimeStamps();
		f64 gpuTickDelta = GpuTimeStampManager::Get()->getGpuTickDeltaInMilliseconds();
		u32 timerCount = timerManager->getTimerCount();
		for (u32 i = 0; i < timerCount; ++i) {
			u32 offset = i * 2;
			f64 time = (timeStamps[offset + 1] - timeStamps[offset]) * gpuTickDelta;
			ImGui::Text("%-25s %2.3f ms", timerManager->getGpuTimerAdditionalInfo(i)->_name, time);
		}
	}

	// CPU Perf 表示
	{
		ImGui::Separator();
		CpuTimerManager* timerManager = CpuTimerManager::Get();
		const u64* timeStamps = CpuTimeStampManager::Get()->getCpuTimeStamps();
		f64 gpuTickDelta = CpuTimeStampManager::Get()->getCpuTickDeltaInMilliseconds();
		u32 timerCount = timerManager->getTimerCount();
		for (u32 i = 0; i < timerCount; ++i) {
			u32 offset = i * 2;
			f64 time = (timeStamps[offset + 1] - timeStamps[offset]) * gpuTickDelta;
			ImGui::Text("%-25s %2.3f ms", timerManager->getGpuTimerAdditionalInfo(i)->_name, time);
		}
	}

	ImGui::End();

	struct DirectionalLightInfo {
		Float3 _direction;
		Color3 _color;
		f32 _intensity;
	};

	auto lightInfo = DebugSerializedStructure<DirectionalLightInfo>("EditorLightInfo");
	Float3& directionalLightRotation = lightInfo._direction;

	ImGui::Begin("EditorLight");
	ImGui::SliderAngle("RotationPitch", reinterpret_cast<f32*>(&directionalLightRotation.x));
	ImGui::SliderAngle("RotationYaw", reinterpret_cast<f32*>(&directionalLightRotation.y));
	ImGui::SliderAngle("RotationRoll", reinterpret_cast<f32*>(&directionalLightRotation.z));
	ImGui::SliderFloat("Intensity", &lightInfo._intensity, 0.0f, 10.0f);
	ImGui::ColorEdit3("Color", reinterpret_cast<f32*>(&lightInfo._color));
	ImGui::End();

	_directionalLight->setIntensity(lightInfo._intensity);
	_directionalLight->setDirection(Matrix4::rotationXYZ(directionalLightRotation.x, directionalLightRotation.y, directionalLightRotation.z).getCol(2).getVector3());
	_directionalLight->setColor(Color(lightInfo._color));
	LightScene::Get()->postUpdateDirectionalLight(_directionalLight);
}
}
