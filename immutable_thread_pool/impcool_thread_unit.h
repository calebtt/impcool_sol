#pragma once
/*
 * impcool_thread_unit.h
 *
 * Helper class for working with threads and task lists.
 * Fully functional stand-alone class to manage a single thread that has a task list (immer::vector)
 * and supports operations such as create/destroy and pause/unpause.
 *
 * Caleb Taylor August 8th, 2022
 * MIT license.
 */
#pragma once
#include <thread>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <syncstream>
#include <immer/vector.hpp>
#include "impcool_bool_cv_pack.h"

namespace impcool
{
    /// <summary> Helper class for working with threads and task lists.
    /// Manages running a thread pool thread. The thread can be started, paused, and destroyed. </summary>
    /// <remarks> This class is also useable on it's own, if so desired. </remarks>
    struct impcool_thread_unit
    {
    public:
        using imptask_t = std::function<void()>;
        using impvec_t = immer::vector<imptask_t>;
        using impthread_t = std::thread;
    private:
        /// <summary> Constant used to store the loop delay time period when no tasks are present. </summary>
        static constexpr std::chrono::milliseconds EmptyWaitTime{ std::chrono::milliseconds(20) };

        /// <summary> Condition variable + bool pack, to notify a specific thread that stop is requested,
        /// for task list update/maintenance aka thread destruction. </summary>
        std::atomic<bool> m_isStopRequested{ false };

        /// <summary> Condition variable + bool pack, to notify a specific thread that pause is requested.
        /// Pauses running thread pool thread, so the tasks are not run. Call notify after pausing to wake
        /// the thread pool func and continue processing. </summary>
        impcool_bool_cv_pack m_isPauseRequested{};

        /// <summary> Condition variable + bool pack, to notify when the pause operation has completed,
        /// and the thread is in a "paused" state. </summary>
        impcool_bool_cv_pack m_isPauseCompleted{};

        /// <summary> Smart pointer to the thread to be constructed. </summary>
        std::unique_ptr<impthread_t> m_workThreadObj{};

        /// <summary> List of tasks to be ran on this specific workThread. </summary>
        std::shared_ptr<impvec_t> m_taskListPtr{};
    public:
        /// <summary> Ctor starts the thread. </summary>
        impcool_thread_unit()
        {
            create_thread();
        }
        // Copy operations...
        impcool_thread_unit(const impcool_thread_unit& other) = delete;
        impcool_thread_unit& operator=(const impcool_thread_unit& other) = delete;
        // Move operations...
        impcool_thread_unit(impcool_thread_unit&& other) = delete;
        impcool_thread_unit& operator=(impcool_thread_unit&& other) = delete;
        /// <summary> Dtor destroys the thread. </summary>
        ~impcool_thread_unit()
        {
            destroy_thread();
        }
    public:
        /// <summary> Starts the work thread running, to execute each task in the list infinitely. </summary>
        /// <returns> true on thread created, false otherwise. </returns>
        bool create_thread()
        {
            if(m_taskListPtr == nullptr)
            {
                m_taskListPtr = std::make_shared<impvec_t>();
            }
            if (m_workThreadObj == nullptr)
            {
                m_isStopRequested = false;
                m_workThreadObj = std::make_unique<impthread_t>(&impcool_thread_unit::threadPoolFunc, this, m_taskListPtr);
                return true;
            }
            return false;
        }

        /// <summary> Push a function with zero or more arguments, but no return value, into the task list (immer::vector).
        /// These tasks are run infinitely, are not popped from the task list after completion. </summary>
        /// <typeparam name="F"> The type of the function. </typeparam>
        /// <typeparam name="A"> The types of the arguments. </typeparam>
        /// <param name="task"> The function to push. </param>
        /// <param name="args"> The arguments to pass to the function. </param>
        template <typename F, typename... A>
        void push_infinite_task(const F& task, const A&... args)
        {
            // In order to add a new task, the currently running thread must be destructed
            // and created anew with a new copy of the task list.

            // Start by requesting destruction.
            start_destruction();
            // Construct a new (immutable) copy of the task list (handy that we aren't modifying the data used by the running thread).
            // Update the class member shared_ptr to the task list.
            if constexpr (sizeof...(args) == 0)
                m_taskListPtr = std::make_shared<impvec_t>(m_taskListPtr->push_back(std::function<void()>(task)));
            else
                m_taskListPtr = std::make_shared<impvec_t>(m_taskListPtr->push_back(std::function<void()>([task, args...]{ task(args...); })));
            // Wait for destruction to complete...
            wait_for_destruction();
            // Re-create thread function for the pool of tasks.
            create_thread();
        }

