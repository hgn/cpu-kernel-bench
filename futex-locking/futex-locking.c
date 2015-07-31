#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <linux/futex.h>

enum {
	MUTEX_TYPE_PTHREAD,
	MUTEX_TYPE_FUTEX_VANILLA,
	MUTEX_TYPE_FUTEX_EXT
};

static int mutex_type = MUTEX_TYPE_PTHREAD;

static pthread_mutex_t pth_mutex = PTHREAD_MUTEX_INITIALIZER;
static int x_mutex;

#define atomic_xadd(P, V) __sync_fetch_and_add((P), (V))
#define cmpxchg(P, O, N) __sync_val_compare_and_swap((P), (O), (N))
#define atomic_inc(P) __sync_add_and_fetch((P), 1)
#define atomic_dec(P) __sync_add_and_fetch((P), -1)
#define atomic_add(P, V) __sync_add_and_fetch((P), (V))
#define atomic_set_bit(P, V) __sync_or_and_fetch((P), 1<<(V))
#define atomic_clear_bit(P, V) __sync_and_and_fetch((P), ~(1<<(V)))

/* Compile read-write barrier */
#define barrier() asm volatile("": : :"memory")

/* Pause instruction to prevent excess processor bus usage */
#define cpu_relax() asm volatile("pause\n": : :"memory")

/* Atomic exchange (of various sizes) */
static inline void *xchg_64(void *ptr, void *x)
{
	__asm__ __volatile__("xchgq %0,%1":"=r"((unsigned long long)x)
			     :"m"(*(volatile long long *)ptr),
			     "0"((unsigned long long)x)
			     :"memory");

	return x;
}

static inline unsigned xchg_32(void *ptr, unsigned x)
{
	__asm__ __volatile__("xchgl %0,%1":"=r"((unsigned)x)
			     :"m"(*(volatile unsigned *)ptr), "0"(x)
			     :"memory");

	return x;
}

static inline unsigned short xchg_16(void *ptr, unsigned short x)
{
	__asm__ __volatile__("xchgw %0,%1":"=r"((unsigned short)x)
			     :"m"(*(volatile unsigned short *)ptr), "0"(x)
			     :"memory");

	return x;
}

/* Test and set a bit */
static inline char atomic_bitsetandtest(void *ptr, int x)
{
	char out;
	__asm__ __volatile__("lock; bts %2,%1\n"
			     "sbb %0,%0\n":"=r"(out),
			     "=m"(*(volatile long long *)ptr)
			     :"Ir"(x)
			     :"memory");

	return out;
}

static volatile int shared_tmp;

int sys_futex(void *addr1, int op, int val1, struct timespec *timeout,
	      void *addr2, int val3)
{
	return syscall(SYS_futex, addr1, op, val1, timeout, addr2, val3);
}

#define MUTEX_INITIALIZER {0}

void x_mutex_init(void)
{
	x_mutex = 0;
}

void x_mutex_destroy(void)
{
	return;
}

void x_mutex_lock_ext(void)
{
	int i, c;

	/* Spin and try to take lock */
	for (i = 0; i < 100; i++) {
		c = cmpxchg(&x_mutex, 0, 1);
		if (!c)
			return;

		cpu_relax();
	}

	/* The lock is now contended */
	if (c == 1)
		c = xchg_32(&x_mutex, 2);

	while (c) {
		/* Wait in the kernel */
		sys_futex(&x_mutex, FUTEX_WAIT_PRIVATE, 2, NULL, NULL, 0);
		c = xchg_32(&x_mutex, 2);
	}

	return;
}

void x_mutex_unlock_ext(void)
{
	int i;

	/* Unlock, and if not contended then exit. */
	if (x_mutex == 2) {
		x_mutex = 0;
	} else if (xchg_32(&x_mutex, 0) == 1)
		return;

	/* Spin and hope someone takes the lock */
	for (i = 0; i < 200; i++) {
		if (x_mutex) {
			/* Need to set to state 2 because there may be waiters */
			if (cmpxchg(&x_mutex, 1, 2))
				return;
		}
		cpu_relax();
	}

	/* We need to wake someone up */
	sys_futex(&x_mutex, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);

	return;
}

void x_mutex_lock(void)
{
	int c = 2;

	while (c) {
		/* Wait in the kernel */
		sys_futex(&x_mutex, FUTEX_WAIT_PRIVATE, 2, NULL, NULL, 0);
		c = xchg_32(&x_mutex, 2);
	}

	return;
}

