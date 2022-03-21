#pragma once

#include "noncopyable.h"

#include <mutex>
#include <functional>
#include <map>
#include <set>
#include <memory>

namespace raft
{
    template <typename T>
    class objfactory : public std::enable_shared_from_this<objfactory<T>>, noncopyable
    {
    private:
        std::mutex m_mutex;
        std::map<int, std::weak_ptr<T>> m_map;
        std::set<int> m_set;

    public:
        template <typename... Args>
        std::shared_ptr<T> Get(int id, Args &&...args)
        {
            std::unique_lock<std::mutex> _(m_mutex);
            std::weak_ptr<T> &ptr = m_map[id];
            std::shared_ptr<T> ret = ptr.lock();
            if (!ret)
            {
                ret.reset(new T(id, std::forward<Args>(args)...), std::bind(&objfactory::DeleteObj, this->shared_from_this(), std::placeholders::_1));
                ptr = ret;
                m_set.insert(id);
            }
            return ret;
        }

        const std::set<int> &GetAllObjKey() const { return m_set; }

    private:
        static void DeleteObj(const std::weak_ptr<objfactory<T>> &factory, T *ptr)
        {
            if (!ptr)
                return;
            std::shared_ptr<objfactory<T>>(factory.lock())->RemoveObj(ptr);
        }

        void RemoveObj(T *ptr)
        {
            std::unique_lock<std::mutex> _(m_mutex);
            auto it = m_map.find(ptr->key());
            if (it != m_map.end() && it->second.expired())
            {
                m_map.erase(ptr->key());
                m_set.erase(ptr->key());
            }
            delete ptr;
        }
    };
}
