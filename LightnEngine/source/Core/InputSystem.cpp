#include "InputSystem.h"

#include <Core/Utility.h>

namespace ltn {
InputSystem g_inputSystem;


bool InputSystem::getKey(KeyCode keyCode) const {
	return isKeyDown(_keyStates[keyCode]);
}

bool InputSystem::getKeyDown(KeyCode keyCode) const {
	return _keyDowns[keyCode] == 1;
}

bool InputSystem::getKeyUp(KeyCode keyCode) const {
	return _keyUps[keyCode] == 1;
}

void InputSystem::update() {
	u8 prevKeyState[256];
	memcpy(prevKeyState, _keyStates, LTN_COUNTOF(prevKeyState));

	GetKeyboardState(_keyStates);

	for (u16 i = 0; i < LTN_COUNTOF(prevKeyState); ++i) {
		_keyDowns[i] = (!isKeyDown(prevKeyState[i])) && isKeyDown(_keyStates[i]);
		_keyUps[i] = isKeyDown(prevKeyState[i]) && (!isKeyDown(_keyStates[i]));
	}

	if (isKeyDown(_keyStates[KEY_CODE_LBUTTON]) && (!isKeyDown(prevKeyState[KEY_CODE_LBUTTON]))) {
		_mousePositions[MOUSE_EVENT_L_DOWN] = _mousePosition;
	}

	if ((!isKeyDown(_keyStates[KEY_CODE_LBUTTON])) && isKeyDown(prevKeyState[KEY_CODE_LBUTTON])) {
		_mousePositions[MOUSE_EVENT_L_UP] = _mousePosition;
	}

	if (isKeyDown(_keyStates[KEY_CODE_RBUTTON]) && (!isKeyDown(prevKeyState[KEY_CODE_RBUTTON]))) {
		_mousePositions[MOUSE_EVENT_R_DOWN] = _mousePosition;
	}

	if ((!isKeyDown(_keyStates[KEY_CODE_LBUTTON])) && isKeyDown(prevKeyState[KEY_CODE_RBUTTON])) {
		_mousePositions[MOUSE_EVENT_R_UP] = _mousePosition;
	}
}

void InputSystem::setMouseEvent(Vector2 position) {
	_mousePosition = position;
}

InputSystem* InputSystem::Get() {
    return &g_inputSystem;
}
}
