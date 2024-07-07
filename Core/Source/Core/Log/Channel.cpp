#include "Channel.h"
#include "Driver.h"
#include "Policy.h"

namespace Core
{
	Channel::Channel(std::vector<std::shared_ptr<IDriver>> drivers)
		:m_drivers{ std::move(drivers) }
	{
	}

	void Channel::Submit(Entry& entry)
	{
		for (auto& policy : m_policies)
		{
			if (!policy->CanSubmit(entry))
			{
				return;
			}
		}
		for (auto& driver : m_drivers)
		{
			driver->Submit(entry);
		}
	}

	void Channel::AttachDriver(std::shared_ptr<IDriver> driver)
	{
		m_drivers.push_back(std::move(driver));
	}
	void Channel::AttachPolicy(std::shared_ptr<IPolicy> policy)
	{
		m_policies.push_back(std::move(policy));
	}
}