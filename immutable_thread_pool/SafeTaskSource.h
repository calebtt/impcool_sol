#pragma once
#include <thread>
#include <memory>
#include <functional>
#include <deque>
#include <mutex>
#include <ranges>
#include "ThreadConcepts.h"

namespace imp
{
    /// <summary>
    /// ThreadTaskSource provides a container that holds async tasks, and some functions
    /// for operating on it.
    /// </summary>
    class SafeTaskSource
    {
    public:
        using Mut_t = std::mutex;
        using TaskInfo = std::function<void()>;
        using Lock_t = std::scoped_lock<Mut_t>;
        using TaskContainer_t = std::deque<TaskInfo>;
    private:
        mutable Mut_t m_taskMut;
        TaskContainer_t m_taskList{};
    public:
        SafeTaskSource() = default;
        SafeTaskSource(const IsFnRange auto& taskList)
        {
            m_taskList = taskList;
        }
        SafeTaskSource(const TaskInfo&& ti)
        {
            PushInfiniteTaskBack(ti);
        }

        SafeTaskSource(const SafeTaskSource& other)
	        : m_taskList{other.m_taskList} {}

        auto operator=(const SafeTaskSource& other) -> SafeTaskSource&
        {
	        if (this == &other)
		        return *this;
	        m_taskList = other.m_taskList;
	        return *this;
        }

        // Getter fn for task list.
        auto Get() const -> TaskContainer_t
        {
            Lock_t tempLock(m_taskMut);
            return m_taskList;
        }
        
        /// <summary> Push a function with zero or more arguments, but no return value, into the task list. </summary>
        /// <typeparam name="F"> The type of the function. </typeparam>
        /// <typeparam name="A"> The types of the arguments. </typeparam>
        /// <param name="taskFn"> The function to push. </param>
        /// <param name="args"> The arguments to pass to the function (by value). </param>
        template <typename F, typename... A>
        void PushInfiniteTaskBack(const F& taskFn, const A&... args)
        {
            Lock_t tempLock(m_taskMut);
            if constexpr (sizeof...(args) == 0)
            {
                m_taskList.emplace_back(TaskInfo{ taskFn });
            }
            else
            {
                m_taskList.emplace_back(TaskInfo([taskFn, args...] { taskFn(args...); }));
            }
        }

        /// <summary> Push a function with zero or more arguments, but no return value, into the task list. </summary>
        /// <typeparam name="F"> The type of the function. </typeparam>
        /// <typeparam name="A"> The types of the arguments. </typeparam>
        /// <param name="task"> The function to push. </param>
        /// <param name="args"> The arguments to pass to the function (by value). </param>
        template <typename F, typename... A>
        void PushInfiniteTaskFront(const F& task, const A&... args)
        {
            Lock_t tempLock(m_taskMut);
            if constexpr (sizeof...(args) == 0)
            {
                m_taskList.emplace_front(TaskInfo{ task });
            }
            else
            {
                m_taskList.emplace_front(TaskInfo([task, args...] { task(args...); }));
            }
        }

        void ResetTaskList(const IsFnRange auto& taskContainer)
        {
            Lock_t tempLock(m_taskMut);
            m_taskList = {};
            for (const auto& elem : taskContainer)
            {
                m_taskList.emplace_back(elem);
            }
        }
    };

}