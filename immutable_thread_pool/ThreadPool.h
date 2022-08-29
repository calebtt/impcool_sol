/*
 * ThreadPool.h
 * An immutable thread pool.
 * Caleb Taylor August 8th, 2022
 * MIT license.
 */
#pragma once
#include <thread>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <syncstream>
#include <array>
#include <immer/vector.hpp>
#include <immer/array.hpp>
#include "BoolCvPack.h"
#include "ThreadUnit.h"

namespace impcool
{
    /// <summary>
    /// An immutable thread pool utilizing the immer library immutable data structures.
    /// Immutable types enforce a convenient form of implementation that relies on the
    /// copying of data instead of mutating it and working around the changing state.
    /// </summary>
    /// <remarks>
    /// Non-copyable.
    /// Non-moveable.
    /// </remarks>
    template<unsigned NumThreads = 4, typename ThreadProvider_t = impcool::ThreadUnit>
    class ThreadPool
    {
        //TODO implement benchmarking and auto-balancing of tasks.
        //TODO figure out a better public interface for this.
    public:
        using Task_t = std::function<void()>;
        using ThreadUnit_t = ThreadProvider_t;

        //using TaskContainer_t = immer::vector<Task_t>;
        //using ThreadUnitList_t = immer::vector<ThreadUnit_t>;
    private:
        // the list of threads
        std::array<ThreadUnit_t, NumThreads> m_threadList;
    public:
        ThreadPool() = default;
    public:
        //TODO add array-based task list push and push_front

        /// <summary> Push a function with zero or more arguments, but no return value, into the task list management object(s).
        /// These tasks are run infinitely, are not popped from the task list after completion. </summary>
        /// <typeparam name="F"> The type of the function. </typeparam>
        /// <typeparam name="A"> The types of the arguments. </typeparam>
        /// <param name="taskFn"> The function to push. </param>
        /// <param name="args"> The arguments to pass to the function. </param>
        template <typename F, typename... A>
        void PushInfiniteTaskBack(const F& taskFn, const A&... args)
        {
            // iterate list, find placement for task (thread with fewest tasks)
            auto minIt = std::min_element(m_threadList.begin(), m_threadList.end(), [](const auto &lhs, const auto &rhs)
            {
                    return lhs.GetNumberOfTasks() < rhs.GetNumberOfTasks();
            });
            minIt->PushInfiniteTaskBack(taskFn, args...);
        }

        void SetPauseThreadsOrdered(const bool doPause) const
        {
	        for(const auto &elem : m_threadList)
		        elem.SetPauseOrdered(doPause);
        }
        void SetPauseThreadsUnordered(const bool doPause) const
        {
            for (const auto& elem : m_threadList) 
                elem.SetPauseUnordered(doPause);
        }
        bool CreateAll()
        {
            for (auto& elem : m_threadList)
            {
                const bool createResult = elem.CreateThread();
                if (!createResult)
                    return false;
            }
            return true;
        }
        void DestroyAll()
        {
            for (auto& elem : m_threadList)
                elem.DestroyThread();
        }

        void WaitForPauseCompleted(const bool requirePauseIsRequested = true)
        {
            for (auto& elem : m_threadList)
                elem.WaitForPauseCompleted(requirePauseIsRequested);
        }

        std::size_t GetTaskCount() const
        {
            std::size_t currentCount{};
            for (const auto& elem : m_threadList)
            {
                currentCount += elem.GetNumberOfTasks();
            }
            return currentCount;
        }

        /// <summary> Returns a non-owning pointer to the internal ThreadUnit array. </summary>
        /// <returns>non-owning pointer to internal ThreadUnit array</returns>
        auto* GetThreadList() const
        {
            return &m_threadList;
        }
    private:

    };
}