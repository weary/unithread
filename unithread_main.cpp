#include "unithread.h"
#include <stdio.h>


struct realthread_t : public thread_t<realthread_t>
{
	realthread_t(launcher_t *launcher, int n) :
		thread_t<realthread_t>(launcher),
		d_n(n), d_alive(true)
	{
		printf("thread %d created\n", d_n);
	}

	void run()
	{
		printf("running thread %d\n", d_n);
		if ((d_n & 3) < 3)
		{
			printf("thread %d creating subthread\n", d_n);
			realthread_t *t = new realthread_t(d_launcher, d_n+1);
			while (t->alive())
			{
				printf("thread %d's subthread %d is still alive, yielding\n", d_n, d_n+1);
				yield();
			}
			delete t;
		}
		printf("after yield back in thread %d\n", d_n);
	}

	bool alive() const { return d_alive; }

	void died(bool from_exception)
	{
		d_alive = false;
		printf("thread %d died (from_exception=%d)\n", d_n, from_exception);
	}

protected:
	int d_n;
	bool d_alive;
};


int main(int argc, char *argv[])
{
	launcher_t launcher;

	realthread_t t1(&launcher, 0);
	realthread_t t2(&launcher, 4);
	realthread_t t3(&launcher, 8);
	realthread_t t4(&launcher, 12);
	launcher.start();

	printf("back in main loop\n");


	return 0;
}
