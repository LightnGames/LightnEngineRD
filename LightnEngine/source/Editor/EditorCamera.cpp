#include "EditorCamera.h"
#include <Application/Application.h>
#include <RendererScene/View.h>
#include <Editor/DebugSerializedStructure.h>
#include <ThiredParty/ImGui/imgui.h>

#include <RendererScene/CommonResource.h>
#include <RendererScene/MeshInstance.h>
#include <RendererScene/Mesh.h>

#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/GpuTimerManager.h>
#include <Core/CpuTimerManager.h>
#include <Renderer/RHI/Rhi.h>

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

	ImGui::Begin("TestInfo");
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

	rhi::QueryVideoMemoryInfo videoMemoryInfo = DeviceManager::Get()->getHardwareAdapter()->queryVideoMemoryInfo();
	ImGui::Text("[VMEM info] %-14s / %-14s byte", ThreeDigiets(videoMemoryInfo._currentUsage).get(), ThreeDigiets(videoMemoryInfo._budget).get());

	{
		ImGui::Separator();
		GpuTimerManager* timerManager = GpuTimerManager::Get();
		const u64* timeStamps = GpuTimeStampManager::Get()->getGpuTimeStamps();
		f64 gpuTickDelta = GpuTimeStampManager::Get()->getGpuTickDeltaInMilliseconds();
		u32 timerCount = timerManager->getTimerCount();
		for (u32 i = 0; i < timerCount; ++i) {
			u32 offset = i * 2;
			f64 time = (timeStamps[offset + 1] - timeStamps[offset]) * gpuTickDelta;
			ImGui::Text("%-20s %2.3f ms", timerManager->getGpuTimerAdditionalInfo(i)->_name, time);
		}
	}

	{
		ImGui::Separator();
		CpuTimerManager* timerManager = CpuTimerManager::Get();
		const u64* timeStamps = CpuTimeStampManager::Get()->getCpuTimeStamps();
		f64 gpuTickDelta = CpuTimeStampManager::Get()->getCpuTickDeltaInMilliseconds();
		u32 timerCount = timerManager->getTimerCount();
		for (u32 i = 0; i < timerCount; ++i) {
			u32 offset = i * 2;
			f64 time = (timeStamps[offset + 1] - timeStamps[offset]) * gpuTickDelta;
			ImGui::Text("%-20s %2.3f ms", timerManager->getGpuTimerAdditionalInfo(i)->_name, time);
		}
	}

	ImGui::End();
}
}
