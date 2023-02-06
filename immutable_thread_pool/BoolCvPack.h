#pragma once
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>

namespace imp
{
    /// <summary> A pack of types used with a condition variable, and some helper functions to aid in
    /// operating on them to perform a common task (WaitForFalse, WaitForTrue, get / update etc.) </summary>
    /// <remarks> Default constructed object has is_condition_true set to false. Copyable, Movable. </remarks>
    struct BoolCvPack
    {
        static constexpr bool DoDebugLog{ false };
        // Some type aliases used in the condition variable packs, and possibly elsewhere.
        // alias for std::condition_variable type
        using CvConditionVar = std::condition_variable;
        // An alias for the scoped lock type used to lock the cv mutex before modifying the shared variable.
        using SetterLock_t = std::lock_guard<std::mutex>;
        // An alias for the scoped lock type used to lock the cv mutex when calling wait() on the condition var.
        using WaiterLock_t = std::unique_lock<std::mutex>;
        // Stop source can be used to cancel the wait operations.
        using StopSource_t = std::stop_source;
    public:
        /// <summary> The shared data condition flag holding the notifiable state. </summary>
        std::atomic<bool> is_condition_true{ false };
        /// <summary> A condition variables used to notify the work thread when "shared data" becomes false. </summary>
        CvConditionVar task_running_cv{};
        /// <summary> The mutex used for controlling access to updating the shared data conditions. </summary>
        std::mutex running_mutex{};
        // Stop source used to cancel the wait operations.
        StopSource_t stop_source{ std::nostopstate };
    public:
        BoolCvPack() noexcept = default;
        ~BoolCvPack() noexcept = default;
        BoolCvPack(const BoolCvPack& other) noexcept
        {
            if constexpr (DoDebugLog)
            {
                std::cerr << "BoolCvPack copy-construct\n";
            }
            stop_source = other.stop_source;
	        is_condition_true.store(other.is_condition_true.load());
        }
        BoolCvPack(BoolCvPack&& other) noexcept
        {
            if constexpr (DoDebugLog)
            {
                std::cerr << "BoolCvPack move-construct\n";
            }
            stop_source = other.stop_source;
	        is_condition_true.store(other.is_condition_true.load());
        }
        BoolCvPack& operator=(const BoolCvPack& other) noexcept
        {
            if constexpr (DoDebugLog)
            {
                std::cerr << "BoolCvPack operator-assign\n";
            }
            if (this == &other)
                return *this;
            stop_source = other.stop_source;
            is_condition_true.store(other.is_condition_true.load());
            return *this;
        }
        BoolCvPack& operator=(BoolCvPack&& other) noexcept
        {
            if constexpr (DoDebugLog)
            {
                std::cerr << "BoolCvPack move-assign\n";
            }
            if (this == &other)
                return *this;
            stop_source = other.stop_source;
            is_condition_true.store(other.is_condition_true.load());
            return *this;
        }
    public:
        /// <summary> Waits for a boolean SharedData atomic to return <b>false</b>.
        /// <b>This uses the condition_variable's "wait()" function</b> and so it will only
        /// wake up and check the condition when another thread calls <c>"notify_one()"</c> or
        /// <c>"notify_all()"</c>. Without the notify from another thread, it would basically be a <b>deadlock</b>. </summary>
        void WaitForFalse() noexcept
        {
            WaiterLock_t pause_lock(running_mutex);
            task_running_cv.wait(pause_lock, [&]() -> bool
                {
                    const bool stopPossibleAndRequested = stop_source.stop_possible() && stop_source.stop_requested();
                    return !is_condition_true || stopPossibleAndRequested;
                });
        }
        /// <summary> Waits for a boolean SharedData atomic to return <b>true</b>.
        /// <b>This uses the condition_variable's "wait()" function</b> and so it will only
        /// wake up and check the condition when another thread calls <c>"notify_one()"</c> or
        /// <c>"notify_all()"</c>. Without the notify from another thread, it would basically be a <b>deadlock</b>. </summary>
        void WaitForTrue() noexcept
        {
            WaiterLock_t pause_lock{ running_mutex };
            task_running_cv.wait(pause_lock, [&]() -> bool
                {
                    const bool stopPossibleAndRequested = stop_source.stop_possible() && stop_source.stop_requested();
                    return is_condition_true || stopPossibleAndRequested;
                });
        }
        /// <summary> Called to update the shared state variable. Notifies all waiting threads
        /// to wake up and perform their wait check. </summary>
        /// <param name="trueOrEnabled"> true to enable, presumably. </param>
        void UpdateState(const bool trueOrEnabled) noexcept
        {
            {
                SetterLock_t setter_lock{ running_mutex };
                is_condition_true = trueOrEnabled;
            }
            task_running_cv.notify_all();
        }
        /// <summary> Returns the value of the atomic bool SharedData.
        /// It is not necessary to follow the <c>condition_variable</c> procedure just to
        /// check this value, nor lock the mutex since it's an atomic. </summary>
        /// <returns>bool value of the SharedData</returns>
        bool GetState() const noexcept
        {
            return is_condition_true;
        }
    };
}