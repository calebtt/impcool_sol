#include <iostream>
#include <string>
#include <cassert>

#include "ThreadUnitFP.h"
#include "BoolCvPack.h"

void AddLotsOfTasks(auto &tc, const std::size_t count)
{
	for(std::size_t i = 0; i < count; ++i)
	{
		tc.PushInfiniteTaskBack([&](auto taskNumber)
			{
				std::osyncstream os(std::cout);
				os << "Task with args: [" << taskNumber << "] running...\n";
				os.emit();
				std::this_thread::sleep_for(std::chrono::milliseconds(250));
			}, i);
	}
}

// Test of the ThreadUnitFP "new paradigm"
void TestThreadPP()
{
	using ThreadUnit_t = imp::ThreadUnitFP;
	using TaskSource_t = imp::ThreadTaskSource;
	// here we begin by creating a ThreadTaskSource object, it holds a range of std::function tasks.
	// and it provides the operations needed to add objects to them, namely synonyms for push_back and push_front
	// it also handles passing arguments into the task function.
	// It essentially copies the object into a wrapper std::function around the original (user provided) lambda std::function, where the wrapper function
	// is able to keep alive say, a shared_ptr, via the type erasure feature of the std::function. So you can pass in a
	// *capturing* lambda and have the task keep the data alive while used! For the example here, I will create
	// a shared_ptr to an osyncstream for synchronizing multi-threaded use of cout.

	std::shared_ptr<std::osyncstream> osp = std::make_shared<std::osyncstream>(std::cout);

	// Construct a task source object, it provides the functions for adding the lambda as a no-argument non-capturing lambda (which wraps the user provided).
	TaskSource_t tts;

	// Also note that a lambda function actually expands to a callable struct with the capture-clause items as data members.
	// So the lambda below might look like (see below) and then the std::function's type-erasure will keep alive the lambda
	// data members (including the std::shared_ptr). But they *MUST* be captured by value for that to work!

	//struct AnonymousLambda
	//{
	//	std::shared_ptr<std::osyncstream> osp;
	//	void operator()()
	//	{
	//		*osp << "A ThreadUnitPlusPlus task is running...\n";
	//		osp->emit();
	//		std::this_thread::sleep_for(std::chrono::seconds(1));
	//	}
	//};


	// Push the capturing lambda.
	tts.PushInfiniteTaskBack([outStream = osp]()
	{
		using namespace std::chrono_literals;
		const auto tname = typeid(ThreadUnit_t).name();
		*outStream << "A "<< tname << " task is running...\n";
		outStream->emit();
		std::this_thread::sleep_for(1s);
	});

	*osp << "Press Enter to stop the test.\n";
	// Construct the thread unit object, pass our constructed task source object.
	ThreadUnit_t tupp(tts);

	// Let the thread run the task until 'enter' is pressed.
	std::string buffer;
	std::getline(std::cin, buffer);
	// Test resetting the task buffer.
	tupp.SetTaskSource({});
}

void TestBoolCvPackCopying()
{
	using Bcp_t = imp::BoolCvPack;
	// Runs through all the copy/move ctors for BoolCvPack
	auto getPack = []() -> auto
	{
		return std::move(Bcp_t{});
	};
	Bcp_t bcp;
	Bcp_t bcpOther;
	bcp.UpdateState(true);

	// Test operator-assign
	bcpOther = bcp;
	assert(bcp.GetState());
	assert(bcpOther.GetState());
	
	// Test move-assign
	bcp = getPack();
	assert(!bcp.GetState());

	// Test copy-construct
	Bcp_t crvRef{ bcpOther };

	// Test move-construct
	Bcp_t rvRef{ std::move(crvRef)};
	assert(rvRef.GetState());
}

void TestThreadUnitMoving()
{
	using ThreadUnit_t = imp::ThreadUnitFP;
	// Construct a thread unit running a single task that does nothing
	const auto sleepLam = []() { std::this_thread::sleep_for(std::chrono::seconds(1)); };
	ThreadUnit_t tupp{ {sleepLam} };

	// move the thread unit into a newly created instance
	// Test move-construct
	ThreadUnit_t tupp2{ std::move(tupp) };
	// assert the moved thread is running.
	assert(tupp2.IsWorking());

	// test the move-assign constructor
	ThreadUnit_t tupp3;
	tupp3 = std::move(tupp2);
	assert(tupp3.IsWorking());
}

//InfiniteThreadPool test
int main()
{
	//multi-threaded output obj
	std::osyncstream os(std::cout);
	const auto MultiPrint = [&](const auto message)
	{
		os << message;
		os.emit();
	};
	//test thread unit
	TestThreadPP();

	//test copying/moving of BoolCvPack
	TestBoolCvPackCopying();

	//test thread unit std::moving
	TestThreadUnitMoving();

	MultiPrint("\nEnter to exit...\n\n");
	std::string buffer;
	std::getline(std::cin, buffer);
}