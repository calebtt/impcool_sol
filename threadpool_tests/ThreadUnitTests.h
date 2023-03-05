#pragma once
#include "pch.h"
#include "CppUnitTest.h"
#include <barrier>
#include <mutex>
#include <atomic>
#include <latch>
#include <memory>
#include "../immutable_thread_pool/ThreadUnitFP.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace threadpooltests
{
	TEST_CLASS(threadunittests)
	{
	public:
		using ThreadUnit_t = imp::ThreadUnitFP;
		using TaskSource_t = imp::SafeTaskSource;

		auto AddLotsOfTasks(auto& tc, const std::size_t count)
		{
			using namespace std::chrono_literals;
			for (std::size_t i = 0; i < count; i++)
			{
				const auto TaskLam = [&](const auto taskNumber) -> void
				{
					constexpr auto SleepTime{ std::chrono::milliseconds(250) };
					std::osyncstream os(std::cout);
					os << "Task with args: [" << taskNumber << "] running...\n";
					os.emit();
					std::this_thread::sleep_for(SleepTime);
				};
				tc.PushInfiniteTaskBack(TaskLam, i);
			}
		}

		TEST_METHOD(TestTUCreate)
		{
			ThreadUnit_t tu{};
			Assert::IsFalse(tu.IsWorking());
			const auto numTasks = tu.GetNumberOfTasks();
			const std::string smsg = std::format("Num Tasks: {}", numTasks);
			const std::wstring wmsg{std::begin(smsg), std::end(smsg)};
			Assert::IsTrue(numTasks == 0, wmsg.c_str() );
			Assert::IsFalse(tu.GetPauseCompletionStatus());
		}

		TEST_METHOD(TestStopPause)
		{
			static constexpr std::size_t TaskCount{ 10 };
			TaskSource_t tts{};
			ThreadUnit_t tu{};

			Assert::IsFalse(tu.IsWorking());

			// test pause completion status
			Assert::IsFalse(tu.GetPauseCompletionStatus(), L"Paused reported as completed incorrectly.");

			// test call to wait that should do nothing...
			tu.WaitForPauseCompleted();
			// test pause completion status
			Assert::IsFalse(tu.GetPauseCompletionStatus(), L"Paused reported as completed incorrectly.");

			// set both pause values
			tu.SetOrderedPause();
			tu.SetUnorderedPause();
			// wait
			tu.WaitForPauseCompleted();
			// test pause completion status
			Assert::IsTrue(tu.GetPauseCompletionStatus(), L"Paused reported as uncompleted incorrectly.");
			// setting the task source does not require pausing the thread, or destroying any thread state
			tu.SetTaskSource({});
			// test pause completion status
			Assert::IsTrue(tu.GetPauseCompletionStatus(), L"Paused reported as completed incorrectly.");
			// reset task source again, while unpaused
			tu.Unpause();
			tu.SetTaskSource({});
			Assert::IsFalse(tu.IsWorking(), L"Thread performing work when it shouldn't be.");
			AddLotsOfTasks(tts, TaskCount);
			tu.SetTaskSource(tts);
			// test task count is correct
			const auto tcount = tu.GetNumberOfTasks();
			Logger::WriteMessage(std::to_wstring(tcount).c_str());
			Assert::AreEqual(TaskCount, tcount, L"Task count doesn't match what is in the thread unit!");
			// test pause completion status
			Assert::IsFalse(tu.GetPauseCompletionStatus(), L"Paused reported as completed incorrectly.");
		}

		TEST_METHOD(TestPausing)
		{
			using namespace  std::chrono_literals;
			using Counter_t = std::size_t;
			static constexpr Counter_t TaskCount{ 10 };
			TaskSource_t tts{};

			AddLotsOfTasks(tts, TaskCount);
			
			for (Counter_t i{}; i < TaskCount; ++i)
			{
				ThreadUnit_t tu{ tts };
				Assert::IsTrue(tu.IsWorking());
				Assert::IsTrue(tu.GetNumberOfTasks() == TaskCount);
				std::this_thread::sleep_for(50ms);
				tu.SetUnorderedPause();
				tu.SetOrderedPause();
				tu.SetUnorderedPause();
				tu.SetOrderedPause();
				tu.SetOrderedPause();
				tu.Unpause();
				tu.Unpause();
				tu.SetUnorderedPause();
				std::this_thread::sleep_for(50ms);
				tu.WaitForPauseCompleted();
				Assert::IsTrue(tu.GetPauseCompletionStatus());
				tu.Unpause();
			}
		}

		// Adds a task function and asserts that it properly reports IsWorking() and pause status.
		TEST_METHOD(TestPauseStatus)
		{
			using namespace  std::chrono_literals;
			// Latch count is 2 for this thread and the created thread
			std::barrier<> bar{ 2 };
			const auto WaitForLatchFn = [&]()
			{
				// Thread confirmed running sync point here
				bar.arrive_and_wait();
				// Testing block wait here
				bar.arrive_and_wait();
			};
			{
				//syncMut.lock();
				TaskSource_t tts{ WaitForLatchFn };
				ThreadUnit_t tu{ tts };
				// Waits for barrier confirming that thread is running the task.
				bar.arrive_and_wait();
				// Asserts the correct status is returned by the thread unit while waiting for the barrier.
				Assert::IsTrue(tu.IsWorking());
				Assert::IsFalse(tu.GetPauseCompletionStatus());
				// Release the barrier before checking status.
				bar.arrive_and_wait();
				// Set a few pause states before testing again.
				tu.SetOrderedPause();
				tu.SetUnorderedPause();
				tu.SetOrderedPause();
				Assert::IsFalse(tu.IsWorking());
				std::this_thread::sleep_for(100ms);
				Assert::IsFalse(tu.IsWorking());
				Assert::IsTrue(tu.GetPauseCompletionStatus());
			}
		}
	};
}
