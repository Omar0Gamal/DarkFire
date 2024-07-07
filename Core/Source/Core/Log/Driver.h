#pragma once
#include <memory>

namespace Core
{
	struct Entry;
	class ITextFormatter;

	class IDriver
	{
	public:
		virtual void Submit(const Entry&) = 0;
		virtual ~IDriver() = default;
	};

	class ITextDriver : public IDriver
	{
	public:
		virtual void SetFormatter(std::shared_ptr<ITextFormatter> ) = 0;
	};
}
