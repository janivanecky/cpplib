#pragma once
#include <Windows.h>
#include <stdint.h>

#define IS_WINDOW_VALID(window) (!(window.window_handle == INVALID_HANDLE_VALUE))

typedef LARGE_INTEGER Ticks;
struct Input
{
	float mouse_dx;
	float mouse_dy;
	float mouse_x = -1.0f;
	float mouse_y = -1.0f;

	bool mouse_lbutton_pressed = false;
	bool mouse_lbutton_down = false;
};

struct Window
{
	HWND window_handle;
	uint32_t window_width;
	uint32_t window_height;
};

enum EventType
{
	EMPTY = 0,
	MOUSE_MOVE,
	MOUSE_LBUTTON_DOWN,
	MOUSE_LBUTTON_UP,
	KEY_DOWN,
	KEY_UP,
	MOUSE_WHEEL,
	EXIT
};

struct MouseMoveData
{
	float x;
	float y;
};

enum KeyCode
{
	ESC = 0,
	F10 = 1,
};

struct KeyPressedData
{
	KeyCode code;
};

struct MouseWheelData
{
	float delta;
};

struct Event
{
	EventType type;
	char data[32] = {};
};

namespace platform
{
	Window get_window(char *window_name, uint32_t window_width, uint32_t window_height);
	bool is_window_valid(Window *window);

	bool get_event(Event *event);

	void show_cursor();
	void hide_cursor();
	
	Ticks get_ticks();
	Ticks get_tick_frequency();
	float get_dt_from_tick_difference(Ticks t1, Ticks t2, Ticks frequency);
}

struct Timer
{
    Ticks frequency;
    Ticks start;
};

namespace timer
{
    Timer get();
    void start(Timer *timer);
    float end(Timer *timer);
	float reset(Timer *timer);
    float checkpoint(Timer *timer);
}