        /// <summary> Returns the number of tasks in the task list.</summary>
        [[nodiscard]]
        std::size_t get_number_of_tasks() const { return m_taskListPtr->size(); }

        /// <summary> Note: Setting the pause value via this function will complete
        /// the in-process task list processing before pausing. </summary>
        /// <param name="enablePause">true to enable pause, false to disable</param>
        void set_pause_value(const bool enablePause)
        {
            m_isPauseRequested.UpdateState(enablePause);
        }

        //TODO add a pause method that pauses it immediately in the task queue, instead of finishing the task loop. Might rename this to ordered_set_pause_value()

        /// <summary> Queryable by the user to indicate that the state of the running thread is indeed
        /// in a paused state. After requesting a pause, a user should query this member fn to know
        /// if/when the operation has completed, or alternatively, call <c>wait_for_pause_completed</c> </summary>
        /// <remarks> Calling <c>wait_for_pause_completed</c> will likely involve much lower CPU usage than implementing
        /// your own wait function via this method. </remarks>
        [[nodiscard]]
    	bool get_pause_completion_status() const
        {
            return m_isPauseCompleted.GetState();
        }

        /// <summary> Called to wait for the thread to enter the "paused" state, after
        /// a call to <c>set_pause_value</c> with <b>true</b>. </summary>
        /// <param name="require_pause_is_requested"> Option to require pause to have been requested before call to this function.
        /// Function does nothing if set to <b>true</b> and no pause has been requested. </param>
        /// <remarks> Set <c>require_pause_is_requested</c> to false to wait <b>indefinitely</b> to enter a pause state. </remarks>
        void wait_for_pause_completed(const bool require_pause_is_requested = true)
        {
            const bool testForRequested = require_pause_is_requested ? m_isPauseRequested.GetState() : true;
            // If pause not yet completed, AND pause is actually requested...
            if (!m_isPauseCompleted.GetState() && testForRequested)
            {
                m_isPauseCompleted.WaitForTrue();
            }
        }

        //TODO add ordered_destroy_thread() too, which waits for the task list to complete an iteration before destruction.

        /// <summary> Destructs the running thread after it finishes running the current task it's on
        /// within the task list. Marks the thread func to stop then joins and waits for it to return.
        /// WILL CLEAR the task list! </summary>
        /// <remarks><b>WILL CLEAR the task list!</b></remarks>
        void destroy_thread()
        {
            set_pause_value(false);
            start_destruction();
            wait_for_destruction();
            m_taskListPtr.reset();
        }
    private:

        void start_destruction()
        {
            m_isStopRequested = true;
        }
        void wait_for_destruction()
        {
            if (m_workThreadObj != nullptr)
            {
                if (m_workThreadObj->joinable())
                {
                    m_workThreadObj->join();
                    m_workThreadObj.reset();
                }
            }
        }
        void threadPoolFunc(const std::shared_ptr<impvec_t> tasks)
        {
            // While not is stop requested.
            while (!m_isStopRequested)
            {
                if (tasks->empty())
                {
                    std::this_thread::sleep_for(EmptyWaitTime);
                }
                // Iterate task list, running tasks set for this thread.
                for (const auto& currentTask : *tasks)
                {
                    currentTask();
                    //double check outer condition here, as this may be long-running.
                    if (m_isStopRequested)
                        break;
                }
                if (m_isPauseRequested.GetState())
                {
                    m_isPauseCompleted.UpdateState(true);
                    m_isPauseRequested.WaitForFalse();
                    m_isPauseCompleted.UpdateState(false);
                }
            }
        }
    };
}