void x_mutex_unlock(void)
{
	int i;

	/* Unlock, and if not contended then exit. */
	if (x_mutex == 2)
		x_mutex = 0;
	else if (xchg_32(&x_mutex, 0) == 1)
		return;

	/* Spin and hope someone takes the lock */
	for (i = 0; i < 1; i++) {
		if (x_mutex) {
			/* Need to set to state 2 because there may be waiters */
			if (cmpxchg(&x_mutex, 1, 2))
				return;
		}
		cpu_relax();
	}

	/* We need to wake someone up */
	sys_futex(&x_mutex, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);

	return;
}

/* ~~1 second */
static void busy_loop(double mult)
{
	int i, cum = 200000000.0 * mult;

	for (i = 0; i < cum; i++) {
		shared_tmp += i;
	}
}

void msleep(unsigned long milisec)
{
	struct timespec req;

	memset(&req, 0, sizeof(struct timespec));

	time_t sec = milisec / 1000;

	milisec = milisec - (sec * 1000);
	req.tv_sec = sec;
	req.tv_nsec = milisec * 1000000L;

	while (nanosleep(&req, &req) == -1)
		continue;
	return;
}

#define LOOK_ROUNDS 1000

static void init(void)
{
	switch (mutex_type) {
	case MUTEX_TYPE_PTHREAD:
		break;
	case MUTEX_TYPE_FUTEX_VANILLA:
		x_mutex_init();
		break;
	case MUTEX_TYPE_FUTEX_EXT:
		x_mutex_init();
		break;
	}
}

static void lock(void)
{
	switch (mutex_type) {
	case MUTEX_TYPE_PTHREAD:
		pthread_mutex_lock(&pth_mutex);
		break;
	case MUTEX_TYPE_FUTEX_VANILLA:
		x_mutex_lock();
		break;
	case MUTEX_TYPE_FUTEX_EXT:
		x_mutex_lock_ext();
		break;
	}
}

static void unlock(void)
{
	switch (mutex_type) {
	case MUTEX_TYPE_PTHREAD:
		pthread_mutex_unlock(&pth_mutex);
		break;
	case MUTEX_TYPE_FUTEX_VANILLA:
		x_mutex_unlock();
		break;
	case MUTEX_TYPE_FUTEX_EXT:
		x_mutex_unlock_ext();
		break;
	}
}

static void *locker(void *args)
{
	int i;
	int num = (int)*((int *)args);

	fprintf(stderr, "# %d alive\n", num);

	//msleep(1000); /* max 4s */

	for (i = 0; i < LOOK_ROUNDS; i++) {
		lock();
		fprintf(stderr, "locker in\n");
		busy_loop(0.1);
		unlock();
		busy_loop(0.1);
		sched_yield();
	}

	free(args);

	return NULL;
}

static void *unlocker(void *args)
{
	int i;
	int num = (int)*((int *)args);

	fprintf(stderr, "# %d alive\n", num);

	for (i = 0; i < LOOK_ROUNDS; i++) {
		lock();
		fprintf(stderr, "unlocker in\n");
		busy_loop(0.1);
		unlock();
		busy_loop(0.1);
		sched_yield();
	}

	free(args);

	return NULL;
}

int main(int ac, char **av)
{
	int ret = 0, *num;
	pthread_t thread_id[2];

	init();

	num = malloc(sizeof(int));
	if (!num) {
		perror("malloc");
		exit(1);
	}
	*num = 0;
	ret = pthread_create(&thread_id[0], NULL, locker, num);
	if (ret) {
		fprintf(stderr, "pthread_create failed: %s\n", strerror(ret));
		return ret;
	}

	num = malloc(sizeof(int));
	if (!num) {
		perror("malloc");
		exit(1);
	}
	*num = 1;
	ret = pthread_create(&thread_id[1], NULL, unlocker, num);
	if (ret) {
		fprintf(stderr, "pthread_create failed: %s\n", strerror(ret));
		return ret;
	}

	fputs("# + -> thread startup; - -> thread shutdown\n", stderr);

	pthread_join(thread_id[0], NULL);
	pthread_join(thread_id[1], NULL);

	fprintf(stderr, "# exiting\n");

	return EXIT_SUCCESS;
}
