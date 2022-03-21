#pragma once

namespace raft
{
    class noncopyable
    {
    public:
        noncopyable() = default;
        noncopyable(const noncopyable &) = delete;
        noncopyable(noncopyable &&) = delete;
        noncopyable &operator=(const noncopyable &) = delete;
        noncopyable &operator=(noncopyable &&) = delete;
        ~noncopyable() = default;
    };
}
