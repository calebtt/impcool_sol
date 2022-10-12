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

//void TestThreadUnit(std::osyncstream &os, const auto &MultiPrint, const size_t taskCount = 5)
//{
//	MultiPrint("\n\nBeginning test of the Thread Unit type...\n");
//	//thread unit object
//	impcool::ThreadUnitPlus tc;
//
//	std::string buffer;
//	// Add lots of tasks that perform simple work, printing a message.
//	// Tasks of the argument-less form as well as tasks with arguments.
//	AddLotsOfTasks(tc, taskCount);
//
//	std::getline(std::cin, buffer);
//	MultiPrint("Adding another task, while it's running...\n");
//	tc.PushInfiniteTaskFront([]() { std::cout << "Another wonderful task added...\n"; });
//	// Pausing the thread, will wait for the entire list of tasks to
//	// be executed first.
//	std::getline(std::cin, buffer);
//	MultiPrint("Pausing (ordered)...\n");
//	//tc.SetPauseValueUnordered(true);
//	tc.SetPauseValueOrdered(true);
//
//	// Un-pausing the thread, will resume executing the tasks on the thread.
//	std::getline(std::cin, buffer);
//	MultiPrint("Unpausing...\n");
//	tc.SetPauseValueOrdered(false);
//	//tc.SetPauseValueUnordered(false);
//
//	// Test aborting the "pause" state and destroying the thread immediately.
//	std::getline(std::cin, buffer);
//	MultiPrint("Testing destroy while pause is requested...\n");
//	tc.SetPauseValueOrdered(true);
//	tc.DestroyThread();
//	tc.DestroyThread();
//
//	// Test adding tasks while paused.
//	std::getline(std::cin, buffer);
//	MultiPrint("Testing add task while paused...\n");
//	//tc.SetPauseValueOrdered(true);
//	tc.WaitForPauseCompleted();
//	tc.PushInfiniteTaskBack([]() { std::cout << "Task added while paused...\n"; });
//
//	// Create a new thread and add new tasks to it.
//	MultiPrint("Creating new thread and pool...\n");
//	tc.CreateThread();
//	AddLotsOfTasks(tc, 2, false);
//	MultiPrint("Added new tasks...\n");
//
//	std::getline(std::cin, buffer);
//	std::cout << "Pausing again (unordered)...\n";
//	tc.SetPauseValueUnordered(true);
//
//	std::getline(std::cin, buffer);
//	std::cout << "Unpausing again...\n";
//	tc.SetPauseValueUnordered(false);
//	// Enter to exit here, testing destructor.
//	std::getline(std::cin, buffer);
//}
//
//template<unsigned ThreadCount = 2>
//void TestThreadPool(std::osyncstream &os, const auto &MultiPrint, const int TaskCount)
//{
//	using namespace std::chrono_literals;
//	static constexpr auto TaskSleepTime = 250ms;
//	std::string buffer;
//	//test thread pool
//	MultiPrint("Beginning the test of the ThreadPool class...\n");
//	
//	std::getline(std::cin, buffer);
//	impcool::ThreadPool<ThreadCount, impcool::ThreadUnitPlus> tp;
//	tp.PushInfiniteTaskBack([]()
//	{
//		std::osyncstream os(std::cout);
//		os << "ThreadPool first task running...\n";
//		os.emit();
//		std::this_thread::sleep_for(TaskSleepTime);
//	});
//	// After the single task has been added, set the pause value to true and wait for it.
//	std::this_thread::sleep_for(std::chrono::milliseconds(750));
//	tp.SetPauseThreadsOrdered(true);
//	tp.WaitForPauseCompleted();
//	MultiPrint("Now we will add a few more tasks, and hope that we can avoid seeing the first task run again.\n");
//	tp.PushInfiniteTaskBack([]()
//		{
//			std::osyncstream os(std::cout);
//			os << "ThreadPool task added while paused... \n";
//			os.emit();
//			std::this_thread::sleep_for(TaskSleepTime);
//		});
//	tp.PushInfiniteTaskBack([]()
//		{
//			std::osyncstream os(std::cout);
//			os << "ThreadPool task added while paused... \n";
//			os.emit();
//			std::this_thread::sleep_for(TaskSleepTime);
//		});
//	MultiPrint("If no output was between here and the last message, the pause before adding threads works.\n");
//	std::getline(std::cin, buffer);
//	MultiPrint("\n\nAdding " + std::to_string(TaskCount) + " tasks after clearing and un-pausing...\n\n");
//	tp.DestroyAll();
//	AddLotsOfTasks(tp, TaskCount, false);
//	tp.SetPauseThreadsOrdered(false);
//	std::getline(std::cin, buffer);
//}

// Test of the ThreadUnitPlusPlus new paradigm
void TestThreadPP()
{
	impcool::ThreadTaskSource tts;
	tts.PushInfiniteTaskBack([]() { std::cerr << "A task running.\n"; });
	impcool::ThreadUnitPlusPlus tupp(tts);
	const auto makeRV = []() { return impcool::ThreadTaskSource{}; };
	impcool::ThreadUnitPlusPlus tupp2(makeRV());
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
	////test thread unit
	//TestThreadUnit(os, MultiPrint, TaskCount);

	////test thread pool
	//TestThreadPool(os, MultiPrint, TaskCount);

	MultiPrint("\nEnter to exit...\n\n");
	std::getline(std::cin, buffer);
}