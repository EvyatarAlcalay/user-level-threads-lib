#include "uthreads.h"
#include "libraries.h"
#include <iostream>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>

#define BLOCK sigprocmask (SIG_BLOCK, &maskedSet, NULL);
#define UNBLOCK sigprocmask (SIG_UNBLOCK, &maskedSet, NULL);

#define LIB_ERROR "thread library error: "
#define MAX_THREADS_ERROR "reached max thread"
#define ILLEGAL_QUANTUMS_ERROR "illegal quantum usecs, should be >= 0"
#define ILLEGAL_ENTRY_POINT_ERROR "illegal entry point, cant bu NULL"
#define NO_SUCH_ID_ERROR "no such tid"
#define ID_0_ERROR "cant block main thread"
#define SYS_ERROR "system error: bla bla\n"

sp_manager manager;

sigset_t maskedSet;

/******************************************/
/*        FUNCTIONS DECLARATIONS          */
/******************************************/

void scheduler(int sig);
void init_timer(int quantum_usecs);
address_t translate_address(address_t addr);
void check_for_sleeping();
void lib_error (const std::string& msg);

/**********************************************/
/*        H FUNCTIONS IMPLEMENTATION          */
/**********************************************/

int uthread_init(int quantum_usecs)
{
  if (quantum_usecs <= 0)
    {
      lib_error (ILLEGAL_QUANTUMS_ERROR);
      UNBLOCK
      return -1;
    }

  sp_thread thread;
  try
    {

      thread = std::make_shared<Thread>();
    }
    catch(int error)
    {
        std::cout << SYS_ERROR << std::endl;
        return -1;
    }

  thread->set_id (0);
  thread->init_quantums();

  manager = std::make_shared<Thread_manager>();

  manager->init_all_quantums ();
  manager->add_thread (thread);
  manager->set_running_thread (thread);

  init_timer (quantum_usecs);
  return 0;
}

int uthread_spawn(thread_entry_point entry_point)
{
  BLOCK
  if (entry_point == nullptr)
  {
    lib_error (ILLEGAL_ENTRY_POINT_ERROR);
      UNBLOCK
      return -1;
  }

  if(manager->get_number_of_threads() >= MAX_THREAD_NUM)
    {
      lib_error (MAX_THREADS_ERROR);
      UNBLOCK
      return -1;
    }

  auto new_thread = std::make_shared<Thread>();
  new_thread->set_id (manager->get_next_free_id());
  new_thread->init_quantums();

  auto stack = std::make_shared<stack_array>();
  new_thread->set_stack(stack);

  auto sp = (address_t) stack.get() + STACK_SIZE - sizeof(address_t);
  auto pc = (address_t) entry_point;

  manager->add_thread (new_thread);
  manager->add_to_ready (new_thread);

  sigsetjmp(new_thread->get_env(), 1);

  (new_thread->get_env()->__jmpbuf)[JB_SP] = translate_address (sp);
  (new_thread->get_env()->__jmpbuf)[JB_PC] = translate_address (pc);

  sigemptyset(&new_thread->get_env()->__saved_mask);
  UNBLOCK
  return new_thread->get_id();
}

int uthread_terminate(int tid)
{
  BLOCK
  sp_thread thread = manager->get_thread_by_id(tid);
  if (thread == nullptr)
    {
      lib_error (NO_SUCH_ID_ERROR);
      UNBLOCK
      return -1;
    }
  State state = thread->get_state();
  if (tid == 0)
    {
      manager->clear_all_threads();
      UNBLOCK
      exit (0);
    }

  if (state == READY)
    {
      manager->delete_from_ready (thread);
      manager->delete_from_all_threads (thread);
    }

  if (state == BLOCKED)
    {
      manager->delete_from_all_threads (thread);
    }

  if (state == RUNNING)
    {
      manager->delete_from_all_threads (thread);
      sp_thread next_to_run = manager->get_ready_to_run();
      manager->set_running_thread (next_to_run);
      manager->increase_all_quantums();
      UNBLOCK
      siglongjmp (next_to_run->get_env(), 1);
    }
  if (state == SLEEPING || state == BLOCKED_AND_SLEEPING)
    {
      manager->delete_from_sleeping(tid);
      manager->delete_from_all_threads (thread);
    }
  UNBLOCK
  return 0;
}

