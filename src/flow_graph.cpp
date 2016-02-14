#include <future>
#include <thread>

#include "flow_graph.hpp"

namespace flow_graph {

class InlineExecutor : public Executor {
  void run(std::function<void()> task) {
    task();
  }
};

class ThreadExecutor : public Executor {
  void run(std::function<void()> task) {
    std::thread(task).detach();
  }
};

SharedExecutor getInlineExecutor() {
  static SharedExecutor executor = std::make_shared<InlineExecutor>();
  return executor;
}

SharedExecutor getThreadExecutor() {
  static SharedExecutor executor = std::make_shared<ThreadExecutor>();
  return executor;
}

Continuation::Continuation(
    std::function<void(SharedExecutor)> task,
    unsigned int counter)
  : task_(task),
    counter_(counter) {
}

void Continuation::notifyAndRun(SharedExecutor executor) {
  if (--counter_ == 0) {
    auto task = task_;
    executor->run([executor, task] { task(executor); });
  }
}

}  // namespace flow_graph
