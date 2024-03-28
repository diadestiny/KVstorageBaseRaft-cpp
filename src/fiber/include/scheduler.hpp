#ifndef __MONSOON_SCHEDULER_H__
#define __MONSOON_SCHEDULER_H__

#include <atomic>
#include <boost/type_index.hpp>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <vector>
#include "fiber.hpp"
#include "mutex.hpp"
#include "thread.hpp"
#include "utils.hpp"

namespace monsoon {
// 调度任务（支持协程类和普通函数(仿函数类型)）
class SchedulerTask {
 public:
  // 允许Scheduler类访问SchedulerTask的私有成员
  friend class Scheduler;

  // 默认构造函数，初始化线程ID为-1（无指定线程）
  SchedulerTask() : thread_(-1) {}

  // 使用协程对象和线程ID构造任务
  SchedulerTask(Fiber::ptr f, int t) : fiber_(f), thread_(t) {}

  // 使用协程对象指针和线程ID构造任务，通过swap交换协程对象
  SchedulerTask(Fiber::ptr* f, int t) : thread_(t) {
    fiber_.swap(*f);
  }

  // 使用函数对象和线程ID构造任务
  SchedulerTask(std::function<void()> f, int t) : cb_(f), thread_(t) {}

  // 重置任务为初始状态
  void reset() {
    fiber_ = nullptr;
    cb_ = nullptr;
    thread_ = -1;
  }

 private:
  Fiber::ptr fiber_; // 封装的协程对象
  std::function<void()> cb_; // 封装的函数对象
  int thread_; // 指定执行任务的线程ID，-1表示无指定线程
};

// N->M协程调度器
class Scheduler {
 public:
  typedef std::shared_ptr<Scheduler> ptr;
  /**
   * @brief 创建调度器
   * @param[in] threads 线程数
   * @param[in] use_caller 是否将当前线程也作为调度线程
   * @param[in] name 名称
   */
  Scheduler(size_t threads = 1, bool use_caller = true, const std::string &name = "Scheduler");
  virtual ~Scheduler();
  const std::string &getName() const { return name_; }
  static Scheduler *GetThis(); // 获取当前线程调度器
  static Fiber *GetMainFiber(); // 获取当前线程的调度器协程

  /**
   * \brief 添加调度任务
   * \tparam TaskType 任务类型，可以是协程对象或函数指针
   * \param task 任务
   * \param thread 指定执行函数的线程，-1为不指定
   */
  template <class TaskType>
  void scheduler(TaskType task, int thread = -1) {
    bool isNeedTickle = false;
    {
      Mutex::Lock lock(mutex_);
      isNeedTickle = schedulerNoLock(task, thread); //指定线程thread运行task
    }

    if (isNeedTickle) {
      tickle();  // 唤醒idle协程
    }
  }
  // 启动调度器
  void start();
  // 停止调度器,等待所有任务结束
  void stop();

 protected:
  // 通知调度器任务到达
  virtual void tickle();
  /**
   * \brief  协程调度函数, 核心函数
   * 默认会启用hook
   */
  void run();
  // 无任务时执行idle协程
  virtual void idle();
  // 返回是否可以停止
  virtual bool stopping();
  // 设置当前线程调度器
  void setThis();
  // 返回是否有空闲进程
  bool isHasIdleThreads() { return idleThreadCnt_ > 0; }

 private:
  // 无锁下，添加调度任务到任务队列中，todo 可以加入使用clang的锁检查
  template <class TaskType>
  bool schedulerNoLock(TaskType t, int thread) {
    bool isNeedTickle = tasks_.empty();
    SchedulerTask task(t, thread);
    if (task.fiber_ || task.cb_) {
      // std::cout << "有效task" << std::endl;
      tasks_.push_back(task);
    }
    // std::cout << "scheduler noblock: isNeedTickle = " << isNeedTickle << std::endl;
    return isNeedTickle;
  }
  
  std::string name_; // 调度器名称
  Mutex mutex_; // 互斥锁
  
  std::vector<Thread::ptr> threadPool_; // 线程池 (关键)
  std::list<SchedulerTask> tasks_; // 任务队列 (关键)
  
  std::vector<int> threadIds_; // 线程池id数组

  size_t threadCnt_ = 0; // 工作线程数量（不包含use_caller的主线程）
  // 活跃线程数
  std::atomic<size_t> activeThreadCnt_ = {0};
  // IDLE线程数
  std::atomic<size_t> idleThreadCnt_ = {0};
  // 是否是use caller
  bool isUseCaller_;
  // use caller= true,调度器所在线程的调度协程
  Fiber::ptr rootFiber_;
  // use caller = true,调度器协程所在线程的id
  int rootThread_ = 0;
  bool isStopped_ = false;
};
}  // namespace monsoon

#endif