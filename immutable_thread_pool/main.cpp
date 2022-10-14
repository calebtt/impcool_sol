// main.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>

#include "ThreadPooler.h"
#include "ThreadUnitPlusPlus.h"

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
	impcool::ThreadTaskSource tts;
	// Push the capturing lambda.
	tts.PushInfiniteTaskBack([&]()
	{
		*osp << "A ThreadUnitPlusPlus task is running...\n";
		osp->emit();
		std::this_thread::sleep_for(std::chrono::seconds(1));
	});

	// Construct the thread unit object, pass our constructed task source object.
	impcool::ThreadUnitPlusPlus tupp(tts);

	// Let the thread run the task until 'enter' is pressed.
	*osp << "Press Enter to stop the test.\n";
	std::string buffer;
	std::getline(std::cin, buffer);
}

void TestPooler()
{
	static constexpr int TaskCount{ 5 };
	static constexpr int ThreadCount{ 3 };
	// Construct a task source object, it provides the functions for adding the lambda as a no-argument non-capturing lambda (which wraps the user provided).
	impcool::ThreadTaskSource tts;
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
	impcool::ThreadPooler<ThreadCount> tpr;

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

	MultiPrint("\nEnter to exit...\n\n");
	std::getline(std::cin, buffer);
}