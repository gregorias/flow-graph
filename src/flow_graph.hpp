#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace flow_graph {

/**
 * An interface for running tasks.
 */
class Executor {
 public:
  virtual ~Executor() {}
  virtual void run(std::function<void()> task) = 0;
};

using SharedExecutor = std::shared_ptr<Executor>;

SharedExecutor getInlineExecutor();
SharedExecutor getThreadExecutor();

/**
 * Continuation is a task that waits for its dependencies to be ready.
 *
 * Providers of dependencies should call notifyAndRun() once they are ready.
 *
 * This class is thread-safe.
 */
class Continuation {
 public:
  Continuation(
      std::function<void(SharedExecutor)> task,
      unsigned int counter);

  /**
   * notifyAndRun should be called once a dependency of this task is ready.
   *
   * The task runs, using executor, iff all dependencies are satisfied.
   *
   * The behavior of notifyAndRun when called more times than there are
   * dependencies is undefined.
   */
  void notifyAndRun(SharedExecutor executor);

 private:
  std::function<void(SharedExecutor)> task_;
  std::atomic_uint counter_;
};

template <class T> class Promise;

/**
 * Future is a flow graph equivalent of std::future. Future differs from a
 * std::future in that it implements an observer pattern -- once the promise
 * puts value, observing continuations are notified and run.
 */
template <class T>
class Future {
 public:
  ~Future() = default;
  Future(Future&& other) = default;

  Future(const Future& other) = delete;
  Future& operator=(const Future& other) = delete;

  /**
   * If ready, returns value stored in the future. Otherwise the behavior is
   * undefined.
   */
  T get() const;

  /**
   * If not ready, adds a continuation to be used once value is present.
   * Added continuations will be notified once a value is put to this Future.
   *
   * If ready, the continuation will be notified and run immediately with an
   * inline executor.
   */
  void listen(std::shared_ptr<Continuation> continuation);

  /**
   * ready returns true iff the value is ready. Subsequent calls to get() will
   * not block.
   */
  bool ready() const;

  /**
   * Blocks till this Future is ready.
   */
  void wait() const;

  template <class Rep, class Period>
  bool waitFor(
      const std::chrono::duration<Rep, Period>& timeoutDuration) const;

  template <class Clock, class Duration>
  bool waitUntil(
      const std::chrono::time_point<Clock, Duration>& timeoutTime) const;
 private:
  friend class Promise<T>;

  mutable std::mutex mutex_;
  mutable std::condition_variable cv_;
  std::unique_ptr<T> value_;
  std::vector<std::shared_ptr<Continuation>>  continuations_;

  Future() = default;

  void put(const T& arg, SharedExecutor executor);
};

template <class T>
using SharedFuture = std::shared_ptr<Future<T>>;

/**
 * Promise is a flow graph equivalent of std::promise. Each Promise has one
 * Future associated with the Promise.
 */
template <class T>
class Promise {
 public:
  Promise();
  Promise(Promise<T>&& other) = default;

  Promise(const Promise<T>& other) = delete;
  Promise<T>& operator=(const Promise<T>& other) = delete;

  SharedFuture<T> getFuture() const;

  /**
   * Puts the value for the associated Future to return. Uses executor to run
   * dependency tasks that are ready once after this put.
   *
   * The behavior of put is undefined if this is called more than once.
   */
  void put(const T& arg, SharedExecutor executor=getInlineExecutor());
 private:
  const SharedFuture<T> future_;
};

/**
 * Lift functions
 */
/**
 * Returns an action running the provided function with executor and a Future
 * for the result.
 * lift0 is a special case of lift, since no dependency is needed. It is
 * provided for convenience.
 */
template <class R>
std::pair<std::function<void()>, SharedFuture<R>> lift0(
    std::function<R()> func,
    SharedExecutor executor);

template <class R, class A0>
SharedFuture<R> lift1(
    std::function<R(A0)> func,
    SharedFuture<A0> arg0);

template <class R, class A0, class A1>
SharedFuture<R> lift2(
    std::function<R(A0, A1)> func,
    SharedFuture<A0> arg0,
    SharedFuture<A1> arg1);

