#include <windowsx.h>
#include "platform.h"

// Constants
const char *BROADCAST_MESSAGE_IDENTIFIER = "cpplib_broadcast";
const uint32_t BROADCAST_MESSAGE_ID = RegisterWindowMessageA(BROADCAST_MESSAGE_IDENTIFIER);
const char *WINDOW_CLASS_NAME = "cpplib_window_class";

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_CLOSE:
	{
		PostQuitMessage(0);
	}
	break;
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
	return 0;
}

bool platform::get_event(Event *event, bool broadcast_message) {
	MSG message;

	event->type = EMPTY;
	bool is_message = PeekMessageA(&message, NULL, 0, 0, PM_REMOVE);
	if(!is_message) return false;

	// In case message ID is higher than BROADCAST_MESSAGE_ID it means that the message is a custom message.
	// We'll assume that custom message is sent from other process by setting
	// `broadcast_message` to `true`. This means that the message ID was incremented by 
	// `BROADCAST_MESSAGE_ID`, so we had to adjust the received message ID to recover the original ID.
	// Note that in case `broadcast_message` is true, we're assuming that previous caller of this function
	// in this process broadcasted the message, so we should just discard it.
	if(message.message > BROADCAST_MESSAGE_ID) {
		if(broadcast_message) return true;
		message.message -= BROADCAST_MESSAGE_ID;
	}

	if(broadcast_message) {
		// Broadcast message. This is used to send messages to other processes running platform:: code.
		PostMessageA(HWND_BROADCAST, message.message + BROADCAST_MESSAGE_ID, message.wParam, message.lParam);
	}

	switch (message.message) {
	case WM_INPUT:
	{
		KeyPressedData *data = (KeyPressedData *)event->data;

		char buffer[sizeof(RAWINPUT)] = {};
		UINT size = sizeof(RAWINPUT);
		GetRawInputData(reinterpret_cast<HRAWINPUT>(message.lParam), RID_INPUT, buffer, &size, sizeof(RAWINPUTHEADER));

		// extract keyboard raw input data
		RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buffer);
		if (raw->header.dwType == RIM_TYPEKEYBOARD) {
			const RAWKEYBOARD& raw_kb = raw->data.keyboard;
			if (raw_kb.Flags & RI_KEY_BREAK) {
				event->type = KEY_UP;
			} else {
				event->type = KEY_DOWN;
			}

			if (raw_kb.VKey == VK_ESCAPE) data->code = KeyCode::ESC;
			else if(raw_kb.VKey == VK_F1) data->code = KeyCode::F1;
			else if(raw_kb.VKey == VK_F2) data->code = KeyCode::F2;
			else if(raw_kb.VKey == VK_F3) data->code = KeyCode::F3;
			else if(raw_kb.VKey == VK_F4) data->code = KeyCode::F4;
			else if(raw_kb.VKey == VK_F5) data->code = KeyCode::F5;
			else if(raw_kb.VKey == VK_F6) data->code = KeyCode::F6;
			else if(raw_kb.VKey == VK_F7) data->code = KeyCode::F7;
			else if(raw_kb.VKey == VK_F8) data->code = KeyCode::F8;
			else if(raw_kb.VKey == VK_F9) data->code = KeyCode::F9;
			else if(raw_kb.VKey == VK_F10) data->code = KeyCode::F10;
			else if(raw_kb.VKey == VK_MENU) data->code = KeyCode::ALT;
			else if(raw_kb.VKey == VK_SPACE) data->code = KeyCode::SPACE;
			else if(raw_kb.VKey == VK_LEFT) data->code = KeyCode::LEFT;
			else if(raw_kb.VKey == VK_RIGHT) data->code = KeyCode::RIGHT;
			else if(raw_kb.VKey == VK_DELETE) data->code = KeyCode::DEL;
			else if(raw_kb.VKey == VK_BACK) data->code = KeyCode::BACKSPACE;
			else if(raw_kb.VKey == VK_HOME) data->code = KeyCode::HOME;
			else if(raw_kb.VKey == VK_END) data->code = KeyCode::END;
			else if(raw_kb.VKey == VK_RETURN) data->code = KeyCode::ENTER;
			else if(raw_kb.VKey == 0x57) data->code = KeyCode::W;
			else if(raw_kb.VKey == 0x41) data->code = KeyCode::A;
			else if(raw_kb.VKey == 0x53) data->code = KeyCode::S;
			else if(raw_kb.VKey == 0x44) data->code = KeyCode::D;
			else data->code = KeyCode::OTHER;
		}
	} break;
	case WM_CHAR:
		switch (message.wParam) {
			case 0x08:
				// Process a backspace.
				break;
			case 0x0A:
				// Process a linefeed.
				break;
			case 0x1B:
				// Process an escape.
				break;
			case 0x09:
				// Process a tab.
				break;
			case 0x0D:
				// Process a carriage return.
				break;
			default:
				// Process displayable characters.
				event->type = CHAR_ENTERED;
				event->data[0] = (char)message.wParam;
				break;
		}
	break;
	case WM_QUIT:
	{
		event->type = EXIT;
	}
	break;
	case WM_LBUTTONUP:
	{
		// See below in WM_LBUTTONDOWN for what this does.
		HWND window = GetActiveWindow();
		ReleaseCapture();

		event->type = MOUSE_LBUTTON_UP;
	} break;
	case WM_LBUTTONDOWN:
	{
		// This deals with situation when you press the button, go outside the window and release.
		// Without this, you wouldn't register BUTTON_UP message.
		HWND window = GetActiveWindow();
		SetCapture(window);

		event->type = MOUSE_LBUTTON_DOWN;
	} break;
	case WM_MOUSEMOVE:
	{
		event->type = MOUSE_MOVE;
		MouseMoveData *data = (MouseMoveData *)event->data;
		data->x = (float)GET_X_LPARAM(message.lParam);
		data->y = (float)GET_Y_LPARAM(message.lParam);

		// Compute mouse position relative to screen.
		POINT pos = {
			(LONG)GET_X_LPARAM(message.lParam),
			(LONG)GET_Y_LPARAM(message.lParam)
		};
		HWND window = GetActiveWindow();
		ClientToScreen(window, &pos);
		data->screen_x = (float)pos.x;
		data->screen_y = (float)pos.y;
	} break;
	case WM_MOUSEWHEEL:
	{
		event->type = MOUSE_WHEEL;
		MouseWheelData *data = (MouseWheelData *)event->data;
		data->delta = GET_WHEEL_DELTA_WPARAM(message.wParam) / 120.0f;
	} break;
	default:
		TranslateMessage(&message);
		DispatchMessageA(&message);
	}

	return true;
}

