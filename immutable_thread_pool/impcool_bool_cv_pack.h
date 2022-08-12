#pragma once
/*
 * impcool_bool_cv_pack.h
 *
 * A pack of types used with a condition variable, and some helper
 * functions to aid in operating on them to perform a common task (WaitForFalse, WaitForTrue, get/update etc.)
 *
 * Caleb Taylor August 8th, 2022
 * MIT license.
 */
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>

namespace impcool
{
    /// <summary> Pack of types used with a condition variable, and the functions
    /// that operate on them. Default constructed object has is_condition_true set to false. </summary>
    struct bool_cv_pack
    {
        // Some type aliases used in the condition variable packs, and possibly elsewhere.
        using SharedData = bool;
        using CvMutex = std::mutex;
        using CvConditionVar = std::condition_variable;
        /// <summary> An alias for the scoped lock type used to lock the cv mutex before modifying the shared variable. </summary>
        using cv_setterlock_t = std::lock_guard<CvMutex>;
        /// <summary> An alias for the scoped lock type used to lock the cv mutex when calling wait() on the condition var. </summary>
        using cv_waiterlock_t = std::unique_lock<CvMutex>;
    public:
        /// <summary> The shared data condition flag holding the notifiable state. </summary>
        std::atomic<SharedData> is_condition_true{ false };
        /// <summary> A condition variables used to notify the work thread when "shared data" becomes false. </summary>
        CvConditionVar task_running_cv{};
        /// <summary> The mutex used for controlling access to updating the shared data conditions. </summary>
        CvMutex running_mutex{};
    public:
        bool_cv_pack() = default;
        ~bool_cv_pack() = default;

        bool_cv_pack(const bool_cv_pack& other)
        {
            is_condition_true.exchange(other.is_condition_true.load());
        }

        bool_cv_pack(bool_cv_pack&& other) noexcept
        {
            is_condition_true.exchange(other.is_condition_true);
        }

        bool_cv_pack& operator=(const bool_cv_pack& other)
        {
	        if (this == &other)
		        return *this;
            is_condition_true.exchange(other.is_condition_true.load());
	        return *this;
        }

        bool_cv_pack& operator=(bool_cv_pack&& other) noexcept
        {
	        if (this == &other)
		        return *this;
            is_condition_true.exchange(other.is_condition_true);
	        return *this;
        }

        /// <summary> Waits for a boolean SharedData atomic to return <b>false</b>.
        /// <b>This uses the condition_variable's "wait()" function</b> and so it will only
        /// wake up and check the condition when another thread calls <c>"notify_one()"</c> or
        /// <c>"notify_all()"</c>. Without the notify from another thread, it would basically be a <b>deadlock</b>. </summary>
        void WaitForFalse() 
        {
            cv_waiterlock_t pause_lock(running_mutex);
            task_running_cv.wait(pause_lock, [&]() -> bool
                {
                    return !is_condition_true;
                });
        }

        /// <summary> Waits for a boolean SharedData atomic to return <b>true</b>.
        /// <b>This uses the condition_variable's "wait()" function</b> and so it will only
        /// wake up and check the condition when another thread calls <c>"notify_one()"</c> or
        /// <c>"notify_all()"</c>. Without the notify from another thread, it would basically be a <b>deadlock</b>. </summary>
        void WaitForTrue()
        {
            cv_waiterlock_t pause_lock{ running_mutex };
            task_running_cv.wait(pause_lock, [&]() -> bool
                {
                    return is_condition_true;
                });
        }

        /// <summary> Called to update the shared state variable. Notifies all waiting threads
        /// to wake up and perform their wait check. </summary>
        /// <param name="trueOrEnabled"> true to enable, presumably. </param>
        void UpdateState(const SharedData trueOrEnabled)
        {
            {
                cv_setterlock_t setter_lock{ running_mutex };
                is_condition_true = trueOrEnabled;
            }
            task_running_cv.notify_all();
        }
        /// <summary> Returns the value of the atomic bool SharedData.
        /// It is not necessary to follow the <c>condition_variable</c> procedure just to
        /// check this value, nor lock the mutex since it's an atomic. </summary>
        /// <returns>bool value of the SharedData</returns>
        bool GetState() const
        {
            return is_condition_true;
        }
    };
}