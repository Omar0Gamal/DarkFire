#pragma once
#include <string>
#include <memory>

namespace backward
{
	class StackTrace;
}

namespace Core
{
	class StackTrace
	{
	public:
		StackTrace();
		StackTrace(const StackTrace& src);
		StackTrace& operator=(const StackTrace& src);
		~StackTrace();
	public:
		std::wstring print() const;
	private:
		std::unique_ptr<backward::StackTrace> m_stackTrace;
	};
}

