#pragma once

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <stdexcept>

#if _HAS_CXX20
#include <coroutine>
#else
#include <functional>
#endif

#include "noncopyable.h"

namespace raft
{

#if _HAS_CXX20
    // 任务队列
    template <typename T>
    class thread_saft_queue
    {
    private:
        std::mutex m_mutex;
        std::condition_variable m_cv;
        std::atomic_flag m_must_return_nullptr;
        std::queue<T> m_queue;

    public:
        thread_saft_queue() = default;
        ~thread_saft_queue() = default;

        void put(T e)
        {
            std::unique_lock<std::mutex> _(m_mutex);
            m_queue.emplace(e);
            m_cv.notify_one();
        }

        std::optional<T> take()
        {
            std::unique_lock<std::mutex> _(m_mutex);
            m_cv.wait(_, [q = this]
                      { return q->m_must_return_nullptr.test() || !q->m_queue.empty(); });

            if (m_must_return_nullptr.test())
                return {};

            T ret = m_queue.front();
            m_queue.pop();

            return ret;
        }

        std::queue<T> &destroy()
        {
            std::unique_lock<std::mutex> _(m_mutex);
            m_must_return_nullptr.test_and_set();
            m_cv.notify_all();
            return m_queue;
        }
    };

    template <typename T>
    struct future : std::future<T>
    {
        struct promise_type : std::promise<T>
        {
            std::future<T> get_return_object() { return this->get_future(); }
            std::suspend_never initial_suspend() { return {}; }
            std::suspend_never final_suspend() noexcept { return {}; }
            void return_void() {}
            void unhandled_exception() {}
        };

        future(std::future<T> &&f) : std::future<T>(std::move(f)) {}

#ifdef _MSC_VER
        future() = default;
#endif
    };

    class thread_pool : public noncopyable
    {
    private:
        thread_saft_queue<std::coroutine_handle<>> m_queue;
        std::queue<std::jthread> m_threads;

    public:
        static thread_pool &get(int thread_num)
        {
            static thread_pool tp{thread_num};
            return tp;
        }

        template <typename T>
        struct awaitable
        {
            using PT = future<T>::promise_type;
            std::coroutine_handle<PT> m_h = nullptr;

            bool await_ready()
            {
                return false;
            }
            void await_suspend(std::coroutine_handle<PT> h)
            {
                m_h = h;
                thread_pool::get(0).submit_coroutine(h);
            }
            std::coroutine_handle<PT> await_resume() { return m_h; }
        };

        template <std::invocable F>
        future<std::invoke_result_t<F>> submit(F task)
        {
            using RT = std::invoke_result_t<F>;
            using PT = future<RT>::promise_type;
            std::coroutine_handle<PT> h = co_await awaitable<RT>();

            if constexpr (std::is_void_v<RT>)
            {
                task();
            }
            else
            {
                h.promise().set_value(task());
            }
        }

        void submit_coroutine(std::coroutine_handle<> h) { m_queue.put(h); }

    private:
        thread_pool() = delete;
        thread_pool(int thread_num)
        {
            while (thread_num-- > 0)
            {
                m_threads.emplace(std::jthread([this]
                                               { this->worker(); }));
            }
        }

        ~thread_pool()
        {
            auto &q = m_queue.destroy();
            while (!m_threads.empty())
            {
                m_threads.pop();
            }

            while (!q.empty())
            {
                q.front().destroy();
                q.pop();
            }
        }

        void worker()
        {
            while (auto task = m_queue.take())
            {
                task.value().resume();
            }
        }
    };
#else
    class thread_pool : public noncopyable
    {
    private:
        std::mutex m_mutex;
        std::condition_variable m_cv;
        bool m_stop;
        std::queue<std::function<void()>> m_queue;
        std::vector<std::thread> m_threads;

    public:
        static thread_pool &get(int thread_num)
        {
            static thread_pool tp(thread_num);
            return tp;
        }

        template <typename F, typename... Args>
        auto submit(F &&f, Args &&...args) -> std::future<typename std::result_of<F(Args...)>::type>
        {
            using RT = typename std::result_of<F(Args...)>::type;

            auto task = std::make_shared<std::packaged_task<RT()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
            std::future<RT> res = task->get_future();
            {
                std::unique_lock<std::mutex> _(m_mutex);
                if (m_stop)
                    throw std::runtime_error("enqueue on stopped thread_pool");

                m_queue.emplace([task]()
                                { (*task)(); });
            }
            m_cv.notify_one();
            return res;
        }

    private:
        thread_pool() = delete;
        thread_pool(int thread_num) : m_stop(false)
        {
            while (thread_num-- > 0)
            {
                m_threads.emplace_back(std::thread([this]
                                                   { this->worker(); }));
            }
        }

        ~thread_pool()
        {
            {
                std::unique_lock<std::mutex> _(m_mutex);
                m_stop = true;
            }
            m_cv.notify_all();
            for (auto &worker : m_threads)
                worker.join();
        }

        void worker()
        {
            while (true)
            {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> _(m_mutex);
                    m_cv.wait(_, [this]
                              { return this->m_stop || !this->m_queue.empty(); });
                    if (m_stop && m_queue.empty())
                        return;
                    task = std::move(m_queue.front());
                    m_queue.pop();
                }

                task();
            }
        }
    };
#endif
}