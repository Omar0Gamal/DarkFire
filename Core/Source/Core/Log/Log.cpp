#include "Log.h"
#include <Core/log/SeverityLevelPolicy.h>
#include <Core/log/MsvcDebugDriver.h>
#include <Core/log/TextFormatter.h>

namespace Core
{
	IChannel* GetDefaultChannel()
	{
		static std::shared_ptr<IChannel> defaultChannel = std::make_shared<Channel>();
		static bool initialized = false;
		if (!initialized)
		{
			std::shared_ptr<MsvcDebugDriver> driver = std::make_shared<MsvcDebugDriver>();
			std::shared_ptr<ITextFormatter> formatter = std::make_shared<DefaultTextFormatter>();
			driver->SetFormatter(formatter);

			defaultChannel->AttachDriver(driver);
			defaultChannel->AttachPolicy(std::make_shared<SeverityLevelPolicy>(Level::Error));
			initialized = true;
		}
		return defaultChannel.get();
	}
}