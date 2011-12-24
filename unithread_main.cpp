#include "unithread.h"
#include <stdio.h>
#include <assert.h>
#include <stdint.h>


struct realthread_t : public unithread::thread_t<realthread_t>
{
	realthread_t(unithread::launcher_t *launcher, int n, unithread::condition_t *cond) :
		thread_t<realthread_t>(launcher),
		d_n(n), d_alive(true), d_cond(cond)
	{
		printf("thread %d created\n", d_n);
	}

	void run()
	{
		printf("running thread %d\n", d_n);
		realthread_t *t = nullptr;
		if ((d_n & 3) == 0)
		{
			printf("thread %d creating subthread\n", d_n);
			t = new realthread_t(launcher(), d_n+1, NULL);
			while (t->alive()) // busy-loop until other thread is gone
			{
				printf("thread %d's subthread %d is still alive, yielding\n", d_n, d_n+1);
				yield();
			}
		}
		else if ((d_n & 3) < 3)
		{
			printf("thread %d creating subthread with condition\n", d_n);
			unithread::condition_t cond(launcher());
			t = new realthread_t(launcher(), d_n+1, &cond);
			yield(cond);
		}
		else // d_n & 3 == 3
		{}
		printf("after yield back in thread %d\n", d_n);
		if (t) { delete t; t = nullptr; }
	}

	bool alive() const { return d_alive; }

	void died(bool from_exception)
	{
		d_alive = false;
		printf("thread %d died (from_exception=%d)\n", d_n, from_exception);
		if (d_cond) d_cond->set();
	}

protected:
	int d_n;
	bool d_alive;
	unithread::condition_t *d_cond;
};


int main(int argc, char *argv[])
{
	unithread::launcher_t launcher;

	realthread_t t1(&launcher, 0, NULL);
	realthread_t t2(&launcher, 4, NULL);
	realthread_t t3(&launcher, 8, NULL);
	realthread_t t4(&launcher, 12, NULL);
	launcher.start();

	printf("back in main loop\n");


	return 0;
}
