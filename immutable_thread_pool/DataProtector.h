#pragma once
#include <mutex>

namespace impcool
{
    template<typename ProtectedData, typename MutexType = std::mutex>
    class DataProtector
    {
    private:
        ProtectedData* m_data;
        MutexType* m_mut;
    public:
        DataProtector(ProtectedData& data, MutexType &mut) : m_data(data), m_mut(mut) {}
        ProtectedData* get()
        {
            m_mut->lock();
            return m_data;
        }
        ~DataProtector()
        {
            m_mut->unlock();
        }
    };
}