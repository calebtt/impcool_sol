#pragma once
/*
 * TaskOpsProvider.h
 *
 * Helper class for working with task lists.
 * Note that some features have been avoided to maintain
 * compatibility with older C++ language standards.
 * Notably, using concepts such as std::ranges::range to specify
 * a requirement that a container supports iteration in a very clear way (std::iterable?)
 *
 * Caleb Taylor August 8th, 2022
 * MIT license.
 */
#include <cstddef>
#include <memory>
#include <functional>

namespace impcool
{
	template<typename TaskContainer_t, typename TaskType_t = std::function<void()>>
	class TaskOpsProvider
	{
	public:
        /// <summary> Push a function with zero or more arguments, but no return value, into the task list (immer::vector).
        /// These tasks are run infinitely, are not popped from the task list after completion. </summary>
        /// <typeparam name="TaskContainer_t"> Type of the container filled with tasks. </typeparam>
        /// <typeparam name="Lambda_t"> The type of the function. </typeparam>
        /// <typeparam name="ArgsList_t"> The types of the arguments. </typeparam>
        /// <param name="taskList"> Container filled with tasks. </param>
        /// <param name="task"> The lambda function to push. </param>
        /// <param name="args"> The arguments to pass to the function. </param>
        template <typename Lambda_t, typename... ArgsList_t>
        static void PushInfiniteTaskBack(TaskContainer_t& taskList, const Lambda_t& task, const ArgsList_t&... args)
        {
            // Construct a new (immutable) copy of the task list (handy that we aren't modifying the data used by the running thread).
            if constexpr (sizeof...(args) == 0)
                taskList = taskList.push_back(TaskType_t{ task });
            else
                taskList = taskList.push_back(TaskType_t{ [task, args...] { task(args...); } });
        }
        /// <summary> Push a function with zero or more arguments, but no return value, into the task list (immer::vector).
        /// These tasks are run infinitely, are not popped from the task list after completion. </summary>
        /// <typeparam name="TaskContainer_t"> Type of the container filled with tasks. </typeparam>
        /// <typeparam name="Lambda_t"> The type of the function. </typeparam>
        /// <typeparam name="ArgsList_t"> The types of the arguments. </typeparam>
        /// <param name="taskList"> Container filled with tasks. </param>
        /// <param name="task"> The lambda function to push. </param>
        /// <param name="args"> The arguments to pass to the function. </param>
        template <typename Lambda_t, typename... ArgsList_t>
        static void PushInfiniteTaskFront(TaskContainer_t& taskList, const Lambda_t& task, const ArgsList_t&... args)
        {
            // Construct a new (immutable) copy of the task list (handy that we aren't modifying the data used by the running thread).
            if constexpr (sizeof...(args) == 0)
                taskList = TaskContainer_t{ TaskType_t{task} } + taskList;
            else
                taskList = TaskContainer_t{ TaskType_t{ [task, args...] { task(args...); }} } + taskList;
        }


		static void AddTaskAtIndex(TaskContainer_t &taskList, std::size_t ind)
		{
			
		}
		/// <summary> Returns the number of tasks in the task list.</summary>
		[[nodiscard]]
		static std::size_t GetNumberOfTasks(TaskContainer_t &taskList) { return taskList.size(); }
	};
}

