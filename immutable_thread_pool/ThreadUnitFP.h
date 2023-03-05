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
#include <optional>
#include <variant>
#include "ThreadTaskSource.h"
#include "BoolCvPack.h"
#include "SafeTaskSource.h"
#include "ThreadConditionals.h"

namespace imp
{
    /// <summary> class to manage a single thread that has a task list. Manages running a single thread.
    /// The thread can be paused, and unpaused. 
    /// The task list can be simply replaced as it is not mutated while in use, only copied,
    /// or counted. A low-level granular kind of access is desirable here, if possible. </summary>
    /// <remarks> There are two pause conditions, each with their own setter function.
    /// Non-copyable, <b>is moveable! (move-construct and move-assign)</b></remarks>
    class ThreadUnitFP
    {
    private:
        static constexpr bool UseUnorderedDestruction{ true };
        /// <summary> Constant used to store the loop delay time period when no tasks are present. </summary>
        static constexpr std::chrono::milliseconds EmptyWaitTime{ std::chrono::milliseconds(20) };
    public:
        using Thread_t = std::jthread;
        using AtomicBool_t = std::atomic<bool>;
        using UniquePtrThread_t = std::unique_ptr<Thread_t>;
        // Alias for the ThreadTaskSource which provides a container and some operations.
        using TaskOpsProvider_t = SafeTaskSource;
        using TaskContainer_t = TaskOpsProvider_t::TaskContainer_t;
        using Conditionals_t = ThreadConditionals;
    private:
        /// <summary> Copy of the last list of tasks to be set to run on this work thread.
        /// Should mirror the list of tasks that the worker thread is using. </summary>
        TaskOpsProvider_t m_taskList{};

        // Stop source for the thread
        std::stop_source m_stopSource{};

        // Pack of items used for pause/unpause/pause-complete "events".
        Conditionals_t m_conditionalsPack{ m_stopSource };

        /// <summary> Smart pointer to the thread to be constructed. </summary>
        UniquePtrThread_t m_workThreadObj{};
    public:
        /// <summary> Ctor creates the thread. </summary>
        ThreadUnitFP(const TaskOpsProvider_t tasks = {}) noexcept
        {
            m_taskList = tasks;
            CreateThread(m_taskList, false);
        }

    	/// <summary> Dtor destroys the thread. </summary>
        ~ThreadUnitFP() noexcept
        {
            DestroyThread();
        }

        // Implemented move operations.
        ThreadUnitFP(ThreadUnitFP&& other) noexcept
	        : m_taskList(std::move(other.m_taskList)),
	          m_stopSource(std::move(other.m_stopSource)),
	          m_conditionalsPack(std::move(other.m_conditionalsPack)),
	          m_workThreadObj(std::move(other.m_workThreadObj))
    	{
        }
        ThreadUnitFP& operator=(ThreadUnitFP&& other) noexcept
        {
            if (this == &other)
                return *this;
            m_conditionalsPack = std::move(other.m_conditionalsPack);
            m_workThreadObj = std::move(other.m_workThreadObj);
            m_taskList = std::move(other.m_taskList);
            m_stopSource = std::move(other.m_stopSource);
            return *this;
        }
        // Deleted copy operations.
        ThreadUnitFP& operator=(const ThreadUnitFP& other) = delete;
        ThreadUnitFP(const ThreadUnitFP& other) = delete;
    public:
        void SetOrderedPause() noexcept
        {
            DoOrderedPause(m_conditionalsPack);
        }

        void SetUnorderedPause() noexcept
        {
            DoUnorderedPause(m_conditionalsPack);
        }

        void Unpause() noexcept
        {
            DoUnpause(m_conditionalsPack);
        }

        /// <summary>
        /// Returns true if thread is created, tasks are added.
        /// </summary>
        /// <remarks>Pause state has no effect on this.</remarks>
        /// <returns>
        /// True if and only if:
        /// <para>Stop is not requested.</para>
        /// <para>The thread is created and running.</para>
        /// <para>There are tasks in the task list.</para>
        /// <para>Pause is not requested nor completed.</para>
        /// </returns>
        [[nodiscard]]
    	bool IsWorking() const noexcept
        {
            [[unlikely]]
            if (m_stopSource.stop_requested())
                return false;
            [[unlikely]]
            if (m_workThreadObj == nullptr)
                return false;
            //return !GetPauseCompletionStatus();
            const bool hasTasks = GetNumberOfTasks() > 0;
            const bool isPausing = IsPausing(m_conditionalsPack);
            const bool isPauseCompleted = IsPauseCompleted(m_conditionalsPack);
            return hasTasks ? !isPausing && !isPauseCompleted : false;
        }

        /// <summary>
        /// Queryable by the user to indicate that the state of the running thread is indeed
        /// in a paused state. After requesting a pause, a user should query this member fn to know
        /// if/when the operation has completed, or alternatively, call <c>wait_for_pause_completed</c>
        /// </summary>
        /// <remarks>
        /// Calling <c>WaitForPauseCompleted</c> will likely involve much lower CPU usage than implementing
        /// your own wait function via this method.
        /// </remarks>
        /// <returns>True if and only if pause request has been completed.</returns>
        [[nodiscard]]
        bool GetPauseCompletionStatus() const noexcept
        {
            return m_conditionalsPack.PauseCompletedPack.GetState();
        }

