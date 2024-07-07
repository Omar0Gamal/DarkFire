#include "SeverityLevelPolicy.h"
#include "Entry.h"

namespace Core
{
	SeverityLevelPolicy::SeverityLevelPolicy(Level level)
		:m_level{ level }
	{
	}

	bool SeverityLevelPolicy::CanSubmit(Entry& entry)
	{
		return entry.m_level <= m_level;
	}
}
