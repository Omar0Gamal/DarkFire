#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Log/Log.h"
#include "Engine/Core/VFS/VirtualFileSystem.h"
#include "Engine/Core/Jobs/ThreadPool.h"
#include "Engine/Core/Random.h"
#include "Engine/Core/Memory/FrameArena.h"

namespace Core
{
	struct DF_API EngineContext
	{
		Log& log;
		VirtualFileSystem& vfs;
		ThreadPool& threadPool;
		Random& random;
		FrameArena& frameArena;

		static EngineContext& Get();
	};

	struct DF_API EditorContext
	{
		void* imGuiContext = nullptr;
		void* (*allocFunc)(size_t sz, void* user_data) = nullptr;
		void (*freeFunc)(void* ptr, void* user_data) = nullptr;
		void* userData = nullptr;
	};
}