HWND platform::get_existing_window(char *window_name) {
	// Retrieve existing window.
	HWND window = FindWindowA(WINDOW_CLASS_NAME, window_name);

	// In case NULL was returned, the window was not found, so we
	// set the value to INVALID_HANDLE_VALUE.
	if(!window) window = (HWND)INVALID_HANDLE_VALUE;

	return window;
}

HWND platform::get_window(char *window_name, uint32_t window_width, uint32_t window_height) {
	HINSTANCE program_instance = GetModuleHandle(0);

	WNDCLASSEXA window_class = {};
	window_class.cbSize = sizeof(WNDCLASSEXA);
	window_class.style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;
	window_class.lpfnWndProc = &WindowProc;
	window_class.hInstance = (HINSTANCE)program_instance;
	window_class.hCursor = LoadCursor(0, IDC_ARROW);
	window_class.lpszClassName = WINDOW_CLASS_NAME;

	HWND window = (HWND)INVALID_HANDLE_VALUE;
	if (RegisterClassExA(&window_class)) {
		DWORD window_flags = WS_VISIBLE | WS_OVERLAPPEDWINDOW;
		//window_flags = WS_VISIBLE | WS_POPUP;
		RECT window_rect = {0, 0, (LONG)window_width, (LONG)window_height};
		AdjustWindowRect(&window_rect, window_flags, FALSE);
		window_width = window_rect.right - window_rect.left;
		window_height = window_rect.bottom - window_rect.top;

		window = CreateWindowA(WINDOW_CLASS_NAME, window_name, window_flags,
								0, 0, window_width, window_height, NULL, NULL, program_instance, NULL);

		RAWINPUTDEVICE device;
		device.usUsagePage = 0x01;
		device.usUsage = 0x06;
		// NOTE: This used to have value RIDEV_NOLEGACY, but that prevents WM_CHAR messages from firing.
		device.dwFlags = 0;
		device.hwndTarget = window;
		RegisterRawInputDevices(&device, 1, sizeof(device));
	}
	return window;
}

bool platform::is_window_valid(HWND window) {
	return window != INVALID_HANDLE_VALUE;
}

void platform::show_cursor() {
	ShowCursor(TRUE);
}

void platform::hide_cursor() {
	while(ShowCursor(FALSE) >= 0);
}

SYSTEMTIME platform::get_datetime() {
	SYSTEMTIME time;
	GetLocalTime(&time);
	return time;
}

Ticks platform::get_ticks() {
	LARGE_INTEGER ticks;

	QueryPerformanceCounter(&ticks);

	return (Ticks)ticks;
}

Ticks platform::get_tick_frequency() {
	LARGE_INTEGER frequency;

	QueryPerformanceFrequency(&frequency);

	return (Ticks)frequency;
}

float platform::get_dt_from_tick_difference(Ticks t1, Ticks t2, Ticks frequency) {
	float dt = (float)((t2.QuadPart - t1.QuadPart) / (double)frequency.QuadPart);
	return dt;
}

Timer timer::get() {
	Timer timer = {};
	timer.frequency = platform::get_tick_frequency();
	return timer;
}

void timer::start(Timer *timer) {
	timer->start = platform::get_ticks();
}

float timer::end(Timer *timer) {
	Ticks current = platform::get_ticks();
	float dt = platform::get_dt_from_tick_difference(timer->start, current, timer->frequency);
	return dt;
}

float timer::checkpoint(Timer *timer) {
	Ticks current = platform::get_ticks();
	float dt = platform::get_dt_from_tick_difference(timer->start, current, timer->frequency);
	timer->start = current;

	return dt;
}