template <class R, class A0, class A1, class A2>
SharedFuture<R> lift3(
    std::function<R(A0, A1, A2)> func,
    SharedFuture<A0> arg0,
    SharedFuture<A1> arg1,
    SharedFuture<A2> arg2);

template <class R, class A0, class A1, class A2, class A3>
SharedFuture<R> lift4(
    std::function<R(A0, A1, A2, A3)> func,
    SharedFuture<A0> arg0,
    SharedFuture<A1> arg1,
    SharedFuture<A2> arg2,
    SharedFuture<A3> arg3);

template <class R, class A0, class A1, class A2, class A3, class A4>
SharedFuture<R> lift5(
    std::function<R(A0, A1, A2, A3, A4)> func,
    SharedFuture<A0> arg0,
    SharedFuture<A1> arg1,
    SharedFuture<A2> arg2,
    SharedFuture<A3> arg3,
    SharedFuture<A4> arg4);

}  // namespace flow_graph

namespace flow_graph {

using std::lock_guard;
using std::move;
using std::mutex;
using std::unique_lock;
using std::vector;

template <class T>
T Future<T>::get() const {
  lock_guard<mutex> lock(mutex_);
  return *value_;
}

template <class T>
void Future<T>::listen(std::shared_ptr<Continuation> continuation) {
  bool isValid = false;
  {
    lock_guard<mutex> lock(mutex_);
    isValid = value_.get() != nullptr;
    if (!isValid) {
      continuations_.push_back(continuation);
    }
  }
  if (isValid) {
    continuation->notifyAndRun(getInlineExecutor());
  }
}

template <class T>
bool Future<T>::ready() const {
  lock_guard<mutex> lock(mutex_);
  return value_.get() != nullptr;
}

template <class T>
void Future<T>::wait() const {
  unique_lock<mutex> lock(mutex_);
  cv_.wait(lock, [this] { return this->value_.get() != nullptr; });
}

template <class T>
template <class Rep, class Period>
bool Future<T>::waitFor(
    const std::chrono::duration<Rep, Period>& timeoutDuration) const {
  unique_lock<mutex> lock(mutex_);
  return cv_.wait_for(lock, timeoutDuration,
      [this] { return this->value_.get() != nullptr; });
}

template <class T>
void Future<T>::put(const T& arg, SharedExecutor executor) {
  vector<std::shared_ptr<Continuation>> continuationsAux;
  {
    unique_lock<mutex> lock(mutex_);
    value_.reset(new T(arg));
    continuationsAux = std::move(continuations_);
  }
  cv_.notify_all();
  for (const auto& continuation : continuationsAux) {
    continuation->notifyAndRun(executor);
  }
}

template <class T>
Promise<T>::Promise() : future_(new Future<T>()) {
}

template <class T>
SharedFuture<T> Promise<T>::getFuture() const {
  return future_;
}

template <class T>
void Promise<T>::put(const T& arg, SharedExecutor executor) {
  future_->put(arg, executor);
}

template <class R>
std::pair<std::function<void()>, SharedFuture<R>> lift0(
    std::function<R()> func,
    SharedExecutor executor) {
  // Using shared_ptr, because std::function is copyable, but Promise is not.
  auto promise =std::make_shared<Promise<R>>();
  SharedFuture<R> future = promise->getFuture();

  auto task = [func, promise, executor] {
    promise->put(func(), executor);
  };

  auto runTask = [task, executor] {
    executor->run(task);
  };

  return std::make_pair(runTask, future);
}

template <class R, class A0>
SharedFuture<R> lift1(
    std::function<R(A0)> func,
    SharedFuture<A0> arg0) {
  // Using shared_ptr, because std::function is copyable, but Promise is not.
  auto promise = std::make_shared<Promise<R>>();
  SharedFuture<R> future = promise->getFuture();

  std::function<void(SharedExecutor)> funcTask =
      [func, arg0, promise]
          (SharedExecutor executor) {
        promise->put(func(arg0->get()), executor);
      };

  arg0->listen(std::make_shared<Continuation>(std::move(funcTask), 1));

  return future;
}

template <class R, class A0, class A1>
SharedFuture<R> lift2(std::function<R(A0, A1)> func,
    SharedFuture<A0> arg0,
    SharedFuture<A1> arg1) {
  // Using shared_ptr, because std::function is copyable, but Promise is not.
  auto promise = std::make_shared<Promise<R>>();
  SharedFuture<R> future = promise->getFuture();

  std::function<void(SharedExecutor)> funcTask =
      [func, arg0, arg1, promise]
          (SharedExecutor executor) {
        promise->put(func(arg0->get(), arg1->get()), executor);
      };

  std::shared_ptr<Continuation> continuation =
      std::make_shared<Continuation>(std::move(funcTask), 2);
  arg0->listen(continuation);
  arg1->listen(continuation);

  return future;
}

template <class R, class A0, class A1, class A2>
SharedFuture<R> lift3(
    std::function<R(A0, A1, A2)> func,
    SharedFuture<A0> arg0,
    SharedFuture<A1> arg1,
    SharedFuture<A2> arg2) {
  // Using shared_ptr, because std::function is copyable, but Promise is not.
  auto promise = std::make_shared<Promise<R>>();
  SharedFuture<R> future = promise->getFuture();

  const std::function<void(SharedExecutor)>& funcTask =
      [func, arg0, arg1, arg2, promise]
          (SharedExecutor executor) {
        promise->put(func(arg0->get(), arg1->get(), arg2->get()), executor);
      };

  std::shared_ptr<Continuation> continuation =
      std::make_shared<Continuation>(std::move(funcTask), 3);
  arg0->listen(continuation);
  arg1->listen(continuation);
  arg2->listen(continuation);

  return future;
}

template <class R, class A0, class A1, class A2, class A3>
SharedFuture<R> lift4(
    std::function<R(A0, A1, A2, A3)> func,
    SharedFuture<A0> arg0,
    SharedFuture<A1> arg1,
    SharedFuture<A2> arg2,
    SharedFuture<A3> arg3) {
  // Using shared_ptr, because std::function is copyable, but Promise is not.
  auto promise = std::make_shared<Promise<R>>();
  SharedFuture<R> future = promise->getFuture();

  const std::function<void(SharedExecutor)>& funcTask =
      [func, arg0, arg1, arg2, arg3, promise]
          (SharedExecutor executor) {
        promise->put(func(arg0->get(), arg1->get(), arg2->get(), arg3->get()),
            executor);
      };

  std::shared_ptr<Continuation> continuation =
      std::make_shared<Continuation>(std::move(funcTask), 4);
  arg0->listen(continuation);
  arg1->listen(continuation);
  arg2->listen(continuation);
  arg3->listen(continuation);

  return future;
}

template <class R, class A0, class A1, class A2, class A3, class A4>
SharedFuture<R> lift5(
    std::function<R(A0, A1, A2, A3, A4)> func,
    SharedFuture<A0> arg0,
    SharedFuture<A1> arg1,
    SharedFuture<A2> arg2,
    SharedFuture<A3> arg3,
    SharedFuture<A4> arg4) {
  // Using shared_ptr, because std::function is copyable, but Promise is not.
  auto promise = std::make_shared<Promise<R>>();
  SharedFuture<R> future = promise->getFuture();

  const std::function<void(SharedExecutor)>& funcTask =
      [func, arg0, arg1, arg2, arg3, arg4, promise]
          (SharedExecutor executor) {
        promise->put(
            func(arg0->get(), arg1->get(), arg2->get(), arg3->get(),
                arg4->get()),
            executor);
      };

  std::shared_ptr<Continuation> continuation =
      std::make_shared<Continuation>(std::move(funcTask), 5);
  arg0->listen(continuation);
  arg1->listen(continuation);
  arg2->listen(continuation);
  arg3->listen(continuation);
  arg4->listen(continuation);

  return future;
}

}  // namespace flow_graph
