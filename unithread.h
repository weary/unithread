#ifndef __SWAPCONTEXT3_H__
#define __SWAPCONTEXT3_H__

#include <ucontext.h>
#include <array>
#include <list>

//#include <valgrind/valgrind.h> // uncomment this if you want to use valgrind


/** basic usage:
	derive a class from thread_t, implementing 'run' and 'died' function
	create a launcher_t
	create an instance of your thread
	call launcher_t::run
*/

namespace unithread
{

struct launcher_t;
struct condition_t;

// common functionality for all threads. do not instantiate, use thread_t instead
typedef void (*threadstartfunc)(void);
struct thread_base_t
{
	thread_base_t(launcher_t *launcher, threadstartfunc func, bool start_runnable, int stacksize);
	~thread_base_t();

	launcher_t *launcher() { return d_launcher; }

	void yield(bool remain_runnable = true);
	void yield(condition_t &cond); // yield until condition is set

	// returns true if the thread will, at some point, run if left unattended
	// ie, thread is waiting for time, or currently active
	bool scheduled() const { return d_scheduled; }

private:
	launcher_t *d_launcher;
	bool d_scheduled;
	ucontext_t d_context;
	char *d_stack;
#ifdef VALGRIND_STACK_REGISTER
	unsigned d_valgrind_stack_id;
#endif

	friend class launcher_t; // to call activate and set_scheduled
	void activate(thread_base_t *oldthread);
	void set_scheduled() { d_scheduled = true; }
};

// the class to derive from. CRTP, the template argument is your class (ie class myclass : public thread_t<myclass> )
template<typename T>
struct thread_t : public thread_base_t
{
	thread_t(launcher_t *launcher, bool start_runnable = true, int stacksize = 8192);
	~thread_t() {}
};


// manages all (EXCEPT currently running) threads
struct simple_threadmanagement_t
{
 	// called when a thread is created (with start_runnable=true) or a thread yields
	// call explicitly after you used yield(remain_runnable=false)
	// calling this function multiple times is valid. don't call it with the active thread
	void add_runnable_thread(thread_base_t *t);

	bool have_inactive_threads() const { return !d_canrun.empty(); }

protected:
	// called from launcher if current thread yields or dies
	// returns nullptr if no thread is ready to run
	thread_base_t *pop_runnable_thread();

private:
	std::list<thread_base_t *> d_canrun;
};


// the thread management
struct launcher_t : public simple_threadmanagement_t
{
	launcher_t() : d_active(nullptr) {}

	void start();

	void yield(); // thread never remains running, add to queue before calling
	thread_base_t *active_thread() const { return d_active; }

	ucontext_t *returnpoint() { return &d_returnpoint; }

	void add_runnable_thread(thread_base_t *t);
protected:
	thread_base_t *d_active; // currently executing thread
	ucontext_t d_returnpoint;
};


// if a thread is blocked until a certain situation changes, create
// a condition and yield on it. execution will resume after someone calls set()
// multiple thread's can wait on the same condition
struct condition_t
{
	condition_t(launcher_t *launcher);

	void set(); // the waiting threads can continue now

	void add_thread(thread_base_t *t) { d_threads.push_back(t); }
protected:
	launcher_t *d_launcher;
	std::list<thread_base_t *> d_threads;
};


template<typename T>
void thread_start_point(T *t) { try { t->run(); t->died(false); } catch(...) { t->died(true); } }

template<typename T>
thread_t<T>::thread_t(launcher_t *launcher, bool start_runnable, int stacksize) :
	thread_base_t(launcher, (threadstartfunc)&thread_start_point<T>, start_runnable, stacksize)
{
	if (0) thread_start_point<T>(nullptr); // need this code to make the compiler instantiate thread_start_point
}

} // namespace unithread

#endif // __SWAPCONTEXT3_H__
