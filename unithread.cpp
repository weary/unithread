
#include "unithread.h"
#include <algorithm>
#include <stdexcept>
#include <string.h>
#include <assert.h>

void unix_die(const std::string &during)
{
	int e = errno;
	throw std::runtime_error("exception during " + during + ", " + strerror(e));
}

thread_base_t::thread_base_t(
		launcher_t *launcher, threadstartfunc func,
		bool start_runnable, int stacksize) :
	d_launcher(launcher), d_stack(new char[stacksize])
{
#ifdef VALGRIND_STACK_REGISTER
	d_valgrind_stack_id = VALGRIND_STACK_REGISTER(d_stack, d_stack+stacksize);
#endif

	if (getcontext(&d_context) != 0)
		unix_die("getting context for new thread");
	d_context.uc_stack.ss_sp = d_stack;
	d_context.uc_stack.ss_size = stacksize;
	d_context.uc_link = launcher->returnpoint();

	makecontext(&d_context, func, 1, this);

	if (start_runnable)
		d_launcher->add_runnable_thread(this);
}

thread_base_t::~thread_base_t()
{
#ifdef VALGRIND_STACK_REGISTER
	VALGRIND_STACK_DEREGISTER(d_valgrind_stack_id);
#endif

	delete []d_stack;
	d_stack = nullptr;
}

void thread_base_t::yield(bool remain_runnable)
{
	d_launcher->yield(remain_runnable);
}

void thread_base_t::activate(thread_base_t *oldthread)
{
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

void simple_threadmanagement_t::add_runnable_thread(thread_base_t *t)
{
	d_canrun.push_back(t);
}

thread_base_t *simple_threadmanagement_t::get_runnable_thread()
{
	if (d_canrun.empty())
		return nullptr;
	thread_base_t *next = d_canrun.front();
	d_canrun.pop_front();
	return next;
}

void launcher_t::yield(bool remain_runnable)
{
	thread_base_t *next = get_runnable_thread();
	if (!next) // we are the only thread -> don't yield
		return;

	assert(d_active);
	thread_base_t *old = d_active;
	if (remain_runnable)
		add_runnable_thread(old);

	d_active = next;
	d_active->activate(old); // old will be used to store current state
}

void launcher_t::start()
{
	bool returnpoint_initialised = false;
	if (getcontext(&d_returnpoint) != 0)
		unix_die("getting returnpoint context");
	if (returnpoint_initialised)
		d_active = nullptr;
	else
		returnpoint_initialised = true;

	// either we are here for the first time, starting the first thread, or a thread just died

	d_active = get_runnable_thread();
	if (d_active)
		d_active->activate(nullptr);
}

