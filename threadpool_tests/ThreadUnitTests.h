#pragma once
#include "pch.h"
#include "CppUnitTest.h"
#include "../immutable_thread_pool/ThreadUnitPlus.h"
#include "../immutable_thread_pool/ThreadPool.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace threadpooltests
{
	TEST_CLASS(threadunittests)
	{
	public:

		TEST_METHOD(TestTUCreate)
		{
			impcool::ThreadUnitPlus tu{};
			Assert::IsTrue(tu.CreateThread());
			Assert::IsFalse(tu.CreateThread());
			Assert::IsFalse(tu.CreateThread());
			Assert::IsFalse(tu.CreateThread());
		}

		TEST_METHOD(TestStartStopPause)
		{
			static constexpr std::size_t TaskCount{ 10 };
			impcool::ThreadUnitPlus tu{};
			const auto AddLotsOfTasks = [](auto& tc, const std::size_t count)
			{
				using namespace std::chrono_literals;
				static constexpr auto SleepTime{ 250ms };
				for (std::size_t i = 0; i < count; i++)
				{
					tc.PushInfiniteTaskBack([&](auto taskNumber)
						{
							std::osyncstream os(std::cout);
							os << "Task with args: [" << taskNumber << "] running...\n";
							os.emit();
							std::this_thread::sleep_for(SleepTime);
						}, i);
				}
			};

			Assert::IsTrue(tu.CreateThread());

			// test pause completion status
			Assert::IsFalse(tu.GetPauseCompletionStatus(), L"Paused reported as completed incorrectly.");

			// test call to wait that should do nothing...
			tu.WaitForPauseCompleted();
			// set both pause values
			tu.SetPauseValueOrdered(true);
			tu.SetPauseValueUnordered(true);
			// wait
			tu.WaitForPauseCompleted();
			// test pause completion status
			Assert::IsTrue(tu.GetPauseCompletionStatus(), L"Paused reported as uncompleted incorrectly.");
			// destroy while paused...
			tu.DestroyThread();
			// test pause completion status
			Assert::IsFalse(tu.GetPauseCompletionStatus(), L"Paused reported as completed incorrectly.");
			// destroy again
			tu.DestroyThread();
			// create a new thread
			Assert::IsTrue(tu.CreateThread(), L"Thread not created when it should be.");
			// test create while created
			Assert::IsFalse(tu.CreateThread(), L"Thread created when it shouldn't be!");
			AddLotsOfTasks(tu, TaskCount);
			// test create while created and has task
			Assert::IsFalse(tu.CreateThread(), L"Thread created when it shouldn't be!");
			// test task count is correct
			const auto taskCount = tu.GetNumberOfTasks();
			Assert::IsTrue(taskCount == TaskCount, L"Task count not correct!");
			// test pause completion status
			Assert::IsFalse(tu.GetPauseCompletionStatus(), L"Paused reported as completed incorrectly.");
		}
	};
}
