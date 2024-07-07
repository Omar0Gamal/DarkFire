#define _CRT_SECURE_NO_WARNINGS
#include "StackTrace.h"
#include <backward.hpp>
#include <Core/util/String.h>
#include <sstream>
namespace Core
{
	StackTrace::StackTrace()
	{
		backward::TraceResolver thisIsAWorkaround;
		m_stackTrace = std::make_unique<backward::StackTrace>();
		m_stackTrace->load_here(64);
		m_stackTrace->skip_n_firsts(6);
	}

	StackTrace::StackTrace(const StackTrace& src)
		: m_stackTrace(std::make_unique<backward::StackTrace>(*src.m_stackTrace))
	{
	}

	StackTrace& StackTrace::operator=(const StackTrace& src)
	{
		m_stackTrace = std::make_unique<backward::StackTrace>(*src.m_stackTrace);
		return *this;
	}

	StackTrace::~StackTrace() {}
	std::wstring StackTrace::print() const
	{
		if (m_stackTrace)
		{
			std::ostringstream oss;
			backward::Printer printer;
			printer.print(*m_stackTrace, oss);
			return ToWide(oss.str());
		}else {
			return L"== EMPTY TRACE ==";
		}
	}
}