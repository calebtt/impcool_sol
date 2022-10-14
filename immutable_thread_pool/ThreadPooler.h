/*
 * ThreadPooler.h
 * A thread pool that utilizes the concept of immutability to
 * make implementation simpler. The task buffer is copied upon
 * thread creation and no shared data exists there.
 * Caleb T. Oct 12th, 2022
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
#include <ranges>
#include <cassert>
#include "ThreadConcepts.h"
#include "ThreadUnitPlusPlus.h"

namespace impcool
{
    /// <summary>
    /// A thread pool for running infinitely reoccurring tasks.
    /// Provides some aggregate operations for the array of thread units.
    /// General use-case is to access the thread units directly for most operations.
    ///
    /// Prototyped with the immer library for immutable data structures.
    /// Immutable types enforce a convenient form of implementation that relies on the
    /// copying of data instead of mutating it and working around the changing state.
    /// </summary>
    /// <remarks>
    /// Non-copyable.
    /// Non-moveable.
    /// </remarks>
    template<unsigned NumThreads = 4, IsThreadUnit ThreadProvider_t = impcool::ThreadUnitPlusPlus>
    class ThreadPooler
    {
        static_assert(NumThreads > 0, "Number of Threads must be > 0...");
        //TODO implement benchmarking and auto-balancing of tasks.
    public:
        using Task_t = std::function<void()>;
        
        /// <summary> <b>PUBLIC</b> member, the list of threads </summary>
        std::array<ThreadProvider_t, NumThreads> ThreadList{};
    public:
        ThreadPooler() = default;
    public:
        /// <summary> Blocking, destroys each thread (by joining) and resets their task list to empty before
        /// re-creation. </summary>
        void ClearAllTasks()
        {
	        for(ThreadProvider_t &elem : ThreadList)
	        {
                elem.SetTaskSource({});
	        }
        }

        /// <summary> Adds a properly apportioned (wrt to the number of threads) task.
        /// These tasks are run infinitely, are not popped from the task list after completion. </summary>
        /// <typeparam name="F"> The type of the function. </typeparam>
        /// <typeparam name="A"> The types of the arguments. </typeparam>
        /// <param name="taskFn"> The function to push. </param>
        /// <param name="args"> The arguments to pass to the function. </param>
        template <typename F, typename... A>
        void PushApportionedTask(const F& taskFn, const A&... args)
        {
            // iterate list, find placement for task (thread with fewest tasks)
            auto minIt = std::min_element(ThreadList.begin(), ThreadList.end(), [](const auto& lhs, const auto& rhs)
                {
                    return lhs.GetNumberOfTasks() < rhs.GetNumberOfTasks();
                });
            assert(minIt != ThreadList.end());
            auto tempTaskList = minIt->GetTaskSource();
            tempTaskList->PushInfiniteTaskBack(taskFn, args...);
            minIt->SetTaskList(tempTaskList);
        }

        /// <summary> Apportion a pre-constructed ThreadTaskSource into the thread unit management object(s).
        /// These tasks are run infinitely, are not popped from the task list after completion. </summary>
        /// <param name="taskFnList"> The task source aka list of functions to push. </param>
        void ResetInfiniteTaskArray(const ThreadTaskSource taskFnList)
        {
            // Number of tasks.
            const std::size_t taskCount = taskFnList.TaskList.size();
            // Tasks per thread.
            const auto tempCount = (taskCount / NumThreads);
            const std::size_t tasksPer = tempCount > 0 ? tempCount : 1;
            const auto chunkedTasks = taskFnList.TaskList | std::ranges::views::chunk(tasksPer);
            //std::cerr << typeid(chunkedTasks).name();

            // Assert the count of chunks produced is less than equal the num threads...
            assert(chunkedTasks.size() <= NumThreads);
            // Set the new task lists...
            for(std::size_t i{}; i < chunkedTasks.size(); ++i)
            {
                const auto viewIndex = static_cast<long long>(i);
                const std::deque<std::function<void()>> tempChunk{ chunkedTasks[viewIndex].begin(), chunkedTasks[viewIndex].end() };
                ThreadList[i].SetTaskSource(tempChunk);
            }
        }

        void SetPauseThreadsOrdered(const bool doPause)
        {
            for (auto& elem : ThreadList)
                elem.SetPauseValueOrdered(doPause);
        }

        void SetPauseThreadsUnordered(const bool doPause)
        {
            for (auto& elem : ThreadList)
                elem.SetPauseValueUnordered(doPause);
        }

        void DestroyAll()
        {
            for (auto& elem : ThreadList)
                elem.DestroyThread();
        }

        void WaitForPauseCompleted()
        {
            for (auto& elem : ThreadList)
                elem.WaitForPauseCompleted();
        }

        [[nodiscard]] std::size_t GetTaskCount() const
        {
            std::size_t currentCount{};
            for (const auto& elem : ThreadList)
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
            for (const auto& elem : ThreadList)
            {
                const auto tempDeque = elem.GetTaskSource();
                taskContainer.reserve(taskContainer.size() + tempDeque.size());
                std::move(std::begin(tempDeque), std::end(tempDeque), std::back_inserter(taskContainer));
            }
            return taskContainer;
        }
    private:

    };
}