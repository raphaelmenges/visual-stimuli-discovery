#pragma once

namespace core
{
	namespace keyboard
	{
		enum class Key { Up, Down, Left, Right, Return};

		bool poll_key(Key key);
	}
}
