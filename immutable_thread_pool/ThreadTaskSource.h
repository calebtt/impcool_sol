#pragma once
#include <thread>
#include <memory>
#include <functional>
#include <deque>
#include <ranges>

namespace imp
{
    /// <summary> Concept for a range of std::function or something convertible to it. </summary>
    template<typename FnRange_t>
    concept IsFnRange = requires(FnRange_t & t)
    {
        { std::ranges::range<FnRange_t> };
        { std::convertible_to<typename FnRange_t::value_type, std::function<void()>> };
    };

    using TaskInfo = std::function<void()>;
    using ThreadTaskSource = std::deque<TaskInfo>;

    /// <summary> Push a function with zero or more arguments, but no return value, into the task list. </summary>
    /// <typeparam name="F"> The type of the function. </typeparam>
    /// <typeparam name="A"> The types of the arguments. </typeparam>
    /// <param name="taskFn"> The function to push. </param>
    /// <param name="args"> The arguments to pass to the function (by value). </param>
    template <typename F, typename... A>
    void PushInfiniteTaskBack(IsFnRange auto& taskRange, const F& taskFn, const A... args)
    {
        if constexpr (sizeof...(args) == 0)
        {
            taskRange.emplace_back(TaskInfo{ taskFn });
        }
        else
        {
            taskRange.emplace_back(TaskInfo([taskFn, args...] { taskFn(std::move(args)...); }));
        }
    }

    /// <summary> Push a function with zero or more arguments, but no return value, into the task list. </summary>
    /// <typeparam name="F"> The type of the function. </typeparam>
    /// <typeparam name="A"> The types of the arguments. </typeparam>
    /// <param name="task"> The function to push. </param>
    /// <param name="args"> The arguments to pass to the function (by value). </param>
    template <typename F, typename... A>
    void PushInfiniteTaskFront(IsFnRange auto& taskRange, const F& task, const A... args)
    {
        if constexpr (sizeof...(args) == 0)
        {
            taskRange.emplace_front(TaskInfo{ task });
        }
        else
        {
            taskRange.emplace_front(TaskInfo([task, args...] { task(std::move(args)...); }));
        }
    }

}