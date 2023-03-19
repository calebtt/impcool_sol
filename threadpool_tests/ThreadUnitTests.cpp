#include "pch.h"
#include "CppUnitTest.h"
#include "../immutable_thread_pool/ThreadUnitPlusPlus.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace threadpooltests
{
	TEST_CLASS(threadunittests)
	{
	public:

		TEST_METHOD(TestTUCreate)
		{
			imp::ThreadUnitPlusPlus tu{};
		}

		TEST_METHOD(TestStopPause)
		{
			static constexpr std::size_t TaskCount{ 10 };
			imp::ThreadTaskSource tts{};
			imp::ThreadUnitPlusPlus tu{};
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
					tc.PushInfiniteTaskBack(TaskLam, i);
				}
			};

			Assert::IsTrue(tu.IsRunning());

			// test pause completion status
			Assert::IsFalse(tu.GetPauseCompletionStatus(), L"Paused reported as completed incorrectly.");

			// test call to wait that should do nothing...
			tu.WaitForPauseCompleted();
			// test pause completion status
			Assert::IsFalse(tu.GetPauseCompletionStatus(), L"Paused reported as completed incorrectly.");

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
			tu.SetTaskSource({});
			Assert::IsTrue(tu.IsRunning(), L"Thread not created when it should be.");
			AddLotsOfTasks(tts, TaskCount);
			tu.SetTaskSource(tts);
			// test task count is correct
			Assert::AreEqual(TaskCount, tu.GetNumberOfTasks(), L"Task count doesn't match what is in the thread unit!");
			// test pause completion status
			Assert::IsFalse(tu.GetPauseCompletionStatus(), L"Paused reported as completed incorrectly.");
		}
	};
}
