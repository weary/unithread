#ifndef __SWAPCONTEXT3_H__
#define __SWAPCONTEXT3_H__

#include <ucontext.h>
#include <array>
#include <list>

//#include <valgrind/valgrind.h> // uncomment this if you want to use valgrind


/** basic usage:
	derive a class from thread_t and implement 'run' function. optionally
	implement 'pre_activate' and 'died' as needed
	create a launcher_t
	create an instance of your thread
	call launcher_t::run
*/

namespace unithread
{

struct launcher_t;
struct condition_t;
typedef unsigned long stacksize_t;

// thread base class, contains common functionality for all threads.
// override at least 'run', and consider 'died' and 'preyield' if you need them
struct thread_t
{
	thread_t(launcher_t *launcher, bool start_runnable = true, stacksize_t stacksize = 0);
	virtual ~thread_t();

	// main function of this thread. implement in derived
	virtual void run() = 0;

	// function called after 'run' finished (called from random other context)
	virtual void died() {} // empty default implementation

	// called before this thread regains control. can be used to prepare state
	virtual void pre_activate() {}

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
	void activate(thread_t *oldthread);
	void set_scheduled() { d_scheduled = true; }
};


// manages all (EXCEPT currently running) threads
struct simple_threadmanagement_t
{
 	// called when a thread is created (with start_runnable=true) or a thread yields
	// call explicitly after you used yield(remain_runnable=false)
	// calling this function multiple times is valid. don't call it with the active thread
	void add_runnable_thread(thread_t *t);

	bool have_inactive_threads() const { return !d_canrun.empty(); }

protected:
	// called from launcher if current thread yields or dies
	// returns nullptr if no thread is ready to run
	thread_t *pop_runnable_thread();

private:
	std::list<thread_t *> d_canrun;
};


// the thread management
struct launcher_t : public simple_threadmanagement_t
{
	launcher_t(stacksize_t default_stacksize = 64*1024) :
		d_active(nullptr), d_default_stacksize(default_stacksize) {}

	void start();

	void yield(); // thread never remains running, add to queue before calling
	thread_t *active_thread() const { return d_active; }

	ucontext_t *returnpoint() { return &d_returnpoint; }

	stacksize_t default_stacksize() const { return d_default_stacksize; }
	void set_default_stacksize(stacksize_t newval) { d_default_stacksize = newval; }

protected:
	friend class thread_t;
	friend class condition_t;
	void add_runnable_thread(thread_t *t);

	thread_t *d_active; // currently executing thread
	ucontext_t d_returnpoint;
	stacksize_t d_default_stacksize;
};


// if a thread is blocked until a certain situation changes, create
// a condition and yield on it. execution will resume after someone calls set()
// multiple thread's can wait on the same condition
struct condition_t
{
	void set(launcher_t *launcher); // the waiting threads can continue now
	void clear();

	void add_thread(thread_t *t);
	void del_thread(thread_t *t);
	bool empty() const { return d_threads.empty(); }
protected:
	std::list<thread_t *> d_threads;
};

// make sure only one thread can execute while this object exists
struct critical_section_guard_t
{
	critical_section_guard_t(launcher_t *launcher, condition_t &cond);
	~critical_section_guard_t();

	void enter();
	void exit();

	// don't copy
	critical_section_guard_t & operator=(const critical_section_guard_t&) = delete;
	critical_section_guard_t(const critical_section_guard_t&) = delete;

protected:
	bool d_active; // true if we are between 'enter' and 'exit' calls
	launcher_t *d_launcher;
	condition_t *d_condition;
};

} // namespace unithread

#endif // __SWAPCONTEXT3_H__
