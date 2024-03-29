#pragma once
#include <Windows.h>
#include <stdint.h>

/////////////////////////////////////////
// Event system specific structures
/////////////////////////////////////////
/*
Example of usage of the event system:

// Each frame
Event event;
while(platform::get_event(&event)) // Get all the events accumulated since the last frame
{
	// Check for event type
    switch(event.type)
	{
		case MOUSE_MOVE:
			// Since we now know that event is of MOUSE_MOVE type, we can just cast
			// Event's data member into MouseMoveData structure and read it that way.
			MouseMoveData *mouse_data = (MouseMoveData *)event.data;
		break;
	}
}
*/

// All the event types
enum EventType {
	EMPTY = 0,
	MOUSE_MOVE,
	MOUSE_LBUTTON_DOWN,
	MOUSE_LBUTTON_UP,
	KEY_DOWN,
	KEY_UP,
	CHAR_ENTERED,
	MOUSE_WHEEL,
	WINDOW_RESIZED,
	EXIT
};

// Event specific data structures

struct MouseMoveData {
	float x;
	float y;
	float screen_x;
	float screen_y;
};

enum KeyCode {
	ESC = 0,
	F1,
	F2,
	F3,
	F4,
	F5,
	F6,
	F7,
	F8,
	F9,
	F10,
	ALT,
	LEFT,
	RIGHT,
	DOWN,
	UP,
	SPACE,
	OTHER,
	DEL,
	ENTER,
	BACKSPACE,
	TAB,
	HOME,
	END,
	W,
	A,
	S,
	D
};

struct KeyPressedData {
	KeyCode code;
};

struct MouseWheelData {
	float delta;
};

struct WindowResizedData {
	float window_width;
	float window_height;
};

// Event data struct
struct Event {
	EventType type;
	char data[32] = {};
};

// Ticks represent CPU ticks
typedef LARGE_INTEGER Ticks;

// `platform` namespace handles interfacing with windows API, with the exception of file system interface
namespace platform {
	// Create and return windows with specific name and dimensions
	HWND get_window(
		char *window_name, uint32_t window_width, uint32_t window_height, int32_t x = 0, int32_t y = 0
	);

	// Get existing window.
	HWND get_existing_window(char *window_name);

	// Get window size and position from the last time it was open
	bool get_last_window_size_pos(
		char *window_name, uint32_t *width, uint32_t *height, int32_t *x, int32_t *y
	);

	// Check if window is valid
	bool is_window_valid(HWND window);

	// Get next Event, should be called per frame until false is returned
	bool get_event(Event *event, bool broadcast_message=false);

	// Cursor manipulation interface
	void show_cursor();
	void hide_cursor();
	
	// Get number of ticks since startup
	Ticks get_ticks();

	// Get tick frequency
	Ticks get_tick_frequency();

	// Compute time difference between two Ticks (number of ticks) and a tick update frequency
	float get_dt_from_tick_difference(Ticks t1, Ticks t2, Ticks frequency);

	// Return SYSTEMTIME representing current date and time
	SYSTEMTIME get_datetime();
}

/////////////////////////////////////////
/// Timer API
/////////////////////////////////////////

// Timer is used for simple timing purposes
struct Timer {
    Ticks frequency;
    Ticks start;
};

namespace timer {
	// Create a new timer
    Timer get();

	// Start timing
    void start(Timer *timer);

	// Return time since the start
    float end(Timer *timer);
	
	// Return the time since the start and start measuring time from now
    float checkpoint(Timer *timer);
}

#ifdef CPPLIB_PLATFORM_IMPL
#include "platform.cpp"
#endif