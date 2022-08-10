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
    struct impcool_bool_cv_pack
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
        mutable CvMutex running_mutex;
    public:
        /// <summary> Default constructor is default. </summary>
        impcool_bool_cv_pack() = default;
        // Copy operations...
        impcool_bool_cv_pack(const impcool_bool_cv_pack& other) = delete;
        impcool_bool_cv_pack& operator=(const impcool_bool_cv_pack& other) = delete;
        // Move operations...
        impcool_bool_cv_pack(impcool_bool_cv_pack&& other) = delete;
        impcool_bool_cv_pack& operator=(impcool_bool_cv_pack&& other) = delete;
        /// <summary> Default destructor is default. </summary>
        ~impcool_bool_cv_pack() = default;
    public:
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