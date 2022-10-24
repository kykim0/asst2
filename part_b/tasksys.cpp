#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <set>

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
  for (int i = 0; i < num_total_tasks; i++) {
    runnable->runTask(i, num_total_tasks);
  }

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

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads): ITaskSystem(num_threads) {
  // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {
  // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
  for (int i = 0; i < num_total_tasks; i++) {
    runnable->runTask(i, num_total_tasks);
  }
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                 const std::vector<TaskID>& deps) {
  // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
  for (int i = 0; i < num_total_tasks; i++) {
    runnable->runTask(i, num_total_tasks);
  }

  return 0;
}

void TaskSystemParallelSpawn::sync() {
  // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
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

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads): ITaskSystem(num_threads) {
  // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {
  // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
  for (int i = 0; i < num_total_tasks; i++) {
    runnable->runTask(i, num_total_tasks);
  }
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
  // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
  for (int i = 0; i < num_total_tasks; i++) {
    runnable->runTask(i, num_total_tasks);
  }

  return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
  // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
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

void runTaskThreadSleep(const bool& done,
                        std::mutex& work_cv_lock,
                        std::condition_variable& work_cv,
                        std::condition_variable& sync_cv,
                        std::mutex& task_lock,
                        std::set<TaskState*>& work_q,
                        std::deque<TaskState*>& ready_q,
                        int& max_done_id,
                        std::atomic<int>& num_threads_exited) {
  while (!done) {
    // Look for a task from the ready to queue to run.
    task_lock.lock();
    if (ready_q.empty()) {
      task_lock.unlock();
      // Sleep if the ready queue is empty.
      std::unique_lock<std::mutex> work_cv_lk(work_cv_lock);
      work_cv.wait(work_cv_lk, [&]() { return done || !ready_q.empty(); });
      continue;
    }
    TaskState* curr_task = ready_q.front();
    task_lock.unlock();

    curr_task->state_lock_.lock();
    const int curr_task_id = curr_task->curr_task_id_;
    const int num_total_tasks = curr_task->num_total_tasks_;
    if (curr_task_id < num_total_tasks) {
      curr_task->curr_task_id_++;
      curr_task->state_lock_.unlock();

      IRunnable* task_runnable = curr_task->runnable_;
      task_runnable->runTask(curr_task_id, num_total_tasks);
      std::atomic<int>& num_total_done = curr_task->num_total_done_;
      const bool is_complete = (++num_total_done >= num_total_tasks);
      if (is_complete) {
        task_lock.lock();
        max_done_id = std::max(max_done_id, curr_task->id_);
        ready_q.pop_front();
        if (!ready_q.empty()) {
          task_lock.unlock();
          continue;
        }

        // Iterate over the work queue to see if there is a task that can run.
        std::set<TaskState*>::iterator it = work_q.begin();
        while (it != work_q.end()) {
          TaskState* task = *it++;
          if (task->max_dep_id_ > max_done_id) { break; }
        }
        if (it != work_q.begin()) {
          ready_q.insert(ready_q.end(), work_q.begin(), it);
          work_q.erase(work_q.begin(), it);
          work_cv.notify_all();
        }
        if (ready_q.empty()) {
          sync_cv.notify_all();
        }
        task_lock.unlock();
      }
    } else {
      curr_task->state_lock_.unlock();
    }
  }
  ++num_threads_exited;
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads)
  : ITaskSystem(num_threads), id_(0), num_threads_(num_threads), done_(false),
    threads_(nullptr), work_cv_lock_(), work_cv_(), sync_cv_(),
    num_threads_exited_(0), task_lock_(), work_q_(), ready_q_(),
    max_done_id_(0) {
  //
  // TODO: CS149 student implementations may decide to perform setup
  // operations (such as thread pool construction) here.
  // Implementations are free to add new class member variables
  // (requiring changes to tasksys.h).
  //
  threads_ = new std::thread[num_threads_];
  for (int i = 0; i < num_threads_; ++i) {
    threads_[i] = std::thread(runTaskThreadSleep,
                              std::ref(done_),
                              std::ref(work_cv_lock_),
                              std::ref(work_cv_),
                              std::ref(sync_cv_),
                              std::ref(task_lock_),
                              std::ref(work_q_),
                              std::ref(ready_q_),
                              std::ref(max_done_id_),
                              std::ref(num_threads_exited_));
  }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
  //
  // TODO: CS149 student implementations may decide to perform cleanup
  // operations (such as thread pool shutdown construction) here.
  // Implementations are free to add new class member variables
  // (requiring changes to tasksys.h).
  //
  done_ = true;
  while (num_threads_exited_ < num_threads_) { work_cv_.notify_all(); }
  for (int i = 0; i < num_threads_; ++i) {
    threads_[i].join();
  }
  delete[] threads_;
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {
  //
  // TODO: CS149 students will modify the implementation of this
  // method in Parts A and B.  The implementation provided below runs all
  // tasks sequentially on the calling thread.
  //
  runAsyncWithDeps(runnable, num_total_tasks, {});
  sync();
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
  //
  // TODO: CS149 students will implement this method in Part B.
  //
  // Initialize the state atomically.
  const TaskID new_id = ++id_;
  TaskState* new_task_state = new TaskState(new_id, runnable, num_total_tasks,
                                            deps);
  task_lock_.lock();
  // Because of the logic in runTaskThreadSleep to sleep when the ready queue is
  // empty, we add the task to the ready queue in this case.
  if (new_task_state->max_dep_id_ < max_done_id_ || ready_q_.empty()) {
    ready_q_.push_back(new_task_state);
  } else {
    work_q_.insert(new_task_state);
  }
  task_lock_.unlock();
  work_cv_.notify_all();
  return new_id;
}

void TaskSystemParallelThreadPoolSleeping::sync() {
  //
  // TODO: CS149 students will modify the implementation of this method in Part B.
  //
  std::unique_lock<std::mutex> task_lk(task_lock_);
  sync_cv_.wait(task_lk, [this]{ return ready_q_.empty(); });
  return;
}
