#pragma once

#include "SyncQueue.h"

#include <functional>
#include <optional>
#include <thread>

class ThreadPool {
  public:
    using Task    = std::function<void()>;
    using OptTask = std::optional<Task>;

  public:
    ThreadPool()
    {
        const auto numWorkers = std::thread::hardware_concurrency() - 1;

        auto doWork = [&]() {
            while (true)
            {
                const auto task = mTasks.Pop();

                if (task.has_value())
                {
                    (*task)();
                }

                else
                    return;
            }
        };

        for (size_t i = 0; i < numWorkers; i++)
        {
            mWorkers.emplace_back(doWork);
        }
    }

    ~ThreadPool()
    {
        for (size_t i = 0; i < mWorkers.size(); i++)
        {
            mTasks.Push(std::nullopt);
        }
    }

    void Push(const Task &task)
    {
        mTasks.Push(task);
    }

  private:
    SyncQueue<OptTask> mTasks;

    std::vector<std::jthread> mWorkers;
};