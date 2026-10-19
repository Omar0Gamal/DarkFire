#include "Engine/Runtime/EngineContext.h"

namespace Core
{
	EngineContext& EngineContext::Get()
	{
		static VirtualFileSystem s_vfs;
		static ThreadPool s_threadPool;
		static Random s_random;
		static FrameArena s_frameArena;

		static EngineContext s_context{
			Log::Get(),
			s_vfs,
			s_threadPool,
			s_random,
			s_frameArena
		};
		return s_context;
	}
}
