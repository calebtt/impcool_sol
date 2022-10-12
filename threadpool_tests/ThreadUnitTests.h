#pragma once
#include "pch.h"
#include "CppUnitTest.h"
#include "../immutable_thread_pool/ThreadUnitPlusPlus.h"
#include "../immutable_thread_pool/ThreadPooler.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace threadpooltests
{
	TEST_CLASS(threadunittests)
	{
	public:

		TEST_METHOD(TestTUCreate)
		{
			impcool::ThreadUnitPlusPlus tu{};
		}

		TEST_METHOD(TestStartStopPause)
		{
			static constexpr std::size_t TaskCount{ 10 };
			impcool::ThreadUnitPlusPlus tu{};
			const auto AddLotsOfTasks = [](auto& tc, const std::size_t count)
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
					tc.TaskList.PushInfiniteTaskBack(TaskLam, i);
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
