#pragma once
/*
 * ThreadUnit.h
 *
 * Helper class for working with a thread.
 * Fully functional stand-alone class to manage a single thread that has a task list
 * and supports operations such as create/destroy and pause/unpause.
 *
 * Caleb T. August 8th, 2022
 * MIT license.
 */
#pragma once
#include <thread>
#include <mutex>
#include <functional>
#include <vector>
#include <condition_variable>
#include <stop_token>
#include <syncstream>
#include <memory>
#include <deque>
#include <cassert>
#include "ThreadTaskSource.h"
#include "BoolCvPack.h"

namespace impcool
{
    /// <summary> Class for working with a thread and task list.
    /// Manages running a thread pool thread. The thread can be started, paused, and destroyed.
    /// The task list can be simply returned as it is not mutated in-use only copied, or counted.
    /// A low-level granular kind of access is desirable here, if possible. </summary>
    /// <remarks> This class is also useable on it's own, if so desired. There are two mutually exclusive
    /// pause conditions, each with their own setter function. </remarks>
    class ThreadUnitPlus
    {
    public:
        // Factory funcs to make it easier to switch to a different smart pointer type, if desired.
        template<typename T>
        auto MakeUniqueSmart(auto ...args) { return std::make_unique<T>(args...); }
        template<typename T>
        auto MakeSharedSmart(auto ...args) { return std::make_shared<T>(args...); }
    public:
        using Thread_t = std::jthread;
        using AtomicBool_t = std::atomic<bool>;
        using UniquePtrThread_t = std::unique_ptr<Thread_t>;
        // Alias for the ThreadTaskSource which provides a container and some operations.
        using TaskOpsProvider_t = ThreadTaskSource;
        using SharedPtrTasks_t = std::shared_ptr<TaskOpsProvider_t>;
        using UniquePtrTasks_t = std::unique_ptr<TaskOpsProvider_t>;
        using TaskContainer_t = decltype(TaskOpsProvider_t::TaskList);
    private:
        struct ThreadConditionals
        {
            BoolCvPack OrderedPausePack;
            BoolCvPack UnorderedPausePack;
            BoolCvPack PauseCompletedPack;
            //BoolCvPack isStopRequested;
            // Waits for both pause requests to be false.
            void WaitForBothPauseRequestsFalse()
            {
                OrderedPausePack.WaitForFalse();
                UnorderedPausePack.WaitForFalse();
            }
            void Notify()
            {
                OrderedPausePack.task_running_cv.notify_all();
                UnorderedPausePack.task_running_cv.notify_all();
                PauseCompletedPack.task_running_cv.notify_all();
            }
        };
    private:
        /// <summary> Constant used to store the loop delay time period when no tasks are present. </summary>
        static constexpr std::chrono::milliseconds EmptyWaitTime{ std::chrono::milliseconds(20) };

