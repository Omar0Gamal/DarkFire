#pragma once
#include <string>

namespace Core
{
	struct Entry;

	class ITextFormatter
	{
	public:
		virtual std::wstring Format(const Entry&) const = 0;
		virtual ~ITextFormatter() = default;
	};

	class DefaultTextFormatter : public ITextFormatter
	{
	public:
		std::wstring Format(const Entry&) const override;
	};
}