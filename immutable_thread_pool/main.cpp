// main.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>

#include "ThreadPooler.h"
#include "ThreadUnitPlusPlus.h"

void AddLotsOfTasks(auto &tc, std::size_t count, const bool addFirstTwo = true)
{
	if (addFirstTwo)
	{
		tc.PushInfiniteTaskBack([&]()
			{
				std::osyncstream os(std::cout);
				//std::cout << "Infinite task is infinite...\n";
				os << "Task #1 running...\n";
				os.emit();
				std::this_thread::sleep_for(std::chrono::milliseconds(750));
			});
		tc.PushInfiniteTaskBack([&]()
			{
				std::osyncstream os(std::cout);
				os << "Task #2 running...\n";
				os.emit();
				std::this_thread::sleep_for(std::chrono::milliseconds(750));
			});
	}
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
		*osp << "A task is running...\n";
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
	static constexpr int TaskCount{ 5 };
	std::string buffer;
	//test thread unit
	TestThreadPP();

	////test thread pool
	//TestThreadPool(os, MultiPrint, TaskCount);

	MultiPrint("\nEnter to exit...\n\n");
	std::getline(std::cin, buffer);
}