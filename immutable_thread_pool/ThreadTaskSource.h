#pragma once
/*
 * ThreadTaskSource.h
 *
 * ThreadTaskSource provides a container that holds async tasks, and some functions
 * for operating on it.
 *
 * Caleb T. Sept. 4th, 2022
 * MIT license.
 */

#include <thread>
#include <memory>
#include <functional>
#include <deque>

namespace impcool
{

	/// <summary>
	/// ThreadTaskSource provides a container that holds async tasks, and some functions
	/// for operating on it.
	/// </summary>
	class ThreadTaskSource
	{
	public:
		using TaskWrapper_t = std::function<void()>;
		using TaskContainer_t = std::deque<TaskWrapper_t>;
	public:
        /// <summary>
        /// Public data member, allows direct access to the task source.
        /// </summary>
        TaskContainer_t TaskList{};
	public:
        /// <summary> Push a function with zero or more arguments, but no return value, into the task list. </summary>
        /// <typeparam name="F"> The type of the function. </typeparam>
        /// <typeparam name="A"> The types of the arguments. </typeparam>
        /// <param name="taskFn"> The function to push. </param>
        /// <param name="args"> The arguments to pass to the function. </param>
        template <typename F, typename... A>
        void PushInfiniteTaskBack(const F& taskFn, const A&... args)
        {
            if constexpr (sizeof...(args) == 0)
            {
                TaskList.push_back(std::function<void()>{taskFn});
            }
            else
            {
                TaskList.push_back(std::function<void()>([taskFn, args...] { taskFn(args...); }));
            }
        }

        /// <summary> Push a function with zero or more arguments, but no return value, into the task list. </summary>
        /// <typeparam name="F"> The type of the function. </typeparam>
        /// <typeparam name="A"> The types of the arguments. </typeparam>
        /// <param name="task"> The function to push. </param>
        /// <param name="args"> The arguments to pass to the function. </param>
        template <typename F, typename... A>
        void PushInfiniteTaskFront(const F& task, const A&... args)
        {
            if constexpr (sizeof...(args) == 0)
            {
                TaskList.push_front(std::function<void()>(task));
            }
            else
            {
                TaskList.push_front(std::function<void()>([task, args...] { task(args...); }));
            }
        }
	};

}