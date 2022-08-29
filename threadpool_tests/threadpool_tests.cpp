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
			tp.DestroyAll();
		}
	};
}
