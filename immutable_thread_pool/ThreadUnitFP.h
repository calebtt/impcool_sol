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

namespace imp
{
    namespace pause
	{
        // A couple types for the variant (still better than an enum)
        struct OrderedPause_t
        {
            void operator()([[maybe_unused]] BoolCvPack& orderedState, [[maybe_unused]] BoolCvPack& unorderedState) const noexcept
            {
                orderedState.UpdateState(true);
                unorderedState.UpdateState(false);
            }
        };
        struct UnorderedPause_t
        {
            void operator()([[maybe_unused]] BoolCvPack& orderedState, [[maybe_unused]] BoolCvPack& unorderedState) const noexcept
            {
                unorderedState.UpdateState(true);
                orderedState.UpdateState(false);
            }
        };
        struct NeitherPause_t
        {
            void operator()([[maybe_unused]] BoolCvPack& orderedState, [[maybe_unused]] BoolCvPack& unorderedState) const noexcept
            {
                orderedState.UpdateState(false);
                unorderedState.UpdateState(false);
            }
        };
    }

    // Responsibiliity is the thread only
    struct ThreadContext_t
    {
        using Thread_t = std::jthread;
        using UniquePtrThread_t = std::unique_ptr<Thread_t>;
        UniquePtrThread_t workThreadObj;
        std::stop_source stopSource;
    };

    /// <summary> Fully functional nearly stand-alone class to manage a single thread that has a task list. Manages running a single thread pool
    /// thread. The thread can be paused, and destroyed. The task list can be simply returned as it is not mutated while in use, only copied,
    /// or counted. A low-level granular kind of access is desirable here, if possible. </summary>
    /// <remarks> This class is useable on it's own. There are two mutually exclusive pause conditions, each with their own setter function.
    /// Non-copyable, <b>is moveable! (move-construct and move-assign)</b></remarks>
    class ThreadUnitFP
    {
    private:
        /// <summary> Constant used to store the loop delay time period when no tasks are present. </summary>
        static constexpr std::chrono::milliseconds EmptyWaitTime{ std::chrono::milliseconds(20) };
    public:
        using Thread_t = std::jthread;
        using AtomicBool_t = std::atomic<bool>;
        using UniquePtrThread_t = std::unique_ptr<Thread_t>;
        // Alias for the ThreadTaskSource which provides a container and some operations.
        using TaskOpsProvider_t = imp::ThreadTaskSource;
        using TaskContainer_t = decltype(TaskOpsProvider_t::TaskList);
        using PauseMethod_t = std::variant<pause::OrderedPause_t, pause::UnorderedPause_t, pause::NeitherPause_t>;
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
        // Pack of items used for pause/unpause/pause-complete "events".
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
        ThreadUnitFP(const imp::ThreadTaskSource tasks = {}) noexcept
        {
            m_taskList = tasks;
            CreateThread(m_taskList, false);
        }

    	/// <summary> Dtor destroys the thread. </summary>
        ~ThreadUnitFP()
        {
            DestroyThread();
        }

        // Implemented move operations.
        ThreadUnitFP(ThreadUnitFP&& other) noexcept
	        : m_conditionalsPack(std::move(other.m_conditionalsPack)),
	          m_workThreadObj(std::move(other.m_workThreadObj)),
	          m_taskList(std::move(other.m_taskList)),
	          m_stopSource(std::move(other.m_stopSource))
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
        auto SetPauseValue(PauseMethod_t pauseMethod)
        {
            // The generic lambda here generates the overload for the specific type held by pauseMethod,
            // and with the type, calls the call operator with the ordered and unordered pause conditionals packs
            // in order to perform the appropriate operation for setting/unsetting the pause value.
            std::visit([&](auto& v) { v(m_conditionalsPack.OrderedPausePack, m_conditionalsPack.UnorderedPausePack); }, pauseMethod );
        }

        /// <summary> Generally if the thread is not running, there is an error state or it is destructing. </summary>
        [[nodiscard]]
    	bool IsRunning() const
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
        [[nodiscard]]
    	std::size_t GetNumberOfTasks() const
        {
            return m_taskList.TaskList.size();
        }

        /// <summary> Returns a copy of the last set immutable task list, it should mirror
        /// the tasks running on the thread. </summary>
        [[nodiscard]]
    	auto GetTaskSource() const
        {
            return m_taskList;
        }

        /// <summary> Stops the thread, replaces the task list, creates the thread again. </summary>
        void SetTaskSource(ThreadTaskSource newTaskList)
        {
            StartDestruction();
            WaitForDestruction();
            m_taskList = newTaskList;
            CreateThread(newTaskList);
        }

    private:
    	/// <summary> Starts the work thread running, to execute each task in the list infinitely. </summary>
        /// <returns> true on thread created, false otherwise (usually thread already created). </returns>
        bool CreateThread(const ThreadTaskSource tasks, const bool isPausedOnStart = false) noexcept
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

        /// <summary> Destructs the running thread after it finishes running the current task it's on
        /// within the task list. Marks the thread func to stop then joins and waits for it to return. </summary>
        /// <remarks><b>WILL CLEAR the task source!</b> To start the thread again, just set a new task source.</remarks>
        void DestroyThread()
        {
            StartDestruction();
            WaitForDestruction();
            m_taskList.TaskList = {};
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