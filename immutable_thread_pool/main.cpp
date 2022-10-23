#include <iostream>
#include <string>

#include "ThreadPooler.h"
#include "ThreadUnitPlusPlus.h"
#include "BoolCvPack.h"

void AddLotsOfTasks(auto &tc, const std::size_t count)
{
	for(size_t i = 0; i < count; i++)
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

// Test of the ThreadUnitPlusPlus new paradigm
void TestThreadPP()
{
	// here we begin by creating a ThreadTaskSource object, it holds a range of std::function tasks.
	// and it provides the operations needed to add objects to them, namely synonyms for push_back and push_front
	// it also handles passing arguments into the task function.
	// It essentially copies the object into a wrapper std::function around the original (user provided) lambda std::function, where the wrapper function
	// is able to keep alive say, a shared_ptr, via the type erasure feature of the std::function. So you can pass in a
	// *capturing* lambda and have the task keep the data alive while used! For the example here, I will create
	// a shared_ptr to an osyncstream for synchronizing multi-threaded use of cout.

	std::shared_ptr<std::osyncstream> osp = std::make_shared<std::osyncstream>(std::cout);

	// Construct a task source object, it provides the functions for adding the lambda as a no-argument non-capturing lambda (which wraps the user provided).
	imp::ThreadTaskSource tts;

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
	tts.PushInfiniteTaskBack([=]()
	{
		*osp << "A ThreadUnitPlusPlus task is running...\n";
		osp->emit();
		std::this_thread::sleep_for(std::chrono::seconds(1));
	});

	// Construct the thread unit object, pass our constructed task source object.
	imp::ThreadUnitPlusPlus tupp(tts);

	// Let the thread run the task until 'enter' is pressed.
	*osp << "Press Enter to stop the test.\n";
	std::string buffer;
	std::getline(std::cin, buffer);
	// Test resetting the task buffer.
	tupp.SetTaskSource({});
}

void TestPooler()
{
	static constexpr int TaskCount{ 5 };
	static constexpr int ThreadCount{ 10 };
	// Construct a task source object, it provides the functions for adding the lambda as a no-argument non-capturing lambda (which wraps the user provided).
	imp::ThreadTaskSource tts;
	// Push the capturing lambda.
	tts.PushInfiniteTaskBack([&]()
		{
			std::osyncstream os(std::cout);
			os << "A ThreadPooler task is running...\n";
			os.emit();
			std::this_thread::sleep_for(std::chrono::seconds(1));
		});
	AddLotsOfTasks(tts, TaskCount-1);

	// Construct a thread pooler object
	imp::ThreadPooler<ThreadCount> tpr;

	// Reset aggregate task source, this evenly apportions the tasks across the number of threads.
	// If you want to specify which tasks go on which thread, you can access the task array directly.
	// The 'thread unit' class is usable on it's own, and is exposed for use.
	// i.e.,
	// tpr.ThreadList[0].SetTaskSource(tts);
	tpr.ResetInfiniteTaskArray(tts);

	// Let the thread run the task until 'enter' is pressed.
	std::cout << "Press Enter to stop the test.\n";
	std::string buffer;
	std::getline(std::cin, buffer);
}

void TestBoolCvPackCopying()
{
	// Runs through all the copy/move ctors for BoolCvPack
	auto getPack = []() { return imp::BoolCvPack{}; };
	imp::BoolCvPack bcp, bcpOther;
	bcp.UpdateState(true);
	bcpOther = bcp;
	assert(bcp.GetState());
	assert(bcpOther.GetState());
	bcp = getPack();
	assert(!bcp.GetState());
	imp::BoolCvPack crvRef{ bcpOther };
	imp::BoolCvPack rvRef{ std::move(crvRef)};
	assert(rvRef.GetState());
}

void TestThreadUnitMoving()
{
	// Construct a thread unit running a single task that does nothing
	const auto sleepLam = []() { std::this_thread::sleep_for(std::chrono::seconds(1)); };
	imp::ThreadUnitPlusPlus tupp{ {sleepLam} };
	// move the thread unit into a newly created instance (test the move ctor)
	imp::ThreadUnitPlusPlus tupp2{ std::move(tupp) };
	// assert the moved thread is running.
	assert(tupp2.IsRunning());
	// test the move-assign constructor
	const auto getRvalueRef = []() { return imp::ThreadUnitPlusPlus{}; };
	imp::ThreadUnitPlusPlus tupp3;
	tupp3 = std::move(tupp2);
	assert(tupp3.IsRunning());
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
	std::string buffer;
	//test thread unit
	TestThreadPP();

	//test thread pool
	TestPooler();

	//test copying/moving of BoolCvPack
	TestBoolCvPackCopying();

	//test thread unit std::moving
	TestThreadUnitMoving();

	MultiPrint("\nEnter to exit...\n\n");
	std::getline(std::cin, buffer);
}