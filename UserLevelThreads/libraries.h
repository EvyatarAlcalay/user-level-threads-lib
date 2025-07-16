#ifndef LIBRARIES_H
#define LIBRARIES_H

#include <queue>
#include <vector>
#include <unordered_map>
#include <setjmp.h>
#include <memory>
#include <array>
#include <iostream>

#define STACK_SIZE 4096

class Thread;
class Thread_manager;

typedef enum State
{
    RUNNING,
    READY,
    BLOCKED,
    SLEEPING,
    BLOCKED_AND_SLEEPING
}State;

typedef std::shared_ptr<Thread> sp_thread;
typedef std::array<char, STACK_SIZE> stack_array;
typedef std::shared_ptr<stack_array> sp_stack;
typedef std::shared_ptr<Thread_manager> sp_manager;
typedef std::pair<size_t, sp_thread> sleeping_thread;
typedef std::shared_ptr<std::pair<size_t, sp_thread>> sp_sleeping_thread;
typedef unsigned long address_t;

#define JB_SP 6
#define JB_PC 7


class Thread
{
 private:
  int id;
  State state;
  sigjmp_buf env;
  size_t quantums;
  sp_stack stack;


 public:
  void set_id(int new_id)
  {
    this->id = new_id;
  }

  void set_stack(sp_stack new_stack)
  {
    this->stack = new_stack;
  }

  void init_quantums()
  {
    this->quantums = 0;
  }

  void increase_quantums()
  {
    this->quantums++;
  }

  void set_state(State new_state)
  {
    this->state = new_state;
  }

  int get_id()
  {
    return this->id;
  }

  State get_state()
  {
    return this->state;
  }

  sigjmp_buf &get_env()
  {
    return this->env;
  }

  int get_quantums()
  {
    return this->quantums;
  }
};

bool cmp(const sp_sleeping_thread& first, const sp_sleeping_thread& second)
{
  return first->first > second->first;
}

bool cmp_rv(const sp_sleeping_thread& first, const sp_sleeping_thread& second)
{
    return first->first < second->first;
}

class Thread_manager
{
 private:
  std::queue<sp_thread> ready_queue;
  std::unordered_map<int, sp_thread> all_threads;
  std::vector<sp_sleeping_thread> sleeping_threads;

  sp_thread running_thread;
  size_t all_quantums;


 public:
  void set_running_thread(const sp_thread& new_running_thread)
  {
    new_running_thread->set_state(RUNNING);
    new_running_thread->increase_quantums();
    this->running_thread = new_running_thread;
  }

  void init_all_quantums()
  {
    this->all_quantums = 1;
  }

  void increase_all_quantums()
  {
    this->all_quantums++;
  }

  sp_thread get_running_thread()
  {
    return this->running_thread;
  }

  void add_thread(sp_thread thread)
  {
    this->all_threads.insert ({thread->get_id(), thread});
  }

  void add_to_ready(const sp_thread& thread_ptr)
  {
    thread_ptr->set_state (READY);
    this->ready_queue.push (thread_ptr);
  }

  sp_thread get_ready_to_run()
  {
    sp_thread thread = this->ready_queue.front();
    this->ready_queue.pop();
    return thread;
  }

  int get_next_free_id()
  {
    for (int i = 1; i < 100; i++)
      {
        if (this->all_threads.find(i) == this->all_threads.end())
          {
            return i;
          }
      }
    return 0;
  }

  sp_thread get_thread_by_id(int id)
  {
    auto ret = this->all_threads.find (id);
    if (ret == this->all_threads.end())
      {
        return nullptr;
      }
    return ret->second;
  }

  size_t get_number_of_threads()
  {
    return this->all_threads.size();
  }

  void delete_from_ready(sp_thread thread_to_delete)
  {
    std::queue<sp_thread> tmp_queue;
    std::queue<sp_thread>& q = this->ready_queue;
    size_t s = q.size();
    size_t cnt = 0;

    while (q.front().get() != thread_to_delete.get())
      {
        tmp_queue.push (this->ready_queue.front());
        q.pop();
        cnt++;
      }

    q.pop();

    while (!tmp_queue.empty())
      {
        q.push(tmp_queue.front());
        tmp_queue.pop();
      }
    size_t k = s - cnt - 1;
    while(k--)
      {
        sp_thread p = q.front();
        q.pop();
        q.push(p);
      }
  }

  void delete_from_all_threads (sp_thread thread_to_delete)
  {
    auto del = this->all_threads.find (thread_to_delete->get_id());
    if (del == this->all_threads.end())
      {
        return;
      }
    this->all_threads.erase (del);
  }

  void clear_all_threads()
  {
    while (!this->ready_queue.empty ())
      {
        this->ready_queue.pop ();
      }
    this->sleeping_threads.clear();
    this->all_threads.clear ();
    this->running_thread = nullptr;
  }

  size_t get_all_quantums() const
  {
    return this->all_quantums;
  }

  void add_to_sleeping(const sp_thread& thread, size_t wakeup_time)
  {
    thread->set_state (SLEEPING);
    sp_sleeping_thread s = std::make_shared<sleeping_thread>();
    *s = std::make_pair(wakeup_time, thread);
    auto& v = this->sleeping_threads;
    v.push_back (s);
    std::push_heap (v.begin(), v.end(), cmp);
  }

  sp_sleeping_thread get_sleep()
  {
    auto& v = this->sleeping_threads;
    if (v.empty())
    {
        return nullptr;
    }
    std::pop_heap (v.begin(), v.end(), cmp);
    auto s = v.back();
    std::sort_heap (v.begin(), v.end(), cmp_rv);
    return s;
  }

  void wake_sleeping()
  {
    auto& v = this->sleeping_threads;
    std::pop_heap (v.begin(), v.end(), cmp);
    auto s = v.back();
    v.pop_back();
    if (s->second->get_state() == BLOCKED_AND_SLEEPING)
      {
        s->second->set_state(BLOCKED);
        return;
      }
    this->add_to_ready (s->second);
  }

  void delete_from_sleeping (int id)
  {
    auto& v = this->sleeping_threads;
    for (auto elem = v.begin(); elem!=v.end(); elem++)
      {
        if ((*elem)->second->get_id() == id)
          {
              v.erase (elem);
              std::sort_heap (v.begin(), v.end(), cmp_rv);
              return;
          }
      }

  }

};

#endif //LIBRARIES_H