#pragma once
#include <variant>

#include "BoolCvPack.h"

namespace imp
{
	/**
	* \brief To block one or more threads until another thread both modifies a shared variable (the condition) and notifies the condition_variable.
	* For each pause state, requested-ordered, requested-unordered, pause-completed.
	*/
	struct ThreadConditionals
    {
        imp::BoolCvPack OrderedPausePack;
        imp::BoolCvPack UnorderedPausePack;
        imp::BoolCvPack PauseCompletedPack;
        ThreadConditionals(const std::stop_source stopSource)
        {
            SetStopSource(stopSource);
        }
        // Waits for both pause requests to be false.
        void WaitForBothPauseRequestsFalse() noexcept
        {
            OrderedPausePack.WaitForFalse();
            UnorderedPausePack.WaitForFalse();
        }
        void Notify() noexcept
        {
            OrderedPausePack.task_running_cv.notify_all();
            UnorderedPausePack.task_running_cv.notify_all();
            PauseCompletedPack.task_running_cv.notify_all();
        }
        void SetStopSource(const std::stop_source sts) noexcept
        {
            PauseCompletedPack.stop_source = sts;
            OrderedPausePack.stop_source = sts;
            UnorderedPausePack.stop_source = sts;
        }
    };

    inline
    void DoOrderedPause(ThreadConditionals& conds) noexcept
    {
        conds.OrderedPausePack.UpdateState(true);
        conds.UnorderedPausePack.UpdateState(false);
        conds.Notify();
    }
    inline
    void DoUnorderedPause(ThreadConditionals& conds) noexcept
    {
        conds.UnorderedPausePack.UpdateState(true);
        conds.OrderedPausePack.UpdateState(false);
        conds.Notify();
    }
    inline
    void DoUnpause(ThreadConditionals& conds) noexcept
    {
        conds.UnorderedPausePack.UpdateState(false);
        conds.OrderedPausePack.UpdateState(false);
        conds.PauseCompletedPack.UpdateState(false);
        conds.Notify();
    }
    inline
	bool IsPausing(const ThreadConditionals& conds) noexcept
    {
        return conds.OrderedPausePack.GetState() || conds.UnorderedPausePack.GetState();
    }
    inline
	bool IsPauseCompleted(const ThreadConditionals& conds) noexcept
    {
        return conds.PauseCompletedPack.GetState();
    }
}
