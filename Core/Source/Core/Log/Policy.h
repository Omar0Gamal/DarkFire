#pragma once

namespace Core
{
	struct Entry;

	class IPolicy
	{
	public:
		virtual ~IPolicy() = default;
	public:
		virtual bool CanSubmit(Entry&) = 0;
	};
}