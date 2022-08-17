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
#include <vector>
#include <condition_variable>
#include <syncstream>
#include "impcool_bool_cv_pack.h"

namespace impcool
{
    /// <summary> Helper class for working with threads and task lists.
    /// Manages running a thread pool thread. The thread can be started, paused, and destroyed.
    /// The task list can be simply returned as it is immutable, or counted.
    /// A low-level granular kind of access is desireable here, if possible. </summary>
    /// <remarks> This class is also useable on it's own, if so desired. There are two mutually exclusive
    /// pause conditions, each with their own setter function. </remarks>
    struct ThreadUnit
    {
    private:
        // Factory funcs to make it easier to switch to a different smart pointer type, if desired.
        template<typename T>
        auto MakeUniqueSmart(auto ...args) { return std::make_unique<T>(args...); }
        template<typename T>
        auto MakeSharedSmart(auto ...args) { return std::make_shared<T>(args...); }
    public:
        using TaskWrapper_t = std::function<void()>;
        using thread_t = std::thread;
        using bool_cv_t = BoolCvPack;
        using atomic_t = std::atomic<bool>;
        using TaskContainer_t = std::vector<TaskWrapper_t>;
        using unique_thread_t = std::unique_ptr<thread_t>;
    private:
        /// <summary> Constant used to store the loop delay time period when no tasks are present. </summary>
        static constexpr std::chrono::milliseconds EmptyWaitTime{ std::chrono::milliseconds(20) };
        /// <summary> Condition variable + bool pack, to notify a specific thread that stop is requested,
        /// for task list update/maintenance aka thread destruction. </summary>
        atomic_t m_isStopRequested{ false };
        /// <summary> Condition variable + bool pack, to notify a specific thread that pause is requested.
        /// Pauses running thread pool thread, so the tasks are not run. Call update_state after pausing to wake
        /// the thread pool func and continue processing. </summary>
        //imp_bool_cv_ptr_t m_isOrderedPauseRequested{ MakeUniqueSmart<imp_bool_cv_t>() };
        bool_cv_t m_isOrderedPauseRequested{ };
        /// <summary> Condition variable + bool pack, to notify a specific thread that pause is requested.
        /// Pauses running thread pool thread, so the tasks are not run. Call update_state after pausing to wake
        /// the thread pool func and continue processing. </summary>
        /// <remarks> For the immediate pause functionality. Pauses in the middle of executing the task list. </remarks>
        bool_cv_t m_isUnorderedPauseRequested{ };
        /// <summary> Condition variable + bool pack, to notify when the pause operation has completed,
        /// and the thread is in a "paused" state. </summary>
        bool_cv_t m_isPauseCompleted{ };
        /// <summary> Smart pointer to the thread to be constructed. </summary>
        unique_thread_t m_workThreadObj{};
        /// <summary> List of tasks to be ran on this specific workThread. </summary>
        TaskContainer_t m_taskListPtr;
    public:
        /// <summary> Ctor starts the thread. </summary>
        ThreadUnit()
        {
            CreateThread();
        }
        ThreadUnit(const ThreadUnit& other)
	        : m_isOrderedPauseRequested(other.m_isOrderedPauseRequested),
	          m_isUnorderedPauseRequested(other.m_isUnorderedPauseRequested),
	          m_isPauseCompleted(other.m_isPauseCompleted),
	          m_taskListPtr(other.m_taskListPtr)
        {
            m_workThreadObj.reset(other.m_workThreadObj.get());
            m_isStopRequested.exchange(other.m_isStopRequested);
        }
        ThreadUnit(ThreadUnit&& other) noexcept
	        : m_isOrderedPauseRequested(std::move(other.m_isOrderedPauseRequested)),
	          m_isUnorderedPauseRequested(std::move(other.m_isUnorderedPauseRequested)),
	          m_isPauseCompleted(std::move(other.m_isPauseCompleted)),
	          m_workThreadObj(std::move(other.m_workThreadObj)),
	          m_taskListPtr(std::move(other.m_taskListPtr))
        {
            m_isStopRequested.exchange(other.m_isStopRequested);
        }
        ThreadUnit& operator=(const ThreadUnit& other)
        {
	        if (this == &other)
		        return *this;
            m_isStopRequested.store(m_isStopRequested);
	        m_isOrderedPauseRequested = other.m_isOrderedPauseRequested;
	        m_isUnorderedPauseRequested = other.m_isUnorderedPauseRequested;
	        m_isPauseCompleted = other.m_isPauseCompleted;
            m_workThreadObj.reset(other.m_workThreadObj.get());
	        m_taskListPtr = other.m_taskListPtr;
	        return *this;
        }
        ThreadUnit& operator=(ThreadUnit&& other) noexcept
        {
	        if (this == &other)
		        return *this;
            m_isStopRequested.store(std::move(other.m_isStopRequested));
	        m_isOrderedPauseRequested = std::move(other.m_isOrderedPauseRequested);
	        m_isUnorderedPauseRequested = std::move(other.m_isUnorderedPauseRequested);
	        m_isPauseCompleted = std::move(other.m_isPauseCompleted);
	        m_workThreadObj = std::move(other.m_workThreadObj);
	        m_taskListPtr = std::move(other.m_taskListPtr);
	        return *this;
        }
        /// <summary> Dtor destroys the thread. </summary>
        ~ThreadUnit()
        {
            DestroyThread();
        }
    public:
        /// <summary> Starts the work thread running, to execute each task in the list infinitely. </summary>
        /// <returns> true on thread created, false otherwise. </returns>
        bool CreateThread()
        {
            if (m_workThreadObj == nullptr)
            {
                m_isStopRequested.store(false);
                m_workThreadObj = MakeUniqueSmart<thread_t>(&impcool::ThreadUnit::threadPoolFunc, this, std::cref(m_taskListPtr));
                return true;
            }
            return false;
        }
        /// <summary> Setting the pause value via this function will complete
        /// the in-process task list processing before pausing. </summary>
        /// <param name="enablePause"> true to enable pause, false to disable </param>
        /// <remarks><b>Note:</b> The two different pause states (for <c>true</c> value) are mutually exclusive! Only one may be set at a time. </remarks>
        void SetPauseValueOrdered(const bool enablePause)
        {
            //If a value is being set to true, and the other pause state is already true...
            if (enablePause && m_isUnorderedPauseRequested.GetState())
            {
                //Do nothing...
                return;
            }
            //Otherwise...
            m_isOrderedPauseRequested.UpdateState(enablePause);
        }
        /// <summary> Setting the pause value via this function will complete
        /// only the in-process task before pausing mid-way in the list, if not at the end. </summary>
        /// <param name="enablePause"> true to enable pause, false to disable </param>
        /// /// <remarks><b>Note:</b> The two different pause states (for <c>true</c> value) are mutually exclusive! Only one may be set at a time. </remarks>
        void SetPauseValueUnordered(const bool enablePause)
        {
            //If a value is being set to true, and the other pause state is already true...
            if (enablePause && m_isOrderedPauseRequested.GetState())
            {
                //Do nothing...
                return;
            }
            m_isUnorderedPauseRequested.UpdateState(enablePause);
        }
        /// <summary> Queryable by the user to indicate that the state of the running thread is indeed
        /// in a paused state. After requesting a pause, a user should query this member fn to know
        /// if/when the operation has completed, or alternatively, call <c>wait_for_pause_completed</c> </summary>
        /// <remarks> Calling <c>wait_for_pause_completed</c> will likely involve much lower CPU usage than implementing
        /// your own wait function via this method. </remarks>
        [[nodiscard]]
        bool GetPauseCompletionStatus() const
        {
            return m_isPauseCompleted.GetState();
        }
        /// <summary> Called to wait for the thread to enter the "paused" state, after
        /// a call to <c>set_pause_value</c> with <b>true</b>. </summary>
        /// <param name="require_pause_is_requested"> Option to require pause to have been requested before call to this function.
        /// Function does nothing if set to <b>true</b> and no pause has been requested. </param>
        /// <remarks> Set <c>require_pause_is_requested</c> to false to wait <b>indefinitely</b> to enter a pause state. </remarks>
        void WaitForPauseCompleted(const bool require_pause_is_requested = true)
        {
            const bool testForRequested = require_pause_is_requested ? m_isOrderedPauseRequested.GetState() : true;
            // If pause not yet completed, AND pause is actually requested...
            if (!m_isPauseCompleted.GetState() && testForRequested)
            {
                m_isPauseCompleted.WaitForTrue();
            }
        }
        /// <summary> Destructs the running thread after it finishes running the current task it's on
        /// within the task list. Marks the thread func to stop then joins and waits for it to return.
        /// WILL CLEAR the task list! </summary>
        /// <remarks><b>WILL CLEAR the task list!</b></remarks>
        void DestroyThread()
        {
            SetPauseValueOrdered(false);
            SetPauseValueOrdered(false);
            StartDestruction();
            WaitForDestruction();
            //m_taskListPtr.reset();
            m_taskListPtr = {};
        }
    public:
        /// <summary> Push a function with zero or more arguments, but no return value, into the task list (immer::vector).
        /// These tasks are run infinitely, are not popped from the task list after completion. </summary>
        /// <typeparam name="F"> The type of the function. </typeparam>
        /// <typeparam name="A"> The types of the arguments. </typeparam>
        /// <param name="task"> The function to push. </param>
        /// <param name="args"> The arguments to pass to the function. </param>
        template <typename F, typename... A>
        void PushInfiniteTaskBack(const F& task, const A&... args)
        {
            // In order to add a new task, the currently running thread must be destructed
            // and created anew with a new copy of the task list.

            // Start by requesting destruction.
            StartDestruction();
            // Construct a new (immutable) copy of the task list (handy that we aren't modifying the data used by the running thread).
            if constexpr (sizeof...(args) == 0)
                m_taskListPtr.push_back(std::function<void()>(task));
            else
                m_taskListPtr.push_back(std::function<void()>([task, args...]{ task(args...); }));
            // Wait for destruction to complete...
            WaitForDestruction();
            // Re-create thread function for the pool of tasks.
            CreateThread();
        }
        /// <summary> Push a function with zero or more arguments, but no return value, into the task list (immer::vector).
        /// These tasks are run infinitely, are not popped from the task list after completion. </summary>
        /// <typeparam name="F"> The type of the function. </typeparam>
        /// <typeparam name="A"> The types of the arguments. </typeparam>
        /// <param name="task"> The function to push. </param>
        /// <param name="args"> The arguments to pass to the function. </param>
        template <typename F, typename... A>
        void PushInfiniteTaskFront(const F& task, const A&... args)
        {
            // In order to add a new task, the currently running thread must be destructed
            // and created anew with a new copy of the task list.

            // Start by requesting destruction.
            StartDestruction();
            // Construct a new (immutable) copy of the task list (handy that we aren't modifying the data used by the running thread).
            if constexpr (sizeof...(args) == 0)
                m_taskListPtr.insert(m_taskListPtr.begin(), std::function<void()>(task));
            else
                m_taskListPtr.insert(m_taskListPtr.begin(), std::function<void()>([task, args...]{ task(args...); }));
            // Wait for destruction to complete...
            WaitForDestruction();
            // Re-create thread function for the pool of tasks.
            CreateThread();
        }
        /// <summary> Returns the number of tasks in the task list.</summary>
        [[nodiscard]]
        std::size_t GetNumberOfTasks() const { return m_taskListPtr.size(); }
        /// <summary> Returns a copy of the immutable task list. </summary>
        [[nodiscard]]
        auto GetTaskList() const { return m_taskListPtr; }
        /// <summary> Stops the thread, replaces the task list, creates the thread again. </summary>
        void SetTaskList(const TaskContainer_t &newTaskList)
        {
            StartDestruction();
            WaitForDestruction();
            m_taskListPtr = newTaskList;
            CreateThread();
        }
    private:
        void StartDestruction()
        {
            m_isStopRequested.store(true);
        }
        void WaitForDestruction()
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
        void threadPoolFunc(const TaskContainer_t &tasks)
        {
            const auto TestForPause = [&](auto& pauseObj)
            {
                if (pauseObj.GetState())
                {
                    m_isPauseCompleted.UpdateState(true);
                    pauseObj.WaitForFalse();
                    m_isPauseCompleted.UpdateState(false);
                }
            };
            // While not is stop requested.
            while (!m_isStopRequested.load())
            {
                if (tasks.empty())
                {
                    std::this_thread::sleep_for(EmptyWaitTime);
                }
                // Iterate task list, running tasks set for this thread.
                for (const auto& currentTask : tasks)
                {
                    currentTask();
                    //double check outer condition here, as this may be long-running,
                    //causes destruction to occur unordered.
                    if (m_isStopRequested.load())
                        break;
                    //test for unordered pause request
                    TestForPause(m_isUnorderedPauseRequested);
                }
                //test for ordered pause
                TestForPause(m_isOrderedPauseRequested);
            }
        }
    };
}