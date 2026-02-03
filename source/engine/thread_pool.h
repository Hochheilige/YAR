#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>

class ThreadPool
{
public:
	explicit ThreadPool(size_t thread_count = std::thread::hardware_concurrency())
		: stop_flag(false)
	{
		if (thread_count == 0)
			thread_count = 1;

		workers.reserve(thread_count);
		for (size_t i = 0; i < thread_count; ++i)
		{
			workers.emplace_back([this] { worker_loop(); });
		}
	}

	~ThreadPool()
	{
		{
			std::lock_guard<std::mutex> lock(queue_mutex);
			stop_flag = true;
		}
		condition.notify_all();

		for (auto& worker : workers)
		{
			if (worker.joinable())
				worker.join();
		}
	}

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
	ThreadPool(ThreadPool&&) = delete;
	ThreadPool& operator=(ThreadPool&&) = delete;

	template<typename F, typename... Args>
	auto submit(F&& func, Args&&... args) -> std::shared_future<std::invoke_result_t<F, Args...>>
	{
		using return_type = std::invoke_result_t<F, Args...>;

		auto task = std::make_shared<std::packaged_task<return_type()>>(
			std::bind(std::forward<F>(func), std::forward<Args>(args)...)
		);

		std::shared_future<return_type> result = task->get_future().share();

		{
			std::lock_guard<std::mutex> lock(queue_mutex);
			if (stop_flag)
				return result; // pool is shutting down, task won't execute

			tasks.emplace([task]() { (*task)(); });
		}

		condition.notify_one();
		return result;
	}

	size_t thread_count() const
	{
		return workers.size();
	}

private:
	void worker_loop()
	{
		while (true)
		{
			std::function<void()> task;
			{
				std::unique_lock<std::mutex> lock(queue_mutex);
				condition.wait(lock, [this] { return stop_flag || !tasks.empty(); });

				if (stop_flag && tasks.empty())
					return;

				task = std::move(tasks.front());
				tasks.pop();
			}

			task();
		}
	}

	std::vector<std::thread> workers;
	std::queue<std::function<void()>> tasks;
	std::mutex queue_mutex;
	std::condition_variable condition;
	bool stop_flag;
};
