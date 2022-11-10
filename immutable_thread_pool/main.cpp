#include <iostream>
#include <string>
#include <type_traits>
#include <concepts>
#include <future>
#include <atomic>
#include <condition_variable>


//void AddLotsOfTasks(auto &tc, const std::size_t count)
//{
//	for(size_t i = 0; i < count; i++)
//	{
//		tc.PushInfiniteTaskBack([&](auto taskNumber)
//			{
//				std::osyncstream os(std::cout);
//				os << "Task with args: [" << taskNumber << "] running...\n";
//				os.emit();
//				std::this_thread::sleep_for(std::chrono::milliseconds(250));
//			}, i);
//	}
//}
//

namespace AsyncUtil
{
	using AsyncStopRequested = std::atomic<bool>;

	struct PausableAsync
	{
	public:
		//TODO make this work, a shared_ptr to just an atomic seems a bit heavy,
		//but it's probably worth it just to avoid leaking the memory accidentally,
		//plus this pausable can manage it's own lifetime now, somewhat.
		std::shared_ptr<AsyncStopRequested> stopper;
		std::shared_future<void> taskFuture;
	};

	/// <summary>
	///	Wraps a lambda with arguments, but no return value, into an argument-less std::function.
	///	The function also applies the AsyncStopRequested stop token logic, it will return immediately
	///	and perform no action if stop has been requested. The check is performed before the task is executed.
	///	</summary>
	/// <typeparam name="F"> The type of the function. </typeparam>
	/// <typeparam name="A"> The types of the arguments. </typeparam>
	///	<param name="stopToken"> Used to cancel processing. </param>
	/// <param name="taskFn"> The function to push. </param>
	/// <param name="args"> The arguments to pass to the function (by value). </param>
	template <typename F, typename... A>
	auto make_pausable_task(const AsyncStopRequested& stopToken, const F& taskFn, const A&... args)
	{
		if constexpr (sizeof...(args) == 0)
		{
			return std::function<void()>{[&stopToken, taskFn]()
			{
					if (stopToken)
						return;
					taskFn();
			} };
		}
		else
		{
			return std::function<void()>{[&stopToken, taskFn, args...]()
			{
					if (stopToken)
						return;
					taskFn(args...);
			} };
		}
	}

	/// <summary>
	///	Takes a range of pause-able tasks and returns a single std::function<void()> that calls them
	///	in succession.
	/// </summary>
	auto make_async_runnable_package(const auto& taskList)
	{
		return std::function<void()>
		{[&]()
			{
				for (const auto& elem : taskList)
				{
					elem();
				}
			}
		};
	}

	/// <summary>
	/// Starts the task running on (presumably) another thread from the pool via std::async,
	///	returns a shared_future and a pointer to the stopper object.
	/// </summary>
	/// <param name="stopToken"></param>
	/// <param name="task"></param>
	/// <param name="launchPolicy"></param>
	/// <returns></returns>
	auto start_stoppable_async(AsyncStopRequested& stopToken, const auto& task, std::launch launchPolicy = std::launch::async)
		-> PausableAsync
	{
		return PausableAsync{.stopper = &stopToken, .taskFuture = std::async(launchPolicy, task).share()};
	}
}

//InfiniteThreadPool test
int main()
{
	using namespace AsyncUtil;
	const auto taskCounterFn = [](const int n)
	{
		std::cout << "Task " << n << " running...\n";
		std::this_thread::sleep_for(std::chrono::seconds(2));
		std::cout << "Task " << n << " finished...\n";
	};

	AsyncStopRequested st{ false };
	const std::vector taskList
	{
		make_pausable_task(st, taskCounterFn, 1),
		make_pausable_task(st, taskCounterFn, 2),
		make_pausable_task(st, taskCounterFn, 3)
	};
	auto asyncPackage = make_async_runnable_package(taskList);
	auto futurePackage = start_stoppable_async(st, asyncPackage);
	std::this_thread::sleep_for(std::chrono::seconds(1));
	*futurePackage.stopper = true;

	futurePackage.taskFuture.wait();

	std::cout << "\nEnter to exit...\n\n";
	std::string buffer;
	std::getline(std::cin, buffer);
}