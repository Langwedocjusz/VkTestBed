#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

template <typename T>
class SyncQueue {
  public:
    SyncQueue() = default;

    template <typename U>
    void Push(U &&elem)
    {
        {
            std::lock_guard lock(mMutex);
            mQueue.push(std::forward<U>(elem));
        }

        mCV.notify_one();
    }

    /// Attempts to pop from queue, returns nullopt if empty.
    std::optional<T> TryPop()
    {
        std::lock_guard lock(mMutex);

        if (mQueue.empty())
            return std::nullopt;
        else
        {
            auto res = std::move(mQueue.front());
            mQueue.pop();
            return res;
        }
    }

    /// Waits until queue is non-empty, then pops
    T Pop()
    {
        std::unique_lock lock(mMutex);

        mCV.wait(lock, [&] { return !mQueue.empty(); });

        auto res = std::move(mQueue.front());
        mQueue.pop();
        return res;
    }

    bool Empty()
    {
        std::lock_guard lock(mMutex);
        return mQueue.empty();
    }

  private:
    std::queue<T> mQueue;

    std::mutex              mMutex;
    std::condition_variable mCV;
};