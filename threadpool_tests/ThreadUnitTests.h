#pragma once
#include "pch.h"
#include "CppUnitTest.h"
#include "../immutable_thread_pool/ThreadUnitFP.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace threadpooltests
{
	TEST_CLASS(threadunittests)
	{
	public:
		using ThreadUnit_t = imp::ThreadUnitFP;
		using TaskSource_t = imp::ThreadTaskSource;

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
			Assert::IsTrue(tu.IsRunning());
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

			Assert::IsTrue(tu.IsRunning());

			// test pause completion status
			Assert::IsFalse(tu.GetPauseCompletionStatus(), L"Paused reported as completed incorrectly.");

			// test call to wait that should do nothing...
			tu.WaitForPauseCompleted();
			// test pause completion status
			Assert::IsFalse(tu.GetPauseCompletionStatus(), L"Paused reported as completed incorrectly.");

			// set both pause values
			tu.SetPauseValue(imp::pause::OrderedPause_t{});
			tu.SetPauseValue(imp::pause::UnorderedPause_t{});
			// wait
			tu.WaitForPauseCompleted();
			// test pause completion status
			Assert::IsTrue(tu.GetPauseCompletionStatus(), L"Paused reported as uncompleted incorrectly.");
			// destroy while paused...
			//tu.DestroyThread();
			// test pause completion status
			Assert::IsFalse(tu.GetPauseCompletionStatus(), L"Paused reported as completed incorrectly.");
			// destroy again
			//tu.DestroyThread();
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

		TEST_METHOD(TestPausing)
		{
			using namespace  std::chrono_literals;
			static constexpr std::size_t TaskCount{ 10 };
			TaskSource_t tts{};

			AddLotsOfTasks(tts, TaskCount);

			ThreadUnit_t tu{ tts };
			for (int i = 0; i < TaskCount; ++i) {
				Assert::IsTrue(tu.IsRunning());
				Assert::IsTrue(tu.GetNumberOfTasks() == TaskCount);
				std::this_thread::sleep_for(500ms);
				tu.SetPauseValue(imp::pause::OrderedPause_t{});
				std::this_thread::sleep_for(500ms);
				tu.WaitForPauseCompleted();
				Assert::IsTrue(tu.GetPauseCompletionStatus());
				tu.SetPauseValue(imp::pause::NeitherPause_t{});
				//tu.DestroyThread();
			}
		}
	};
}
