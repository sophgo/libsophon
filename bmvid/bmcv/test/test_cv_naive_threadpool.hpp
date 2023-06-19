#include <iostream>
#include <thread>
#include <functional>
#include <vector>
#include <queue>
#include <mutex>
#include <memory>
#include <atomic>
#include <random>
#include<map>
/*
	std::default_random_engine e(seed);
	std::uniform_int_distribution<unsigned char> u(0, 255);
	std::uniform_int_distribution<unsigned> s(0, 1024);
*/


class ThreadPool
{
public:
    ThreadPool(int thread_num = std::thread::hardware_concurrency());

	template <typename F, typename... Args>
	void enqueue(F &&f, Args &&... args);
	void wait_all_done();
	void srand(u64 seed);
	std::shared_ptr<std::default_random_engine> get_random_engine();
	~ThreadPool();

private:
	int threads_num;
	std::vector<std::thread >workers;
	std::map<std::thread::id, std::shared_ptr<std::default_random_engine>> random_engines;
	std::queue<std::function<void()>> tasks;
	std::atomic<unsigned int> tast_on_going;
	std::mutex queue_mutex;
	bool stop;
};



void ThreadPool::srand(u64 seed)
{
	random_engines.clear();
	random_engines[std::this_thread::get_id()] = std::make_shared<std::default_random_engine>(seed);
	auto main_thread_engine = random_engines[std::this_thread::get_id()];
	for (int i = 0; i < threads_num; i++)
	{
		int local_seed = (*main_thread_engine)();
		random_engines[workers[i].get_id()] = std::make_shared<std::default_random_engine>(local_seed);
		std::cout << "On thread " << workers[i].get_id() << " the seed is " << local_seed << std::endl;
	}
}

std::shared_ptr<std::default_random_engine> ThreadPool::get_random_engine()
{
	return random_engines[std::this_thread::get_id()];
}

inline ThreadPool::ThreadPool(int thread_num) : stop(false)
{
	this->threads_num = thread_num;
	workers.reserve(threads_num);
	tast_on_going = 0;
	
	for (int i = 0; i < threads_num; ++i)
	{
		workers.emplace_back([this]() {
			while (!stop)
			{
				std::function<void()> task;
				{
					std::unique_lock<std::mutex> lock(this->queue_mutex);
					if (this->tasks.empty())
						continue;
					task = std::move(this->tasks.front());
					this->tasks.pop();
					tast_on_going++;
				}
				task();
				tast_on_going--;
			}
		});
	}
	this->srand(0);
}

template<class F, class... Args>
void ThreadPool::enqueue(F&& f, Args&&... args)
{
	auto task = std::make_shared<std::function<void()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
	{
        std::unique_lock<std::mutex> lock(queue_mutex);
        tasks.emplace([task](){ (*task)(); });
    }
}


void ThreadPool::wait_all_done()
{
	while(true)
	{
		{
			std::unique_lock<std::mutex> lock(queue_mutex);
			if(tasks.empty() && tast_on_going == 0)
			{
				return;
			}
		}
		std::this_thread::yield();
	}
}

inline ThreadPool::~ThreadPool()
{
	stop = true;

	for (int i = 0; i < threads_num; i++)
	{
		if(workers[i].joinable())
			workers[i].join();
	}
}
