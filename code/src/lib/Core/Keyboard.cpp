#include "Keyboard.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace core
{
	namespace keyboard
	{
#ifdef __linux__
		bool poll_key(Key key)
		{
			return false;
		}
#elif _WIN32
		bool poll_key(Key key)
		{
			switch (key)
			{
			case Key::Up:
				return (GetAsyncKeyState(VK_UP) & (1 << 15)) != 0;
			case Key::Down:
				return (GetAsyncKeyState(VK_DOWN) & (1 << 15)) != 0;
			case Key::Left:
				return (GetAsyncKeyState(VK_LEFT) & (1 << 15)) != 0;
			case Key::Right:
				return (GetAsyncKeyState(VK_RIGHT) & (1 << 15)) != 0;
			case Key::Return:
				return (GetAsyncKeyState(VK_RETURN) & (1 << 15)) != 0;
			default:
				return false;
			}
		}
#endif
	}
}
