#pragma once
/*
 * ThreadUnitPlusPlus.h
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
#include "ThreadTaskSource.h"
#include "BoolCvPack.h"

namespace imp
{
    /// <summary> Class for working with a thread and task list.
    /// Manages running a thread pool thread. The thread can be paused, and destroyed.
    /// The task list can be simply returned as it is not mutated in-use only copied, or counted.
    /// A low-level granular kind of access is desirable here, if possible. </summary>
    /// <remarks> This class is also useable on it's own, if so desired. There are two mutually exclusive
    /// pause conditions, each with their own setter function. </remarks>
    class ThreadUnitPlusPlus
    {
        /// <summary> Constant used to store the loop delay time period when no tasks are present. </summary>
        static constexpr std::chrono::milliseconds EmptyWaitTime{ std::chrono::milliseconds(20) };
    public:
        using Thread_t = std::jthread;
        using AtomicBool_t = std::atomic<bool>;
        using UniquePtrThread_t = std::unique_ptr<Thread_t>;

        // Alias for the ThreadTaskSource which provides a container and some operations.
        using TaskOpsProvider_t = imp::ThreadTaskSource;
        using TaskContainer_t = decltype(TaskOpsProvider_t::TaskList);

    private:
        struct ThreadConditionals
        {
	        imp::BoolCvPack OrderedPausePack;
	        imp::BoolCvPack UnorderedPausePack;
	        imp::BoolCvPack PauseCompletedPack;
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
            void SetStopSource(const std::stop_source sts)
            {
                PauseCompletedPack.stop_source = sts;
                OrderedPausePack.stop_source = sts;
                UnorderedPausePack.stop_source = sts;
            }
        };
    private:

        // Conditionals pack
        ThreadConditionals m_conditionalsPack;

        /// <summary> Smart pointer to the thread to be constructed. </summary>
        UniquePtrThread_t m_workThreadObj{};

        /// <summary> Copy of the last list of tasks to be set to run on this work thread.
        /// Should mirror the list of tasks that the worker thread is using. </summary>
        TaskOpsProvider_t m_taskList{};

        // Stop source for the thread
        std::stop_source m_stopSource{};
    public:

        /// <summary> Ctor creates the thread. </summary>
        ThreadUnitPlusPlus(const imp::ThreadTaskSource tasks = {})
        {
            m_taskList = tasks;
            CreateThread(m_taskList, false);
        }

        /// <summary> Dtor destroys the thread. </summary>
        ~ThreadUnitPlusPlus()
        {
            DestroyThread();
        }
    public:

        /// <summary> Setting the pause value via this function will complete the in-process task list
        /// processing before pausing. </summary>
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

        [[nodiscard]] bool IsRunning() const
        {
            return m_workThreadObj != nullptr && !m_stopSource.stop_requested();
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
            if (needsToWait && pauseReq)
            {
                m_conditionalsPack.PauseCompletedPack.WaitForTrue();
                //TODO fix the problem of clearing the pause state when a double pause request is made!
            }
        }

        /// <summary> Returns the number of tasks running on the thread task list.</summary>
        [[nodiscard]] std::size_t GetNumberOfTasks() const
        {
            return m_taskList.TaskList.size();
        }

        /// <summary> Returns a copy of the last set immutable task list, it should mirror
        /// the tasks running on the thread. </summary>
        [[nodiscard]] auto GetTaskSource() const
        {
            return m_taskList;
        }

        /// <summary> Stops the thread, replaces the task list, creates the thread again. </summary>
        void SetTaskSource(const ThreadTaskSource newTaskList)
        {
            StartDestruction();
            WaitForDestruction();
            m_taskList = newTaskList;
            CreateThread(newTaskList);
        }

        /// <summary> Destructs the running thread after it finishes running the current task it's on
        /// within the task list. Marks the thread func to stop then joins and waits for it to return. </summary>
        /// <remarks><b>WILL CLEAR the task source!</b> To start the thread again, just set a new task source.</remarks>
        void DestroyThread()
        {
            StartDestruction();
            WaitForDestruction();
            m_taskList.TaskList = {};
        }
    private:
        /// <summary> Starts the work thread running, to execute each task in the list infinitely. </summary>
        /// <returns> true on thread created, false otherwise (usually thread already created). </returns>
        bool CreateThread(const ThreadTaskSource tasks, const bool isPausedOnStart = false)
        {
            if (m_workThreadObj == nullptr)
            {
                //reset some conditionals aka std::condition_variable 
                m_conditionalsPack.PauseCompletedPack.UpdateState(false);
                m_conditionalsPack.OrderedPausePack.UpdateState(isPausedOnStart);
                m_conditionalsPack.UnorderedPausePack.UpdateState(false);
                //make thread obj
                m_workThreadObj = std::make_unique<Thread_t>([=, this](std::stop_token st) { threadPoolFunc(st, tasks.TaskList); });
                //make local handle to stop_source for thread
                m_stopSource = m_workThreadObj->get_stop_source();
                //update conditionals pack to have stop handle
                m_conditionalsPack.SetStopSource(m_stopSource);
                return true;
            }
            return false;
        }

        /// <summary> Changes stop requested to true... </summary>
        /// <remarks> NOTE: Destruction will occur un-ordered!
        /// If you want the list of tasks in-progress to run until the end, then request an ordered pause first! </remarks>
        void StartDestruction()
        {
            m_stopSource.request_stop();
            m_conditionalsPack.Notify();
        }

        /// <summary> Joins the work thread to the current thread and waits. </summary>
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

        /// <summary> The worker function, on the created running thread. </summary>
        /// <param name="stopToken"> Passed in the std::jthread automatically at creation. </param>
        /// <param name="tasks"> List of tasks copied into this worker function, it is not mutated in-use. </param>
        void threadPoolFunc(const std::stop_token stopToken, const TaskContainer_t tasks)
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