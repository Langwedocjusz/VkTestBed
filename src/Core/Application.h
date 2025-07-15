#pragma once

#include <memory>

class Application {
  public:
    Application();
    ~Application();

    void Run();

  private:
    class Impl;
    std::unique_ptr<Impl> mPImpl;
};