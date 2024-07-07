#pragma once
#include "Policy.h"
#include "Level.h"

namespace Core
{
	class SeverityLevelPolicy : public IPolicy
	{
	public:
		SeverityLevelPolicy(Level level);
	public:
		bool CanSubmit(Entry&) override;
	private:
		Level m_level;
	};
}

