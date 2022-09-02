#include "pch.h"
#include "CppUnitTest.h"
#include "../immutable_thread_pool/ThreadUnit.h"
#include "../immutable_thread_pool/ThreadPool.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace threadpooltests
{
	TEST_CLASS(threadpooltests)
	{
	public:
		
		TEST_METHOD(TestTUCreate)
		{
			impcool::ThreadUnit tu{};
			Assert::IsTrue(tu.CreateThread());
			Assert::IsFalse(tu.CreateThread());
			Assert::IsFalse(tu.CreateThread());
			Assert::IsFalse(tu.CreateThread());
		}

		TEST_METHOD(TestThreadPoolBasics)
		{
			using namespace std::chrono_literals;
			using namespace std::chrono;
			static constexpr std::size_t TaskCount{ 5 };
			const std::string TestName{ "TestThreadPoolBasics" };
			impcool::ThreadPool<> tp{};
			tp.CreateAll();
			tp.DestroyAll();
			tp.CreateAll();
			constexpr auto MsgFn = []()
			{
				Logger::WriteMessage("Logging from a running thread in test.\n");
				std::this_thread::sleep_for(200ms);
			};

			// Add TaskCount tasks
			for (std::size_t i{}; i < TaskCount; ++i)
			{
				tp.PushInfiniteTaskBack(MsgFn);
			}

			std::this_thread::sleep_for(2s);

			// assert num tasks is what we put in
			Assert::AreEqual(tp.GetTaskCount(), TaskCount, L"Task count not measured to be what we added!\n");

			// get non-owning pointer to internal threadunit array
			const auto* threads = tp.GetThreadList();
			// perform self count of tasks in all threads within
			std::size_t tempTaskCount{};
			for (const auto& elem : *threads)
				tempTaskCount += elem.GetTaskList().size();
			// assert self count and reported count are equal.
			Assert::AreEqual(tp.GetTaskCount(), tempTaskCount, L"Self measured task count is not equal to count reported by call to GetTaskCount()\n");

			Assert::IsFalse(tp.GetUnifiedTaskList().empty(), L"Unified task list returned was empty when it shouldn't be.\n");
			tp.DestroyAll();
			Assert::IsTrue(tp.GetUnifiedTaskList().empty(), L"Unified task list returned was not empty when it should be.\n");
		}

		TEST_METHOD(TestMutateInternalArray)
		{
			impcool::ThreadPool tp;
			auto *threadList = tp.GetThreadList();
			Assert::IsFalse(threadList->empty(), L"Default constructed threadpool returned an array of threads that was empty!");

			// For each thread unit in the array, add a task and run it once before destroying while paused.
			for(size_t i = 0; i < tp.GetThreadList()->size(); i++)
			{
				auto& firstThread = threadList->at(i);
				firstThread.PushInfiniteTaskBack([]() { Logger::WriteMessage("From task.\n"); });
				Assert::IsTrue(firstThread.GetNumberOfTasks() != 0, L"Number of tasks returned from the retrieved ThreadUnit is reported as 0!");
				firstThread.SetPauseValueOrdered(true);
				firstThread.WaitForPauseCompleted();
				Assert::IsTrue(firstThread.GetPauseCompletionStatus(), L"Ordered pause was set, and reported as completed after a wait, but not verified with GetPauseCompletionStatus()");
				firstThread.DestroyThread();
			}
			// Now try canceling the pause before requesting it again.
			for (size_t i = 0; i < tp.GetThreadList()->size(); i++)
			{
				auto& firstThread = threadList->at(i);
				firstThread.PushInfiniteTaskBack([]() { Logger::WriteMessage("From task.\n"); });
				Assert::IsTrue(firstThread.GetNumberOfTasks() != 0, L"Number of tasks returned from the retrieved ThreadUnit is reported as 0!");
				firstThread.SetPauseValueOrdered(true);
				firstThread.SetPauseValueOrdered(false);
				firstThread.SetPauseValueOrdered(true);
				firstThread.WaitForPauseCompleted();
				Assert::IsTrue(firstThread.GetPauseCompletionStatus(), L"Ordered pause was set, and reported as completed after a wait, but not verified with GetPauseCompletionStatus()");
				firstThread.DestroyThread();
			}
		}

		TEST_METHOD(TestPassingArgs)
		{
			using namespace std::chrono_literals;
			using namespace std::chrono;
			static constexpr auto TimeDelay{ 1s };
			impcool::ThreadPool tp;

			std::atomic<bool> testCondition{ false };
			tp.PushInfiniteTaskBack([&testCondition]()
				{
					testCondition = true;
					std::this_thread::sleep_for(TimeDelay);
				});
			tp.SetPauseThreadsOrdered(true);
			tp.WaitForPauseCompleted();
			tp.DestroyAll();
			Assert::IsTrue(testCondition, L"Test condition was not set to true in the thread!\n");
		}
	};
}
