#ifndef __MONSOON_FIBER_H__
#define __MONSOON_FIBER_H__

#include <stdio.h>
#include <ucontext.h>
#include <unistd.h>
#include <functional>
#include <iostream>
#include <memory>
#include "utils.hpp"

namespace monsoon {
class Fiber : public std::enable_shared_from_this<Fiber> {
 public:
  typedef std::shared_ptr<Fiber> ptr;
  
  enum State { // Fiber状态机
    READY, // 就绪态，刚创建后或者yield后状态
    RUNNING, // 运行态，resume之后的状态
    TERM, // 结束态，协程的回调函数执行完之后的状态
  };

 private:
  Fiber(); // 初始化当前线程的协程功能，构造线程主协程对象

 public:
  Fiber(std::function<void()> cb, size_t stackSz = 0, bool run_in_scheduler = true); // 构造子协程
  ~Fiber();
  void reset(std::function<void()> cb); // 重置协程状态，复用栈空间
  void resume(); // 切换协程到运行态
  void yield(); // 让出协程执行权
  
  uint64_t getId() const { return id_; } // 获取协程Id
  State getState() const { return state_; } // 获取协程状态

  
  static void SetThis(Fiber *f); // 设置当前正在运行的协程
  
  // 获取当前线程中的执行协程，如果当前线程没有创建协程，则创建第一个协程，且该协程为当前线程的主协程，其他协程通过该协程来调度
  static Fiber::ptr GetThis();

  static uint64_t TotalFiberNum();
  static void MainFunc(); // 协程回调函数
  static uint64_t GetCurFiberID();

 private:
  
  uint64_t id_ = 0; // 协程ID
  uint32_t stackSize_ = 0; // 协程栈大小
  State state_ = READY;
  ucontext_t ctx_; // 协程上下文
  void *stack_ptr = nullptr; // 协程栈地址
  std::function<void()> cb_; // 协程回调函数
  bool isRunInScheduler_; // 本协程是否参与调度器调度
};
}  // namespace monsoon

#endif