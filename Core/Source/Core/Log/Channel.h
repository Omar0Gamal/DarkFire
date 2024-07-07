#pragma once
#include <memory>
#include <vector>

namespace Core
{
	struct Entry;
	class IDriver;
	class IPolicy;

	class IChannel
	{
	public:
		virtual void Submit(Entry&) = 0;
		virtual ~IChannel() = default;
	public:
		virtual void AttachDriver(std::shared_ptr<IDriver>) = 0;
		virtual void AttachPolicy(std::shared_ptr<IPolicy>) = 0;
	};

	class Channel : public IChannel
	{
	public:
		Channel(std::vector<std::shared_ptr<IDriver>> drivers = {});
	public:
		void Submit(Entry&) override;
		void AttachDriver(std::shared_ptr<IDriver>) override;
		void AttachPolicy(std::shared_ptr<IPolicy>) override;
	private:
		std::vector<std::shared_ptr<IDriver>> m_drivers;
		std::vector<std::shared_ptr<IPolicy>> m_policies;
	};
}
