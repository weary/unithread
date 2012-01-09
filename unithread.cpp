
#include "unithread.h"
#include <algorithm>
#include <stdexcept>
#include <string.h>
#include <assert.h>

using namespace unithread;

typedef void (*threadstartfunc)(void);

static void unix_die(const std::string &during)
{
	int e = errno;
	throw std::runtime_error("exception during " + during + ", " + strerror(e));
}

void thread_start_point(thread_t *t)
{
	t->run();
}

thread_t::thread_t(
		launcher_t *launcher,
		bool start_runnable,
		stacksize_t stacksize) :
	d_launcher(launcher),
	d_scheduled(false),
	d_stack(nullptr)
{
	if (!stacksize)
		stacksize = d_launcher->default_stacksize();
	d_stack = new char[stacksize];
#ifdef VALGRIND_STACK_REGISTER
	d_valgrind_stack_id = VALGRIND_STACK_REGISTER(d_stack, d_stack+stacksize);
#endif
	assert(d_launcher);

	if (getcontext(&d_context) != 0)
		unix_die("getting context for new thread");
	d_context.uc_stack.ss_sp = d_stack;
	d_context.uc_stack.ss_size = stacksize;
	d_context.uc_link = launcher->returnpoint();

	makecontext(&d_context, (threadstartfunc)&thread_start_point, 1, this);

	if (start_runnable)
		d_launcher->add_runnable_thread(this);
}

thread_t::~thread_t()
{
#ifdef VALGRIND_STACK_REGISTER
	VALGRIND_STACK_DEREGISTER(d_valgrind_stack_id);
#endif

	delete []d_stack;
	d_stack = nullptr;
}

void thread_t::yield(bool remain_runnable)
{
	if (d_launcher->active_thread() != this)
		throw std::runtime_error("called yield on non-active thread");

	d_scheduled = false;
	if (remain_runnable)
		d_launcher->add_runnable_thread(this);

	d_launcher->yield();
}

void thread_t::yield(condition_t &cond)
{
	if (d_launcher->active_thread() != this)
		throw std::runtime_error("called conditional yield on non-active thread");

	cond.add_thread(this);

	this->yield(false);
}

void thread_t::activate(thread_t *oldthread)
{
	assert(d_scheduled);
	assert(oldthread != this);

	pre_activate(); // callback just before yield

	if (oldthread)
	{
		if (swapcontext(&oldthread->d_context, &d_context) != 0)
			unix_die("swapping context");
	}
	else
	{
		if (setcontext(&d_context) != 0)
			unix_die("setting new context");
	}
}

void simple_threadmanagement_t::add_runnable_thread(thread_t *t)
{
	assert(t->scheduled()); // call add_runnable_thread on launcher_t, not this one
	d_canrun.push_back(t);
}

thread_t *simple_threadmanagement_t::pop_runnable_thread()
{
	if (d_canrun.empty())
		return nullptr;
	thread_t *next = d_canrun.front();
	d_canrun.pop_front();
	return next;
}

void launcher_t::yield()
{
	thread_t *next = pop_runnable_thread();
	if (next == d_active) // we are the only thread -> don't yield
		return;
	if (!next)
		throw std::runtime_error("no next available thread, cannot yield!");

	assert(d_active);
	thread_t *old = d_active;

	d_active = next;
	d_active->activate(old); // old will be used to store current state
}

void launcher_t::add_runnable_thread(thread_t *t)
{
	if (t->scheduled())
		return;

	t->set_scheduled();
	simple_threadmanagement_t::add_runnable_thread(t);
}

void launcher_t::start()
{
	volatile bool returnpoint_initialised = false;
	if (getcontext(&d_returnpoint) != 0)
		unix_die("getting returnpoint context");
	if (returnpoint_initialised)
	{ // some thread just died, call 'died'
		thread_t *justdied = d_active;
		d_active = nullptr;
		justdied->died(); // called from 'random' context, take care
	}
	else
		returnpoint_initialised = true;

	// either we are here for the first time, starting the first thread, or a thread just died

	d_active = pop_runnable_thread();
	if (d_active)
		d_active->activate(nullptr);
}

void condition_t::set(launcher_t *launcher)
{
	for(thread_t *t: d_threads)
		launcher->add_runnable_thread(t);
}

void condition_t::clear()
{
	d_threads.clear();
}

void condition_t::add_thread(thread_t *t)
{
	auto iter = std::find(d_threads.cbegin(), d_threads.cend(), t);
	if (iter == d_threads.cend())
		d_threads.push_back(t);
}

void condition_t::del_thread(thread_t *t)
{
	auto iter = std::find(d_threads.begin(), d_threads.end(), t);
	if (iter != d_threads.cend())
		d_threads.erase(iter);
}

critical_section_guard_t::critical_section_guard_t(launcher_t *launcher, condition_t &cond) : // condition should be shared amongst all competing threads
	d_active(false), d_launcher(launcher), d_condition(&cond)
{
	enter();
}

critical_section_guard_t::~critical_section_guard_t()
{
	if (d_active)
		exit();
}

void critical_section_guard_t::enter()
{
	assert(!d_active);
	while (!d_condition->empty()) // are we the first
	{
		d_condition->add_thread(d_launcher->active_thread());
		d_launcher->yield();
		// if we woke, someone left the critical section (and cleared it)
	}
	d_condition->add_thread(nullptr); // add dummy to signal condition is busy
	d_active = true;
}

void critical_section_guard_t::exit()
{
	assert(d_active);
	d_condition->del_thread(nullptr); // remove dummy
	d_condition->set(d_launcher);
	d_condition->clear();
	d_active = false;
}

