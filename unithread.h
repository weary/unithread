#ifndef __SWAPCONTEXT3_H__
#define __SWAPCONTEXT3_H__

#include <ucontext.h>
#include <array>
#include <list>

//#include <valgrind/valgrind.h> // uncomment this to save 4 bytes per thread

struct launcher_t;

// common functionality for all threads. do not instantiate, use thread_t instead
typedef void (*threadstartfunc)(void);
struct thread_base_t
{
	thread_base_t(launcher_t *launcher, threadstartfunc func, bool start_runnable, int stacksize);
	~thread_base_t();

	void yield(bool remain_runnable = true);

protected:
	launcher_t *d_launcher;

private:
	ucontext_t d_context;
	char *d_stack;
#ifdef VALGRIND_STACK_REGISTER
	unsigned d_valgrind_stack_id;
#endif

	friend class launcher_t; // to call activate
	void activate(thread_base_t *oldthread);
};

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
	void add_runnable_thread(thread_base_t *t);

	bool have_inactive_threads() const { return !d_canrun.empty(); }

protected:
	// called from launcher if current thread yields or dies
	// returns nullptr if no thread is ready to run
	thread_base_t *get_runnable_thread();

private:
	std::list<thread_base_t *> d_canrun;
};

struct launcher_t : public simple_threadmanagement_t
{
	launcher_t() : d_active(nullptr) {}

	void start();

	void yield(bool remain_runnable);
	thread_base_t *active_thread() const { return d_active; }

	ucontext_t *returnpoint() { return &d_returnpoint; }

protected:
	thread_base_t *d_active; // currently executing thread
	ucontext_t d_returnpoint;
};


template<typename T>
void thread_start_point(T *t) { try { t->run(); t->died(false); } catch(...) { t->died(true); } }

template<typename T>
thread_t<T>::thread_t(launcher_t *launcher, bool start_runnable, int stacksize) :
	thread_base_t(launcher, (threadstartfunc)&thread_start_point<T>, start_runnable, stacksize)
{
	if (0) thread_start_point<T>(nullptr); // need this code to make the compiler instantiate thread_start_point
}


#endif // __SWAPCONTEXT3_H__
