#pragma once

#include <deque>
#include <functional>

class DeletionQueue {
  public:
    DeletionQueue() = default;

    void push_back(std::function<void()> &&function);
    void flush();

  private:
    std::deque<std::function<void()>> mDeletors;
};