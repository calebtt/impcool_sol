/*
 * ThreadPool.h
 * A thread pool that utilizes the concept of immutability to
 * make implementation simpler. The task buffer is copied upon
 * thread creation and no shared data exists there.
 * Caleb T. August 8th, 2022
 * MIT license.
 */
#pragma once
#include <thread>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <syncstream>
#include <array>
#include <algorithm>
#include <cassert>
#include "ThreadUnitPlus.h"

namespace impcool
{
    /// <summary>
    /// Concept for a ThreadUnit describing operations that must be supported.
    /// </summary>
    template<typename ThreadType_t>
    concept IsThreadUnit = requires (ThreadType_t & t)
    {
        t.CreateThread();
    	t.SetPauseValueOrdered(true);
        t.SetPauseValueUnordered(true);
        t.WaitForPauseCompleted();
        t.DestroyThread();
        t.PushInfiniteTaskBack([](int) {}, 1);
        t.GetNumberOfTasks();
    };

    /// <summary>
    /// A thread pool for running infinitely reoccurring tasks.
    /// Prototyped with the immer library for immutable data structures.
    /// Immutable types enforce a convenient form of implementation that relies on the
    /// copying of data instead of mutating it and working around the changing state.
    /// </summary>
    /// <remarks>
    /// Non-copyable.
    /// Non-moveable.
    /// Does NOT start the threads running on construction.
    /// </remarks>
    template<unsigned NumThreads = 4, IsThreadUnit ThreadProvider_t = impcool::ThreadUnitPlus>
    class ThreadPool
    {
        static_assert(NumThreads > 0, "Number of Threads must be > 0...");
        //TODO implement benchmarking and auto-balancing of tasks.
    public:
        using Task_t = std::function<void()>;
        using ThreadUnit_t = ThreadProvider_t;
    private:
        // the list of threads
        std::array<ThreadUnit_t, NumThreads> m_threadList{};
        ThreadTaskSource m_taskList;
    public:
        ThreadPool()
        {
	        for(auto &elem : m_threadList)
	        {
                elem.CreateThread();
	        }
        }
    public:
    	/// <summary> Push a pre-constructed vector of std::function with zero arguments, and no return value, into the task list management object(s).
        /// These tasks are run infinitely, are not popped from the task list after completion. </summary>
        /// <typeparam name="F"> The type of the function. </typeparam>
        /// <typeparam name="A"> The types of the arguments. </typeparam>
        /// <param name="taskFnList"> The list of functions to push. </param>
        //void PushInfiniteTaskArrayBack(const std::ranges::range auto &taskFnList)
        //{
        //    // Iterate the list of threads, request an ordered pause.
        //    for(auto & threadUnit : m_threadList)
        //        threadUnit.SetPauseValueOrdered(true);

        //    // Wait for ordered pause to complete...
        //    for (auto& threadUnit : m_threadList)
        //        threadUnit.WaitForPauseCompleted();
        //    //TODO this is broken!

        //    for (const auto& taskElem : taskFnList)
        //    {
        //        // iterate list, find placement for task (thread with fewest tasks)
        //        auto minIt = std::min_element(m_threadList.begin(), m_threadList.end(), [](const auto& lhs, const auto& rhs)
        //            {
        //                return lhs.GetNumberOfTasks() < rhs.GetNumberOfTasks();
        //            });
        //        minIt->PushInfiniteTaskBack(taskFnList);
        //    }
        //}


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
            assert(minIt != m_threadList.end());
            minIt->PushInfiniteTaskBack(taskFn, args...);
        }

        void SetPauseThreadsOrdered(const bool doPause)
        {
	        for(auto &elem : m_threadList)
		        elem.SetPauseValueOrdered(doPause);
        }

        void SetPauseThreadsUnordered(const bool doPause)
        {
            for (auto& elem : m_threadList) 
                elem.SetPauseValueUnordered(doPause);
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

        void WaitForPauseCompleted()
        {
            for (auto& elem : m_threadList)
                elem.WaitForPauseCompleted();
        }

        [[nodiscard]] std::size_t GetTaskCount() const
        {
            std::size_t currentCount{};
            for (const auto& elem : m_threadList)
            {
                currentCount += elem.GetNumberOfTasks();
            }
            return currentCount;
        }

        /// <summary> Returns a vector of tasks associated with each ThreadUnit as a single container. </summary>
        [[nodiscard]] auto GetUnifiedTaskList()
        {
            std::vector<Task_t> taskContainer;
            //typename ThreadUnit_t::TaskContainer_t taskContainer;
	        for(const auto &elem : m_threadList)
	        {
                const auto tempDeque = elem.GetTaskList();
                taskContainer.reserve(taskContainer.size() + tempDeque.size());
                std::move(std::begin(tempDeque), std::end(tempDeque), std::back_inserter(taskContainer));
	        }
            return taskContainer;
        }

        /// <summary> Returns a non-owning pointer to the internal ThreadUnit array. </summary>
        /// <returns> non-owning pointer to internal ThreadUnit array </returns>
        /// <remarks> Can be used to add tasks to individual ThreadUnits if desired, or start/stop/pause them individually. </remarks>
        [[nodiscard]] auto* GetThreadList()
        {
            return &m_threadList;
        }
    private:

    };
}