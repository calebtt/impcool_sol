#include "pch.h"
#include "CppUnitTest.h"
#include "ThreadUnitTests.h"
#include "../immutable_thread_pool/ThreadUnitPlusPlus.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace threadpooltests
{
	TEST_CLASS(stub)
	{
	public:
		TEST_METHOD(stubfunc)
		{
			Assert::IsTrue(true);
		}
	};
	TEST_CLASS(threadpooltests)
	{
	public:
	};
}