int uthread_block(int tid)
{
  BLOCK
  if (tid == 0)
  {
    lib_error (ID_0_ERROR);
      UNBLOCK
      return -1;
  }
  sp_thread thread = manager->get_thread_by_id(tid);
  if (manager->get_running_thread ()->get_id () == tid)
  {
      manager->get_running_thread ()->set_state (BLOCKED);
      UNBLOCK
      scheduler (26);
      return 0;
  }
  if (thread == nullptr)
  {
    lib_error (NO_SUCH_ID_ERROR);
      UNBLOCK
      return -1;
  }

  if (thread->get_state () == BLOCKED)
  {
    UNBLOCK
    return 0;
  }
  if (thread->get_state() == SLEEPING)
    {
      thread->set_state (BLOCKED_AND_SLEEPING);
      UNBLOCK
      return 0;
    }
  manager->delete_from_ready(thread);
  thread->set_state(BLOCKED);
  UNBLOCK
  return 0;
}

int uthread_resume(int tid)
{
  BLOCK
  sp_thread thread = manager->get_thread_by_id(tid);
  if (thread == nullptr)
  {
    lib_error (NO_SUCH_ID_ERROR);
      UNBLOCK
      return -1;
  }
  State curr_state = thread->get_state();
  if (curr_state == RUNNING || curr_state == READY || curr_state == SLEEPING)
  {
    UNBLOCK
    return 0;
  }
  if (curr_state == BLOCKED_AND_SLEEPING)
    {
      thread->set_state (SLEEPING);
      UNBLOCK
      return 0;
    }
  manager->add_to_ready (thread);
  UNBLOCK
  return 0;
}

int uthread_sleep(int num_quantums)
{
  BLOCK
  if (manager->get_running_thread()->get_id() == 0)
    {
      lib_error (ID_0_ERROR);
      UNBLOCK
      return -1;
    }

  sp_thread thread = manager->get_running_thread();
  manager->add_to_sleeping (thread, manager->get_all_quantums() + num_quantums);

  sp_thread next_to_run = manager->get_ready_to_run();
  manager->set_running_thread (next_to_run);
  manager->increase_all_quantums();

  int ret = sigsetjmp(thread->get_env(), 1);

  if (ret == 0)
    {
      UNBLOCK
      siglongjmp (next_to_run->get_env(), 1);
    }
  UNBLOCK
  return 0;
}

int uthread_get_tid()
{
  BLOCK
  int q =  manager->get_running_thread()->get_id();
  UNBLOCK
  return q;
}

int uthread_get_total_quantums()
{
  BLOCK
  int n = manager->get_all_quantums();
  UNBLOCK
  return n;
}

int uthread_get_quantums(int tid)
{
  BLOCK
  auto thread = manager->get_thread_by_id (tid);
  if (thread == nullptr)
    {
      lib_error (NO_SUCH_ID_ERROR);
      UNBLOCK
      return -1;
    }
  int n = thread->get_quantums();
  UNBLOCK
  return n;
}

/**********************************************/
/*                   HELPERS                  */
/**********************************************/

void scheduler(int sig)
{

//  printf("got to scheduler\n");

  check_for_sleeping();

  sp_thread prev_running = manager->get_running_thread();
  if (prev_running->get_state() != BLOCKED)
    {
      //check for bugs
      manager->add_to_ready (prev_running);
    }

  sp_thread next_to_run = manager->get_ready_to_run();
//  next_to_run->increase_quantums();
  manager->set_running_thread (next_to_run);

  manager->increase_all_quantums ();

//  printf("prev: %d\nnext: %d\n", prev_running->get_id(), next_to_run->get_id
//  ());

  int ret = sigsetjmp(prev_running->get_env(), 1);

  if (ret == 0)
  {
    siglongjmp (next_to_run->get_env(), 1);
  }
}

void init_timer(int quantum_usecs)
{
  struct sigaction sa = {0};
  struct itimerval timer;

  sa.sa_handler = &scheduler;
  if (sigaction(SIGVTALRM, &sa, NULL) < 0)
  {
    printf("sigaction error.");
  }

  timer.it_value.tv_sec = quantum_usecs / 1000000;        // first time interval, seconds part
  timer.it_value.tv_usec = quantum_usecs % 1000000;        // first time interval, microseconds part

  timer.it_interval.tv_sec = quantum_usecs / 1000000;
  timer.it_interval.tv_usec = quantum_usecs % 1000000;

  if (setitimer(ITIMER_VIRTUAL, &timer, NULL))
  {
    printf("setitimer error.");
  }
}

address_t translate_address(address_t addr)
{
  address_t ret;
  asm volatile("xor    %%fs:0x30,%0\n"
               "rol    $0x11,%0\n"
      : "=g" (ret)
      : "0" (addr));
  return ret;
}

void check_for_sleeping()
{
  auto s = manager->get_sleep();
  if (s == nullptr)
  {
      return;
  }
  while (s->first <= manager->get_all_quantums())
    {
      manager->wake_sleeping ();
      s = manager->get_sleep ();
      if (s == nullptr)
      {
          return;
      }
    }
}

void lib_error (const std::string& msg)
{
  std::cout << LIB_ERROR << msg << std::endl;
}
