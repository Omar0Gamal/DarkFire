#pragma once
#include "Channel.h"
#include "EntryBuilder.h"

namespace Core
{
	IChannel* GetDefaultChannel();
}

#define darklog EntryBuilder(__FILEW__, __FUNCTIONW__ ,__LINE__).channel(Core::GetDefaultChannel())