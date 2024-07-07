#pragma once
#include "Entry.h"

namespace Core
{
	class IChannel;

	class EntryBuilder : private Entry
	{
	public:
		EntryBuilder(const wchar_t* sourceFile, const wchar_t* sourceFunctionName, int sourceLine);
		~EntryBuilder();
		EntryBuilder& message(std::wstring message);
		EntryBuilder& level(Level);
		EntryBuilder& channel(IChannel*);
		EntryBuilder& verbose(std::wstring message = L"");
		EntryBuilder& debug(std::wstring message = L"");
		EntryBuilder& info(std::wstring message = L"");
		EntryBuilder& warn(std::wstring message = L"");
		EntryBuilder& error(std::wstring message = L"");
		EntryBuilder& fatal(std::wstring message = L"");
	private:
		IChannel* m_dest = nullptr;
	};
}
