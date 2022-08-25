#include "pch.h"
#include "CppUnitTest.h"
#include "../immutable_thread_pool/ThreadUnit.h"

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
	};
}
