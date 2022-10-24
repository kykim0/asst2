#ifndef _TASKSYS_H
#define _TASKSYS_H

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <set>

#include "itasksys.h"

/*
 * TaskSystemSerial: This class is the student's implementation of a
 * serial task execution engine.  See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemSerial : public ITaskSystem {
 public:
  TaskSystemSerial(int num_threads);
  ~TaskSystemSerial();
  const char* name();
  void run(IRunnable* runnable, int num_total_tasks);
  TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                          const std::vector<TaskID>& deps);
  void sync();
};

/*
 * TaskSystemParallelSpawn: This class is the student's implementation of a
 * parallel task execution engine that spawns threads in every run()
 * call.  See definition of ITaskSystem in itasksys.h for documentation
 * of the ITaskSystem interface.
 */
class TaskSystemParallelSpawn : public ITaskSystem {
 public:
  TaskSystemParallelSpawn(int num_threads);
  ~TaskSystemParallelSpawn();
  const char* name();
  void run(IRunnable* runnable, int num_total_tasks);
  TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                          const std::vector<TaskID>& deps);
  void sync();
};

/*
 * TaskSystemParallelThreadPoolSpinning: This class is the student's
 * implementation of a parallel task execution engine that uses a
 * thread pool. See definition of ITaskSystem in itasksys.h for
 * documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSpinning : public ITaskSystem {
 public:
  TaskSystemParallelThreadPoolSpinning(int num_threads);
  ~TaskSystemParallelThreadPoolSpinning();
  const char* name();
  void run(IRunnable* runnable, int num_total_tasks);
  TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                          const std::vector<TaskID>& deps);
  void sync();
};

class TaskState {
 public:
  TaskState(const TaskID id,
            IRunnable* runnable,
            const int num_total_tasks,
            const std::vector<TaskID>& deps={})
    : id_(id), state_lock_(), curr_task_id_(0),
      num_total_tasks_(num_total_tasks), num_total_done_(0),
      runnable_(runnable), max_dep_id_(-1) {
    if (!deps.empty()) {
      max_dep_id_ = *std::max_element(deps.begin(), deps.end());
    }
  }

  const TaskID id_;
  std::mutex state_lock_;
  int curr_task_id_;
  int num_total_tasks_;
  std::atomic<int> num_total_done_;
  IRunnable* runnable_;
  TaskID max_dep_id_;
};

struct TaskStateComp {
  bool operator()(const TaskState* lhs, const TaskState* rhs) const {
    // TODO(kykim): Should we distinguish among tasks with the same max dep id?
    return lhs->max_dep_id_ > rhs->max_dep_id_;
  }
};

/*
 * TaskSystemParallelThreadPoolSleeping: This class is the student's
 * optimized implementation of a parallel task execution engine that uses
 * a thread pool. See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSleeping : public ITaskSystem {
 public:
  TaskSystemParallelThreadPoolSleeping(int num_threads);
  ~TaskSystemParallelThreadPoolSleeping();
  const char* name();
  void run(IRunnable* runnable, int num_total_tasks);
  TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                          const std::vector<TaskID>& deps);
  void sync();

 private:
  std::atomic<TaskID> id_;
  const int num_threads_;
  bool done_;
  std::thread* threads_;
  std::mutex work_cv_lock_;
  std::condition_variable work_cv_;
  std::condition_variable sync_cv_;
  std::atomic<int> num_threads_exited_;

  std::mutex task_lock_;
  // std::deque<TaskState*> work_q_;   // Tasks pending.
  std::set<TaskState*> work_q_;
  std::deque<TaskState*> ready_q_;  // Tasks ready to run.
  TaskID max_done_id_;
};

#endif
