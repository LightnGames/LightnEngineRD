#include <GfxCore/impl/DebugWindow.h>
#include <GfxCore/impl/GraphicsSystemImpl.h>
#include <GfxCore/impl/QueryHeapSystem.h>
#include <Core/Application.h>
#include <direct.h>

DescriptorHandle _imguiHandle;

#if DEBUG_WINDOW_ENABLE
#define SWITCH_CODE(x) x
#else
#define SWITCH_CODE(x) true
#endif

void DebugWindow::Start(const char* windowName) {
	DebugGui::Start(windowName);
}

void DebugWindow::initialize() {
#if DEBUG_WINDOW_ENABLE
	GraphicsSystemImpl* graphicsSystem = GraphicsSystemImpl::Get();
	DescriptorHeapAllocator* descriptorHeapAllocater = graphicsSystem->getSrvCbvUavGpuDescriptorAllocator();
	_imguiHandle = descriptorHeapAllocater->allocateDescriptors(1);

	Application* app = ApplicationSystem::Get()->getApplication();
	DebugGui::DebugWindowDesc desc = {};
	desc._device = graphicsSystem->getDevice();
	desc._descriptorHeap = descriptorHeapAllocater->getDescriptorHeap();
	desc._srvHandle = _imguiHandle;
	desc._hWnd = app->getWindowHandle();
	desc._bufferingCount = BACK_BUFFER_COUNT;
	desc._rtvFormat = BACK_BUFFER_FORMAT;
	DebugGui::InitializeDebugWindowGui(desc);

	if(_mkdir(DEBUG_WINDOW_STRUCTURE_FOLDER_PATH)) {
		LTN_INFO("create debug window folder %s", DEBUG_WINDOW_STRUCTURE_FOLDER_PATH);
	}

	s32* hWnd = app->getWindowHandle();
	ApplicationCallBack f = [hWnd](u32 message, u64 wParam, s64 lParam) {
		DebugGui::TranslateWindowProc(hWnd, message, wParam, lParam);
	};
	app->registApplicationCallBack(f);

#endif
}

void DebugWindow::terminate() {
#if DEBUG_WINDOW_ENABLE
	DebugGui::TerminateDebugWindowGui();

	DescriptorHeapAllocator* descriptorHeap = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
	descriptorHeap->discardDescriptor(_imguiHandle);
#endif
}

void DebugWindow::beginFrame() {
#if DEBUG_WINDOW_ENABLE
	DebugGui::BeginDebugWindowGui();
#endif
}

void DebugWindow::renderFrame(CommandList* commandList) {
#if DEBUG_WINDOW_ENABLE
	QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
	DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::YELLOW, "ImGui");
	DebugGui::RenderDebugWindowGui(commandList);
	queryHeapSystem->setCurrentMarkerName("ImGui");
	queryHeapSystem->setMarker(commandList);
#endif
}

void DebugWindow::Text(const char * text, ...) {
	va_list args;
	va_start(args, text);
	SWITCH_CODE(DebugGui::Text(text, args));
	va_end(args);
}

bool DebugWindow::DragFloat(const char* label, float* v, float v_speed, float v_min, float v_max, const char* format, float power) {
	return SWITCH_CODE(DebugGui::DragFloat(label, v, v_speed, v_min, v_max, format, power));
}

bool DebugWindow::DragFloat2(const char* label, float v[2], float v_speed, float v_min, float v_max, const char* format, float power) {
	return SWITCH_CODE(DebugGui::DragFloat2(label, v, v_speed, v_min, v_max, format, power));
}

bool DebugWindow::DragFloat3(const char* label, float v[3], float v_speed, float v_min, float v_max, const char* format, float power) {
	return SWITCH_CODE(DebugGui::DragFloat3(label, v, v_speed, v_min, v_max, format, power));
}

bool DebugWindow::DragFloat4(const char* label, float v[4], float v_speed, float v_min, float v_max, const char* format, float power) {
	return SWITCH_CODE(DebugGui::DragFloat4(label, v, v_speed, v_min, v_max, format, power));
}

bool DebugWindow::SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format, float power) {
	return SWITCH_CODE(DebugGui::SliderFloat(label, v, v_min, v_max, format, power));
}

bool DebugWindow::SliderFloat2(const char* label, float v[2], float v_min, float v_max, const char* format, float power) {
	return SWITCH_CODE(DebugGui::SliderFloat2(label, v, v_min, v_max, format, power));
}

bool DebugWindow::SliderFloat3(const char* label, float v[3], float v_min, float v_max, const char* format, float power) {
	return SWITCH_CODE(DebugGui::SliderFloat3(label, v, v_min, v_max, format, power));
}

bool DebugWindow::SliderFloat4(const char* label, float v[4], float v_min, float v_max, const char* format, float power) {
	return SWITCH_CODE(DebugGui::SliderFloat4(label, v, v_min, v_max, format, power));
}

bool DebugWindow::SliderAngle(const char* label, float* v_rad, float v_degrees_min, float v_degrees_max, const char* format) {
	return SWITCH_CODE(DebugGui::SliderAngle(label, v_rad, v_degrees_min, v_degrees_max, format));
}

bool DebugWindow::ColorEdit3(const char* label, float col[3], DebugGui::ColorEditFlags flags) {
	return SWITCH_CODE(DebugGui::ColorEdit3(label, col, flags));
}

bool DebugWindow::ColorEdit4(const char* label, float col[4], DebugGui::ColorEditFlags flags) {
	return SWITCH_CODE(DebugGui::ColorEdit4(label, col, flags));
}

bool DebugWindow::Checkbox(const char* label, bool* v) {
	return SWITCH_CODE(DebugGui::Checkbox(label, v));
}

bool DebugWindow::Combo(const char* label, s32* current_item, const char* const items[], s32 items_count, s32 popup_max_height_in_items) {
	return false;
}

void DebugWindow::End() {
	DebugGui::End();
}
