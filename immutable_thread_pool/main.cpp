// main.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>

#include "immutable_thread_pool.h"
#include "impcool_thread_unit.h"


void AddLotsOfTasks(auto &tc, auto &os, std::size_t count, const bool addFirstTwo = true)
{
	if (addFirstTwo)
	{
		tc.PushInfiniteTaskBack([&]()
			{
				//std::cout << "Infinite task is infinite...\n";
				os << "Task #1 running...\n";
				os.emit();
				std::this_thread::sleep_for(std::chrono::milliseconds(750));
			});
		tc.PushInfiniteTaskBack([&]()
			{
				os << "Task #2 running...\n";
				os.emit();
				std::this_thread::sleep_for(std::chrono::milliseconds(750));
			});
	}
	for(size_t i = 0; i < count; i++)
	{
		tc.PushInfiniteTaskBack([&](auto taskNumber)
			{
				os << "Task with args: [" << taskNumber << "] running...\n";
				os.emit();
				std::this_thread::sleep_for(std::chrono::milliseconds(750));
			}, i);
	}
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
	//thread unit object
	impcool::ThreadUnit tc;

	// Add lots of tasks that perform simple work, printing a message.
	// Tasks of the argument-less form as well as tasks with arguments.
	AddLotsOfTasks(tc, os, 5);
	std::string buffer;

	std::getline(std::cin, buffer);
	MultiPrint("Adding another task, while it's running...\n");
	tc.PushInfiniteTaskBack([]() { std::cout << "Another wonderful task added...\n"; });
	// Pausing the thread, will wait for the entire list of tasks to
	// be executed first.
	std::getline(std::cin, buffer);
	MultiPrint("Pausing (ordered)...\n");
	//tc.SetPauseValueUnordered(true);
	tc.SetPauseValueOrdered(true);

	// Un-pausing the thread, will resume executing the tasks on the thread.
	std::getline(std::cin, buffer);
	MultiPrint("Unpausing...\n");
	tc.SetPauseValueOrdered(false);
	//tc.SetPauseValueUnordered(false);

	// Test aborting the "pause" state and destroying the thread immediately.
	std::getline(std::cin, buffer);
	MultiPrint("Testing destroy while pause is requested...\n");
	tc.SetPauseValueOrdered(true);
	tc.DestroyThread();

	// Create a new thread and add new tasks to it.
	MultiPrint("Creating new thread and pool...\n");
	tc.CreateThread();
	AddLotsOfTasks(tc, os, 2, false);
	MultiPrint("Added new tasks...\n");

	std::getline(std::cin, buffer);
	std::cout << "Pausing again (unordered)...\n";
	tc.SetPauseValueUnordered(true);

	std::getline(std::cin, buffer);
	std::cout << "Unpausing again...\n";
	tc.SetPauseValueUnordered(false);
	// Enter to exit here, testing destructor.
	std::getline(std::cin, buffer);

	//impcool::impcool_thread_pool itp{};
	//itp.push_infinite_task([]() { std::cout << "Visual Studio uber alles1.\n"; });
	//itp.push_infinite_task([]() { std::cout << "Visual Studio uber alles2.\n"; });
	//itp.push_infinite_task([]() { std::cout << "Visual Studio uber alles3.\n"; });
	//itp.push_infinite_task([]() { std::cout << "Visual Studio uber alles4.\n"; });
	
	
}