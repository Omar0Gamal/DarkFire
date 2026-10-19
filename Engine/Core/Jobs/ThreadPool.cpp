#include "Engine/Core/Jobs/ThreadPool.h"
#include <algorithm>

namespace Core
{
	ThreadPool::ThreadPool(uint32_t threadCount)
	{
		if (threadCount == 0)
		{
			threadCount = std::max(1u, std::thread::hardware_concurrency());
		}

		m_workers.reserve(threadCount);
		for (uint32_t i = 0; i < threadCount; ++i)
		{
			m_workers.emplace_back([this]()
			{
				while (true)
				{
					std::function<void()> task;
					{
						std::unique_lock<std::mutex> lock(m_queueMutex);
						m_cv.wait(lock, [this]() { return m_stop || !m_tasks.empty(); });
						if (m_stop && m_tasks.empty())
						{
							return;
						}
						task = std::move(m_tasks.front());
						m_tasks.pop();
					}
					task();
				}
			});
		}
	}

	ThreadPool::~ThreadPool()
	{
		{
			std::unique_lock<std::mutex> lock(m_queueMutex);
			m_stop = true;
		}
		m_cv.notify_all();
		for (std::thread& worker : m_workers)
		{
			if (worker.joinable())
			{
				worker.join();
			}
		}
	}

	void ThreadPool::parallelFor(size_t count, const std::function<void(size_t)>& func)
	{
		if (count == 0) return;
		if (count == 1 || m_workers.empty())
		{
			for (size_t i = 0; i < count; ++i)
			{
				func(i);
			}
			return;
		}

		std::atomic<size_t> remaining{count};
		std::mutex completionMutex;
		std::condition_variable completionCv;

		for (size_t i = 0; i < count; ++i)
		{
			enqueue([&func, &remaining, &completionMutex, &completionCv, i]()
			{
				func(i);
				if (--remaining == 0)
				{
					std::lock_guard<std::mutex> lock(completionMutex);
					completionCv.notify_one();
				}
			});
		}

		std::unique_lock<std::mutex> lock(completionMutex);
		completionCv.wait(lock, [&remaining]() { return remaining.load() == 0; });
	}
}
