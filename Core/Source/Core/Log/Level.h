#pragma once
#include <string>

namespace Core 
{
	enum class Level 
	{
		None,
		Fatal,
		Error,
		Warn,
		Info,
		Debug,
		Verbose
	};

	std::wstring GetLevelName(Level);
}

