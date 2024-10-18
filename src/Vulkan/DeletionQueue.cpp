#include "DeletionQueue.h"

#include <ranges>

void DeletionQueue::push_back(std::function<void()> &&function)
{
    mDeletors.push_back(function);
}

void DeletionQueue::flush()
{
    for (auto &deletor : mDeletors | std::views::reverse)
    {
        deletor();
    }

    mDeletors.clear();
}