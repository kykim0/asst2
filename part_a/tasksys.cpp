#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "tasksys.h"


IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {}
ITaskSystem::~ITaskSystem() {}

/*
 * ================================================================
 * Serial task system implementation
 * ================================================================
 */

const char* TaskSystemSerial::name() {
  return "Serial";
}

TaskSystemSerial::TaskSystemSerial(int num_threads): ITaskSystem(num_threads) {
}

TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable* runnable, int num_total_tasks) {
  for (int i = 0; i < num_total_tasks; i++) {
    runnable->runTask(i, num_total_tasks);
  }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                          const std::vector<TaskID>& deps) {
  return 0;
}

void TaskSystemSerial::sync() {
  return;
}

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelSpawn::name() {
  return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads)
  : ITaskSystem(num_threads), num_threads_(num_threads) {
  //
  // TODO: CS149 student implementations may decide to perform setup
  // operations (such as thread pool construction) here.
  // Implementations are free to add new class member variables
  // (requiring changes to tasksys.h).
  //
  threads_ = new std::thread[num_threads_];
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {
  delete[] threads_;
}

void runTaskThread(const int num_total_tasks, std::atomic<int>& task_id,
                   IRunnable* runnable) {
  while (true) {
    const int curr_task_id = task_id++;
    if (curr_task_id >= num_total_tasks) return;
    runnable->runTask(curr_task_id, num_total_tasks);
  }
}

void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {
  //
  // TODO: CS149 students will modify the implementation of this
  // method in Part A.  The implementation provided below runs all
  // tasks sequentially on the calling thread.
  //
  std::atomic<int> task_id{0};
  for (int i = 0; i < num_threads_; ++i) {
    threads_[i] = std::thread(runTaskThread, num_total_tasks, std::ref(task_id),
                              runnable);
  }
  for (int i = 0; i < num_threads_; ++i) {
    threads_[i].join();
  }
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                 const std::vector<TaskID>& deps) {
  return 0;
}

void TaskSystemParallelSpawn::sync() {
  return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSpinning::name() {
  return "Parallel + Thread Pool + Spin";
}

void runTaskThreadSpin(const bool& done,
                       const int& num_total_tasks,
                       int& task_id,
                       std::atomic<int>& num_total_done,
                       std::mutex& state_lock,
                       IRunnable** runnable) {
  while (!done) {
    state_lock.lock();
    const int curr_task_id = task_id;
    if (curr_task_id < num_total_tasks) {
      task_id++;
      state_lock.unlock();

      IRunnable* curr_runnable = *runnable;
      curr_runnable->runTask(curr_task_id, num_total_tasks);
      ++num_total_done;
    } else {
      state_lock.unlock();
    }
  }
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads)
  : ITaskSystem(num_threads), num_threads_(num_threads), state_lock_(),
    threads_(nullptr), done_(false), curr_task_id_(0), num_total_done_(0),
    num_total_tasks_(0), curr_runnable_(nullptr) {
  //
  // TODO: CS149 student implementations may decide to perform setup
  // operations (such as thread pool construction) here.
  // Implementations are free to add new class member variables
  // (requiring changes to tasksys.h).
  //
  threads_ = new std::thread[num_threads_];
  for (int i = 0; i < num_threads_; ++i) {
    threads_[i] = std::thread(runTaskThreadSpin,
                              std::ref(done_),
                              std::ref(num_total_tasks_),
                              std::ref(curr_task_id_),
                              std::ref(num_total_done_),
                              std::ref(state_lock_),
                              &curr_runnable_);
  }
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
  done_ = true;
  for (int i = 0; i < num_threads_; ++i) {
    threads_[i].join();
  }
  delete[] threads_;
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {
  //
  // TODO: CS149 students will modify the implementation of this
  // method in Part A.  The implementation provided below runs all
  // tasks sequentially on the calling thread.
  //

  // Initialize the state atomically.
  state_lock_.lock();
  num_total_tasks_ = num_total_tasks;
  curr_task_id_ = 0;
  num_total_done_ = 0;
  curr_runnable_ = runnable;
  state_lock_.unlock();

  while (true) {
    const bool is_complete = (num_total_done_ >= num_total_tasks_);
    if (is_complete) { break; }
  }
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
  return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
  return;
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSleeping::name() {
  return "Parallel + Thread Pool + Sleep";
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads)
  : ITaskSystem(num_threads), num_threads_(num_threads) {
  //
  // TODO: CS149 student implementations may decide to perform setup
  // operations (such as thread pool construction) here.
  // Implementations are free to add new class member variables
  // (requiring changes to tasksys.h).
  //
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
  //
  // TODO: CS149 student implementations may decide to perform cleanup
  // operations (such as thread pool shutdown construction) here.
  // Implementations are free to add new class member variables
  // (requiring changes to tasksys.h).
  //
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {
  //
  // TODO: CS149 students will modify the implementation of this
  // method in Parts A and B.  The implementation provided below runs all
  // tasks sequentially on the calling thread.
  //
  for (int i = 0; i < num_total_tasks; i++) {
    runnable->runTask(i, num_total_tasks);
  }
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {

  //
  // TODO: CS149 students will implement this method in Part B.
  //
  return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync() {
  //
  // TODO: CS149 students will modify the implementation of this method in Part B.
  //
  return;
}
