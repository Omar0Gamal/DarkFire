#pragma once

#include <string>
#include <string_view>
#include <cstddef>

namespace Editor
{
	class ICommand
	{
	public:
		virtual ~ICommand() = default;

		virtual void execute() = 0;
		virtual void undo() = 0;
		virtual size_t getMemorySize() const = 0;
		virtual std::string_view getName() const = 0;

		virtual bool mergeWith(const ICommand& other)
		{
			(void)other;
			return false;
		}
	};
}
