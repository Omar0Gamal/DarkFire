#include "EntryBuilder.h"
#include "Channel.h"

namespace Core
{
	EntryBuilder::EntryBuilder(const wchar_t* sourceFile, const wchar_t* sourceFunctionName, int sourceLine)
		:Entry{ 
			.m_level = Level::Error,
			.m_sourceFile = sourceFile,
			.m_sourceFunctionName = sourceFunctionName,
			.m_sourceLine = sourceLine,
			.m_timeStamp = std::chrono::system_clock::now()
		}
	{}

	EntryBuilder::~EntryBuilder()
	{
		if (m_dest)
		{
			if ((int)m_level <= (int)Level::Error)
			{
				m_stackTrace.emplace();
			}
			m_dest->Submit(*this);
		}
	}

	EntryBuilder& EntryBuilder::message(std::wstring message)
	{
		m_message = std::move(message);
		return *this;
	}

	EntryBuilder& EntryBuilder::level(Level level)
	{
		m_level = level;
		return *this;
	}

	EntryBuilder& EntryBuilder::channel(IChannel* dest)
	{
		m_dest = dest;
		return *this;
	}

	EntryBuilder& EntryBuilder::verbose(std::wstring message)
	{
		m_message = std::move(message);
		m_level = Level::Verbose;
		return *this;
	}

	EntryBuilder& EntryBuilder::debug(std::wstring message)
	{
		m_message = std::move(message);
		m_level = Level::Debug;
		return *this;
	}

	EntryBuilder& EntryBuilder::info(std::wstring message)
	{
		m_message = std::move(message);
		m_level = Level::Info;
		return *this;
	}

	EntryBuilder& EntryBuilder::warn(std::wstring message)
	{
		m_message = std::move(message);
		m_level = Level::Warn;
		return *this;
	}

	EntryBuilder& EntryBuilder::error(std::wstring message)
	{
		m_message = std::move(message);
		m_level = Level::Error;
		return *this;
	}

	EntryBuilder& EntryBuilder::fatal(std::wstring message)
	{
		m_message = std::move(message);
		m_level = Level::Fatal;
		return *this;
	}
}
