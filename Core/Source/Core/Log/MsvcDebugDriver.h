#pragma once
#include "Driver.h"
#include <memory>

namespace Core
{
	class MsvcDebugDriver : public ITextDriver
	{
	public:
		MsvcDebugDriver(std::shared_ptr<ITextFormatter> formatter = {});
		void Submit(const Entry&) override;
		void SetFormatter(std::shared_ptr<ITextFormatter>) override;
	private:
		std::shared_ptr<ITextFormatter> m_formatter;
	};
}