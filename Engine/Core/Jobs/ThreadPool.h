#pragma once

#include "Engine/Core/Defines.h"
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <cstdint>

namespace Core
{
	class DF_API ThreadPool
	{
	public:
		explicit ThreadPool(uint32_t threadCount = 0);
		~ThreadPool();

		ThreadPool(const ThreadPool&) = delete;
		ThreadPool& operator=(const ThreadPool&) = delete;

		template<typename F, typename... Args>
		auto enqueue(F&& f, Args&&... args) 
			-> std::future<typename std::invoke_result<F, Args...>::type>
		{
			using return_type = typename std::invoke_result<F, Args...>::type;
			auto task = std::make_shared<std::packaged_task<return_type()>>(
				std::bind(std::forward<F>(f), std::forward<Args>(args)...)
			);
			std::future<return_type> res = task->get_future();
			{
				std::unique_lock<std::mutex> lock(m_queueMutex);
				if (m_stop)
				{
					return std::future<return_type>{};
				}
				m_tasks.emplace([task]() { (*task)(); });
			}
			m_cv.notify_one();
			return res;
		}

		void parallelFor(size_t count, const std::function<void(size_t)>& func);

		uint32_t getThreadCount() const { return static_cast<uint32_t>(m_workers.size()); }

	private:
		std::vector<std::thread> m_workers;
		std::queue<std::function<void()>> m_tasks;
		std::mutex m_queueMutex;
		std::condition_variable m_cv;
		std::atomic<bool> m_stop{false};
	};
}
