#include "thread_pool.h"

#include <iostream>

#if _HAS_CXX20

#include <chrono>
#include <string>
#include <syncstream>

void a_oridinary_function_return_nothing()
{
    std::osyncstream(std::cout) << __func__ << std::endl;
}

std::string a_oridinary_function_return_string()
{
    return std::string(__func__);
}

raft::future<void> a_coroutine_return_nothing()
{
    co_await raft::thread_pool::awaitable<void>();
    std::osyncstream(std::cout) << __func__ << std::endl;
}

raft::future<std::string> a_coroutine_return_string()
{
    auto h = co_await raft::thread_pool::awaitable<std::string>();
    h.promise().set_value(__func__);
}

raft::future<std::string> a_coroutine_return_string_with_input(int x)
{
    auto h = co_await raft::thread_pool::awaitable<std::string>();
    h.promise().set_value(std::string(__func__) + ":" + std::to_string(x));
}

std::string a_function_calling_a_coroutine()
{
    auto r = a_coroutine_return_string();
    return r.get() + " in " + __func__;
}

// You can submit your coroutine handle in your own awaitable
// This implementation is a simplified version of thread_pool::awaitable
struct submit_awaitable : std::suspend_never
{
    void await_suspend(std::coroutine_handle<> h)
    {
        raft::thread_pool::get(0).submit_coroutine(h);
    }
};

raft::future<void> submit_raw_coroutine_handle()
{
    co_await submit_awaitable();
    std::osyncstream(std::cout) << __func__ << std::endl;
}

int main()
{
    constexpr auto N = 3;
    // get thread pool singletion
    auto &tpool = raft::thread_pool::get(N);

    // Ordinary function
    tpool.submit(a_oridinary_function_return_nothing);
    auto funt_ret_sth = tpool.submit(a_oridinary_function_return_string);

    // Coroutine
    tpool.submit(a_coroutine_return_nothing);
    auto coro_ret_sth = tpool.submit(a_coroutine_return_string);
    auto coro_ret_sth_with_input = tpool.submit([]()
                                                { return a_coroutine_return_string_with_input(42); });

    // Funciont calling coroutine
    auto func_calling_coro = tpool.submit(a_function_calling_a_coroutine);

    // Raw coroutine handle
    submit_raw_coroutine_handle();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Also support lambda
    for (int i = 0; i <= N; ++i)
    {
        tpool.submit([i]() -> int
                     {
                        std::osyncstream(std::cout) << "* Task " << i << "+" << std::endl;
                        std::this_thread::sleep_for(std::chrono::seconds(3));
                        std::osyncstream(std::cout) << "* Task " << i << "-" << std::endl;
                        return i; });
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::osyncstream(std::cout) << funt_ret_sth.get() << std::endl;
    std::osyncstream(std::cout) << coro_ret_sth.get().get() << std::endl;
    std::osyncstream(std::cout) << coro_ret_sth_with_input.get().get() << std::endl;
    std::osyncstream(std::cout) << func_calling_coro.get() << std::endl;

    // Destructor of thread_pool blocks until tasks current executing completed
    // Tasks which are still in queue will not be executed
    // So above lambda example, Task 3 is not executed

    return 0;
}
#else
int main()
{
    auto &tpool = raft::thread_pool::get(4);
    std::vector<std::future<int>> results;

    for (int i = 0; i < 8; ++i)
    {
        results.emplace_back(tpool.submit([i]
                                          {
                                            std::cout << "hello " << i << std::endl;
                                            std::this_thread::sleep_for(std::chrono::seconds(1));
                                            std::cout << "world " << i << std::endl;
                                            return i * i; }));
    }

    for (auto &&result : results)
        std::cout << result.get() << " ";
    std::cout << std::endl;

    return 0;
}
#endif
