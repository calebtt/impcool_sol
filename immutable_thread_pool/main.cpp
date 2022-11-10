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
/// <summary> Using the AsyncTaskSource with std::async allows to push a collection of
///	task functions into the same std::async call (thread typically), with the Windows
///	implementation limiting the number of threads to a pool of a few threads.
/// 
/// </summary>
//class AsyncTaskSource
//{
//public:
//	using TaskInfo = std::function<void()>;
//	using TaskDeque = std::deque<TaskInfo>;
//private:
//	struct ThreadConditionals
//	{
//		imp::BoolCvPack OrderedPausePack;
//		imp::BoolCvPack UnorderedPausePack;
//		imp::BoolCvPack PauseCompletedPack;
//		//BoolCvPack isStopRequested;
//		// Waits for both pause requests to be false.
//		void WaitForBothPauseRequestsFalse()
//		{
//			OrderedPausePack.WaitForFalse();
//			UnorderedPausePack.WaitForFalse();
//		}
//		void Notify()
//		{
//			OrderedPausePack.task_running_cv.notify_all();
//			UnorderedPausePack.task_running_cv.notify_all();
//			PauseCompletedPack.task_running_cv.notify_all();
//		}
//		void SetStopSource(const std::stop_source sts)
//		{
//			PauseCompletedPack.stop_source = sts;
//			OrderedPausePack.stop_source = sts;
//			UnorderedPausePack.stop_source = sts;
//		}
//	};
//public:
//	/// <summary> Public data member, allows direct access to the task source. </summary>
//	TaskDeque TaskList{};
//private:
//	ThreadConditionals m_conditionalsPack;
//public:
//	/// <summary> Push a function with zero or more arguments, but no return value, into the task list. </summary>
//	/// <typeparam name="F"> The type of the function. </typeparam>
//	/// <typeparam name="A"> The types of the arguments. </typeparam>
//	/// <param name="taskFn"> The function to push. </param>
//	/// <param name="args"> The arguments to pass to the function (by value). </param>
//	template <typename F, typename... A>
//	void PushInfiniteTaskBack(const F& taskFn, const A&... args)
//	{
//		if constexpr (sizeof...(args) == 0)
//		{
//			TaskList.emplace_back(TaskInfo{ taskFn });
//		}
//		else
//		{
//			TaskList.emplace_back(TaskInfo([taskFn, args...] { taskFn(args...); }));
//		}
//	}
//private:
//	/// <summary> The worker function, on the std::async thread. </summary>
//	/// <param name="stopToken"> Passed in the std::jthread automatically at creation. </param>
//	/// <param name="tasks"> List of tasks copied into this worker function, it is not mutated in-use. </param>
//	void threadPoolFunc(const std::stop_token stopToken, const imp::ThreadTaskSource::TaskRange tasks, ThreadConditionals& m_conditionalsPack)
//	{
//		// Constant used to store the loop delay time period when no tasks are present.
//		static constexpr std::chrono::milliseconds EmptyWaitTime{ std::chrono::milliseconds(20) };
//
//		const auto TestAndWaitForPauseEither = [](ThreadConditionals& pauseObj)
//		{
//			// If either ordered or unordered pause set
//			if (pauseObj.OrderedPausePack.GetState() || pauseObj.UnorderedPausePack.GetState())
//			{
//				// Set pause completion event, which sends the notify
//				pauseObj.PauseCompletedPack.UpdateState(true);
//				// Wait until the pause state is toggled back to false (both)
//				pauseObj.WaitForBothPauseRequestsFalse();
//				// Reset the pause completed state and continue
//				pauseObj.PauseCompletedPack.UpdateState(false);
//			}
//		};
//		const auto TestAndWaitForPauseUnordered = [](ThreadConditionals& pauseObj)
//		{
//			// If either ordered or unordered pause set
//			if (pauseObj.UnorderedPausePack.GetState())
//			{
//				// Set pause completion event, which sends the notify
//				pauseObj.PauseCompletedPack.UpdateState(true);
//				// Wait until the pause state is toggled back to false (both)
//				pauseObj.WaitForBothPauseRequestsFalse();
//				// Reset the pause completed state and continue
//				pauseObj.UnorderedPausePack.UpdateState(false);
//			}
//		};
//
//		// While not is stop requested.
//		while (!stopToken.stop_requested())
//		{
//			//test for ordered pause
//			TestAndWaitForPauseEither(m_conditionalsPack);
//
//			if (tasks.empty())
//			{
//				std::this_thread::sleep_for(EmptyWaitTime);
//			}
//			// Iterate task list, running tasks set for this thread.
//			for (const auto& currentTask : tasks)
//			{
//				//test for unordered pause request (before the fn call!)
//				TestAndWaitForPauseUnordered(m_conditionalsPack);
//				//double check outer condition here, as this may be long-running,
//				//causes destruction to occur unordered.
//				if (stopToken.stop_requested())
//					break;
//				// run the task
//				currentTask();
//			}
//		}
//	}
//};



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