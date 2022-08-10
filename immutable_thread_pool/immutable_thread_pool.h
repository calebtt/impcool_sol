/*
 * immutable_thread_pool.h
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
#include "impcool_bool_cv_pack.h"
#include "impcool_thread_unit.h"

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
    template<unsigned NumThreads = 4>
    class impcool_thread_pool
    {
    public:
        //using concurrency_t = std::invoke_result_t<decltype(std::thread::hardware_concurrency)>;
        using imptask_t = std::function<void()>;
        using impvec_t = immer::vector<imptask_t>;
        using impthread_unit_t = impcool_thread_unit;
        using impvec_thread_t = immer::vector<impthread_unit_t>;
    private:
        //TODO implement benchmarking and auto-balancing of tasks.
        std::array<impvec_thread_t, NumThreads> m_threadList{};
    public:
        impcool_thread_pool() = default;
    public:
        /// <summary> Push a function with zero or more arguments, but no return value, into the task list management object(s).
        /// These tasks are run infinitely, are not popped from the task list after completion. </summary>
        /// <typeparam name="F"> The type of the function. </typeparam>
        /// <typeparam name="A"> The types of the arguments. </typeparam>
        /// <param name="task"> The function to push. </param>
        /// <param name="args"> The arguments to pass to the function. </param>
        template <typename F, typename... A>
        void push_infinite_task(const F& task, const A&... args)
        {
            // iterate list, find placement for task.
            size_t minIndex{};
            for(size_t i = 0; i < m_threadList.size(); i++)
            {
                const auto taskCount = m_threadList[i].get_number_of_tasks();
                if (taskCount < minIndex)
                    minIndex = i;
            }
            // add to manager thread.
            m_threadList[minIndex].push_infinite_task(task, args...);
        }
    private:
     //   [[nodiscard]]
    	//static auto GetCoreCount() noexcept -> concurrency_t
     //   {
     //       const concurrency_t numThreads = std::thread::hardware_concurrency();
     //       return numThreads < DefaultCoreCount ? DefaultCoreCount : numThreads;
     //   }

    };
}