        // Conditionals pack
        ThreadConditionals m_conditionalsPack;
        /// <summary> Smart pointer to the thread to be constructed. </summary>
        UniquePtrThread_t m_workThreadObj{};
        /// <summary> List of tasks to be ran on this specific workThread. </summary>
        SharedPtrTasks_t m_taskListPtr{ MakeSharedSmart<TaskOpsProvider_t>() };
    	// Stop source for the thread
        std::stop_source m_stopSource{};
    public:
        ThreadUnitPlus(SharedPtrTasks_t taskContainer = {})
        {
            // See if constructing task ops provider is necessary
            if (taskContainer != nullptr)
                m_taskListPtr.reset(taskContainer.get());
        }
        /// <summary> Dtor destroys the thread. </summary>
        ~ThreadUnitPlus()
        {
            DestroyThread();
        }
    public:
        /// <summary> Starts the work thread running, to execute each task in the list infinitely. </summary>
        /// <returns> true on thread created, false otherwise (usually thread already created). </returns>
        bool CreateThread(const bool isPausedOnStart = false)
        {
            if (m_workThreadObj == nullptr)
            {
                //reset some conditionals aka std::condition_variable 
                m_conditionalsPack.PauseCompletedPack.UpdateState(false);
                m_conditionalsPack.OrderedPausePack.UpdateState(isPausedOnStart);
                m_conditionalsPack.UnorderedPausePack.UpdateState(false);
                //make thread obj
            	m_workThreadObj = MakeUniqueSmart<Thread_t>([&](std::stop_token st) { threadPoolFunc(st, m_taskListPtr->TaskList); });
                //make local handle to stop_source for thread
                m_stopSource = m_workThreadObj->get_stop_source();
                //update conditionals pack to have stop handle
                m_conditionalsPack.PauseCompletedPack.stop_source = m_stopSource;
                m_conditionalsPack.OrderedPausePack.stop_source = m_stopSource;
                m_conditionalsPack.UnorderedPausePack.stop_source = m_stopSource;
                return true;
            }
            return false;
        }
        /// <summary>
        /// Setting the pause value via this function will complete the in-process task list
        /// processing before pausing.
        /// </summary>
        /// <param name="enablePause"> true to enable pause, false to disable </param>
        /// <remarks><b>Note:</b> The two different pause states (for <c>true</c> value) are mutually exclusive! Only one may be set at a time. </remarks>
        void SetPauseValueOrdered(const bool enablePause)
        {
            m_conditionalsPack.OrderedPausePack.UpdateState(enablePause);
        }
        /// <summary>
        /// Setting the pause value via this function will complete only the in-process task before pausing
        /// mid-way in the list, if not at the end.
        /// </summary>
        /// <param name="enablePause"> true to enable pause, false to disable </param>
        /// <remarks><b>Note:</b> The two different pause states (for <c>true</c> value) are mutually exclusive! Only one may be set at a time. </remarks>
        void SetPauseValueUnordered(const bool enablePause)
        {
            m_conditionalsPack.UnorderedPausePack.UpdateState(enablePause);
        }
        /// <summary>
        /// Queryable by the user to indicate that the state of the running thread is indeed
        /// in a paused state. After requesting a pause, a user should query this member fn to know
        /// if/when the operation has completed, or alternatively, call <c>wait_for_pause_completed</c>
        /// </summary>
        /// <remarks> Calling <c>wait_for_pause_completed</c> will likely involve much lower CPU usage than implementing
        /// your own wait function via this method. </remarks>
        [[nodiscard]]
        bool GetPauseCompletionStatus() const
        {
            return m_conditionalsPack.PauseCompletedPack.GetState();
        }
        /// <summary> Called to wait for the thread to enter the "paused" state, after
        /// a call to <c>set_pause_value</c> with <b>true</b>. </summary>
        void WaitForPauseCompleted()
        {
            const bool pauseReq = m_conditionalsPack.OrderedPausePack.GetState() || m_conditionalsPack.UnorderedPausePack.GetState();
            const bool needsToWait = !m_conditionalsPack.PauseCompletedPack.GetState();
            // If pause not yet completed, AND pause is actually requested...
            if(needsToWait && pauseReq)
            {
                m_conditionalsPack.PauseCompletedPack.WaitForTrue();
                //TODO fix the problem of clearing the pause state when a double pause request is made!
            }
        }
        /// <summary> Destructs the running thread after it finishes running the current task it's on
        /// within the task list. Marks the thread func to stop then joins and waits for it to return.
        /// WILL CLEAR the task list! </summary>
        /// <remarks><b>WILL CLEAR the task list!</b></remarks>
        void DestroyThread()
        {
            //SetPauseValueOrdered(false);
            //SetPauseValueUnordered(false);
            StartDestruction();
            WaitForDestruction();
            m_taskListPtr->TaskList = {};
        }
        /// <summary> Push a function with zero or more arguments, but no return value, into the task list.
        /// These tasks are run infinitely, are not popped from the task list after completion. </summary>
        /// <typeparam name="F"> The type of the function. </typeparam>
        /// <typeparam name="A"> The types of the arguments. </typeparam>
        /// <param name="taskFn"> The function to push. </param>
        /// <param name="args"> The arguments to pass to the function. </param>
        /// <remarks> Note: This will wait for the entire list of tasks in the list to finish before destructing
        /// and recreating the thread with the new task added to the end. This DOES mean <b>the threads will be un-paused
        /// in order to be destructed!</b> </remarks>
        template <typename F, typename... A>
        void PushInfiniteTaskBack(const F& taskFn, const A&... args)
        {
            // In order to add a new task, the currently running thread must be destructed
            // and created anew with a new copy of the task list.

            // Start by requesting destruction.
            StartDestruction();
            // Store pause state, and unpause if necessary.
            const bool isEitherPaused = GetPauseCompletionStatus();
            SetPauseValueUnordered(false);
            SetPauseValueOrdered(false);

            // Update local copy of the task list, note that the thread has it's own COPY of the data.
            m_taskListPtr->PushInfiniteTaskBack(taskFn, args...);

            // Wait for destruction to complete...
            WaitForDestruction();
            // Re-create thread function for the pool of tasks.
            CreateThread(isEitherPaused);
        }
        /// <summary> Push a function with zero or more arguments, but no return value, into the task list.
        /// These tasks are run infinitely, are not popped from the task list after completion. </summary>
        /// <typeparam name="F"> The type of the function. </typeparam>
        /// <typeparam name="A"> The types of the arguments. </typeparam>
        /// <param name="taskFn"> The function to push. </param>
        /// <param name="args"> The arguments to pass to the function. </param>
        /// <remarks> Note: This will wait for the entire list of tasks in the list to finish before destructing
        /// and recreating the thread with the new task. </remarks>
        template <typename F, typename... A>
        void PushInfiniteTaskFront(const F& taskFn, const A&... args)
        {
            // In order to add a new task, the currently running thread must be destructed
            // and created anew with a new copy of the task list.

            // Start by requesting destruction.
            StartDestruction();

            // Update local copy of the task list, note that the thread has it's own COPY of the data.
            m_taskListPtr->PushInfiniteTaskFront(taskFn, args...);

            // Wait for destruction to complete...
            WaitForDestruction();
            // Re-create thread function for the pool of tasks.
            CreateThread();
        }
        /// <summary> Returns the number of tasks in the task list.</summary>
        [[nodiscard]] std::size_t GetNumberOfTasks() const
        {
	        return m_taskListPtr->TaskList.size();
        }
        /// <summary> Returns a copy of the immutable task list. </summary>
        [[nodiscard]] auto GetTaskList() const
        {
	        return m_taskListPtr->TaskList;
        }
        /// <summary> Stops the thread, replaces the task list, creates the thread again. </summary>
        void SetTaskList(const TaskContainer_t& newTaskList)
        {
            StartDestruction();
            WaitForDestruction();
            m_taskListPtr->TaskList = newTaskList;
            CreateThread();
        }
    private:
        /// <summary> Changes stop requested to true... </summary>
        /// <remarks> NOTE: Destruction will occur un-ordered!
        /// If you want the list of tasks in-progress to run until the end, then request an ordered pause first! </remarks>
        void StartDestruction()
        {
            m_stopSource.request_stop();
            m_conditionalsPack.Notify();
        }
        /// <summary>
        /// Joins the work thread to the current thread and waits.
        /// </summary>
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


