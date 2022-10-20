#pragma once
#include <ranges>
#include <type_traits>
#include <functional>

#include "ThreadTaskSource.h"

namespace imp
{
    /// <summary> Concept for a ThreadUnit describing operations that must be supported. </summary>
    template<typename ThreadType_t>
    concept IsThreadUnit = requires (ThreadType_t & t)
    {
        { t.SetPauseValueOrdered(true) };
        { t.SetPauseValueUnordered(true) };
        { t.WaitForPauseCompleted() };
        { t.DestroyThread() };
        { t.GetTaskSource() };
        //{ t.SetTaskSource({}) };
        { t.GetNumberOfTasks() };
    };
}