        /// <summary> Called to wait for the thread to enter the "paused" state, after
        /// a call to <c>Set_Type_Pause()</c>. </summary>
        void WaitForPauseCompleted() noexcept
        {
            const bool pauseReq = m_conditionalsPack.OrderedPausePack.GetState() || m_conditionalsPack.UnorderedPausePack.GetState();
            const bool needsToWait = !m_conditionalsPack.PauseCompletedPack.GetState();
            // If pause not yet completed, AND pause is actually requested...
            if (needsToWait && pauseReq)
            {
                m_conditionalsPack.PauseCompletedPack.WaitForTrue();
            }
        }

        /// <summary> Returns the number of tasks running on the thread task list.</summary>
        [[nodiscard]]
    	auto GetNumberOfTasks() const noexcept -> std::size_t
        {
            return m_taskList.Get().size();
        }

        /// <summary> Returns a copy of the last set immutable task list, it should mirror
        /// the tasks running on the thread. </summary>
        [[nodiscard]]
    	auto GetTaskSource() const noexcept -> TaskOpsProvider_t
        {
            return m_taskList;
        }

        /// <summary> Stops the thread, replaces the task list, creates the thread again. </summary>
        // TODO compare code gen between non-const param and const and ref
        void SetTaskSource(TaskOpsProvider_t newTaskList) noexcept
        {
            //StartDestruction();
            //WaitForDestruction();
            m_taskList = std::move(newTaskList);
            //CreateThread(m_taskList);
        }

    private:
    	/// <summary> Starts the work thread running, to execute each task in the list infinitely. </summary>
        /// <returns> true on thread created, false otherwise (usually thread already created). </returns>
        bool CreateThread(const TaskOpsProvider_t tasks, const bool isPausedOnStart = false) noexcept
        {
            if (m_workThreadObj == nullptr)
            {
                //reset some conditionals aka std::condition_variable 
                m_conditionalsPack.PauseCompletedPack.UpdateState(false);
                m_conditionalsPack.OrderedPausePack.UpdateState(isPausedOnStart);
                m_conditionalsPack.UnorderedPausePack.UpdateState(false);
                //make thread obj
                const auto getTaskListFn = [&]() { return m_taskList.Get(); };
                m_workThreadObj = std::make_unique<Thread_t>([=, this](std::stop_token st) { threadPoolFunc(st, getTaskListFn); });
                //make local handle to stop_source for thread
                m_stopSource = m_workThreadObj->get_stop_source();
                //update conditionals pack to have stop handle
                m_conditionalsPack.SetStopSource(m_stopSource);
                return true;
            }
            return false;
        }

        /// <summary> Destructs the running thread after it finishes running the current task it's on
        /// within the task list. Marks the thread func to stop then joins and waits for it to return. </summary>
        /// <remarks><b>WILL CLEAR the task source!</b> To start the thread again, just set a new task source.</remarks>
        void DestroyThread() noexcept
        {
            StartDestruction();
            WaitForDestruction();
            m_taskList = {};
        }

        /// <summary> Changes stop requested to true... </summary>
        /// <remarks> NOTE: Destruction will occur un-ordered!
        /// If you want the list of tasks in-progress to run until the end, then request an ordered pause first! </remarks>
        void StartDestruction() noexcept
        {
            m_stopSource.request_stop();
            m_conditionalsPack.OrderedPausePack.UpdateState(false);
            m_conditionalsPack.UnorderedPausePack.UpdateState(false);
            m_conditionalsPack.Notify();
        }

        /// <summary> Joins the work thread to the current thread and waits. </summary>
        void WaitForDestruction() noexcept
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
        void threadPoolFunc(const std::stop_token stopToken, const auto taskGetterFn) noexcept
        {
            const auto TestAndWaitForPauseEither = [](Conditionals_t& pauseObj)
            {
                // If either ordered or unordered pause set
                [[unlikely]]
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
            const auto TestAndWaitForPauseUnordered = [](Conditionals_t& pauseObj)
            {
                // If either ordered or unordered pause set
                [[unlikely]]
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
                // Get a copy of the task list before every iteration.
                const auto tasks = taskGetterFn();
                //test for ordered pause
                TestAndWaitForPauseEither(m_conditionalsPack);
                if (tasks.empty())
                {
                    std::this_thread::yield();
                }
                // Iterate task list, running tasks set for this thread.
                for (const auto& currentTask : tasks)
                {
                    //test for unordered pause request (before the fn call!)
                    TestAndWaitForPauseUnordered(m_conditionalsPack);
                    //double check outer condition here, as this may be long-running,
                    //causes destruction to occur unordered.
                    if constexpr (UseUnorderedDestruction)
                    {
                        if (stopToken.stop_requested())
                            break;
                    }
                    // run the task
                    currentTask();
                }
            }
        }
    };
}