        void threadPoolFunc(std::stop_token stopToken, const TaskContainer_t tasks)
        {
            const auto TestAndWaitForPauseEither = [](ThreadConditionals& pauseObj)
            {
                // If either ordered or unordered pause set
                if (pauseObj.OrderedPausePack.GetState() || pauseObj.UnorderedPausePack.GetState())
                {
                    // Set pause completion event, which sends the notify
                    pauseObj.PauseCompletedPack.UpdateState(true);
                    // Wait until the pause state is toggled back to false (both)
                    pauseObj.WaitForBothPauseRequestsFalse();
                    // Reset the pause completed state and continue
                    pauseObj.PauseCompletedPack.UpdateState(false);
                }
            };
            const auto TestAndWaitForPauseUnordered = [](ThreadConditionals& pauseObj)
            {
                // If either ordered or unordered pause set
                if (pauseObj.UnorderedPausePack.GetState())
                {
                    // Set pause completion event, which sends the notify
                    pauseObj.PauseCompletedPack.UpdateState(true);
                    // Wait until the pause state is toggled back to false (both)
                    pauseObj.WaitForBothPauseRequestsFalse();
                    // Reset the pause completed state and continue
                    pauseObj.UnorderedPausePack.UpdateState(false);
                }
            };
            // While not is stop requested.
            while (!stopToken.stop_requested())
            {
                //test for ordered pause
                TestAndWaitForPauseEither(m_conditionalsPack);

                if (tasks.empty())
                {
                    std::this_thread::sleep_for(EmptyWaitTime);
                }
                // Iterate task list, running tasks set for this thread.
                for (const auto& currentTask : tasks)
                {
                    //test for unordered pause request (before the fn call!)
                    TestAndWaitForPauseUnordered(m_conditionalsPack);
                    //double check outer condition here, as this may be long-running,
                	//causes destruction to occur unordered.
                    if (stopToken.stop_requested())
                        break;
                    // run the task
                    currentTask();
                }
            }
        }
    };
}