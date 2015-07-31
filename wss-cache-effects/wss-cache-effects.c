#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <assert.h>
#include <inttypes.h>
#include <getopt.h>
#include <signal.h>
#include <assert.h>



enum {
	MODE_LINEAR = 1,
	MODE_REVERSE,
	MODE_RANDOM,

	MODE_LINEAR_WRITE,
	MODE_REVERSE_WRITE,
	MODE_RANDOM_WRITE,
};

uint8_t *data;
size_t working_set_size;

int mode;

#define DEFAULT_NO_ITERATIONS 1000
unsigned int iterations = DEFAULT_NO_ITERATIONS;

#define DEFAULT_NO_ACCESS 1000
unsigned int access_no = DEFAULT_NO_ACCESS;

static unsigned long x = 123456789, y = 362436069, z = 521288629;

static inline unsigned long fast_rand(void)
{
	unsigned long t;
	x ^= x << 16;
	x ^= x >> 5;
	x ^= x << 1;

	t = x;
	x = y;
	y = z;
	z = t ^ x ^ y;

	return z;
}


static void die(const char *msg)
{
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

static void process_linear(void)
{
	int i, j;
	uint8_t tmp;

	assert(access_no < working_set_size);

	for (i = 0; i < iterations; i++) {
		for (j = 0; j < access_no; j++) {
			tmp = data[j];
		}
	}
}


static void process_reverse(void)
{
	int i, j;
	uint8_t tmp;

	assert(access_no < working_set_size);

	for (i = 0; i < iterations; i++) {
		for (j = access_no - 1; j >= 0; j--) {
			tmp = data[j];
		}
	}
}


static void process_random(void)
{
	int i, j;
	uint8_t tmp;

	for (i = 0; i < iterations; i++) {
		for (j = 0; j < access_no; j++) {
			tmp = data[fast_rand() % working_set_size];
		}
	}
}

static void process_linear_write(void)
{
	int i, j;
	uint8_t tmp = 23;

	assert(access_no < working_set_size);

	for (i = 0; i < iterations; i++) {
		for (j = 0; j < access_no; j++) {
			data[j] = tmp;
		}
	}
}


static void process_reverse_write(void)
{
	int i, j;
	uint8_t tmp = 23;

	assert(access_no < working_set_size);

	for (i = 0; i < iterations; i++) {
		for (j = access_no - 1; j >= 0; j--) {
			data[j] = tmp;
		}
	}
}


static void process_random_write(void)
{
	int i, j;
	uint8_t tmp = 23;

	for (i = 0; i < iterations; i++) {
		for (j = 0; j < access_no; j++) {
			data[fast_rand() % working_set_size] = tmp;
		}
	}
}

static void xgetopt(int ac, char **av)
{
	int c, ret, error;

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{"iteration",    1, 0, 'i'},
			{"mode",         1, 0, 'm'},
			{"working-size", 1, 0, 'w'},
			{"access",       1, 0, 'a'},
			{0, 0, 0, 0}
		};

		c = getopt_long(ac, av, "i:w:m:", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
			case 'a':
				access_no = atoi(optarg);
				break;
			case 'i':
				iterations = atoi(optarg);
				break;
			case 'w':
				working_set_size = atoi(optarg);
				break;
			case 'm':
				if (!strcasecmp(optarg, "linear")) {
					mode = MODE_LINEAR;
				} else if (!strcasecmp(optarg, "reverse")) {
					mode = MODE_REVERSE;
				} else if (!strcasecmp(optarg, "random")) {
					mode = MODE_RANDOM;

				} else if (!strcasecmp(optarg, "linear-write")) {
					mode = MODE_LINEAR_WRITE;
				} else if (!strcasecmp(optarg, "reverse-write")) {
					mode = MODE_REVERSE_WRITE;
				} else if (!strcasecmp(optarg, "random-write")) {
					mode = MODE_RANDOM_WRITE;
				} else {
					fprintf(stderr, "unknown mode >%s<\n", optarg);
					exit(1);
				}

				break;
			case '?':
				die("getopt");
				break;
			default:
				fprintf(stderr, "?? getopt returned character code 0%o ??\n", c);
				break;
		}
	}
}

#define TIME_GT(x,y) (x->tv_sec > y->tv_sec || (x->tv_sec == y->tv_sec && x->tv_usec > y->tv_usec))
#define TIME_LT(x,y) (x->tv_sec < y->tv_sec || (x->tv_sec == y->tv_sec && x->tv_usec < y->tv_usec))

static int subtime(struct timeval *op1, struct timeval *op2,
		struct timeval *result)
{
	int borrow = 0, sign = 0;
	struct timeval *temp_time;

	if (TIME_LT(op1, op2)) {
		temp_time = op1;
		op1  = op2;
		op2  = temp_time;
		sign = 1;
	}

	if (op1->tv_usec >= op2->tv_usec) {
		result->tv_usec = op1->tv_usec-op2->tv_usec;
	} else {
		result->tv_usec = (op1->tv_usec + 1000000) - op2->tv_usec;
		borrow = 1;
	}
	result->tv_sec = (op1->tv_sec-op2->tv_sec) - borrow;

	return sign;
}

static double tv_to_sec(struct timeval *tv)
{
	return (double)tv->tv_sec + (double)tv->tv_usec / 1000000;
}


int main(int ac, char **av)
{
	int i;
	char *trash_tmp;
	struct timeval tv_start, tv_end, tv_res;

	xgetopt(ac, av);

	/* make sure for random mode we will access all data areas */
	assert(RAND_MAX >= working_set_size);

	data = malloc(working_set_size);
	if (!data)
		die("malloc");

	/* trash cache */
#define TRASH_SIZE 10000000
	trash_tmp = malloc(TRASH_SIZE);
	for (i = 0; i < TRASH_SIZE; i++)
		trash_tmp[i] = rand();
	free(trash_tmp);


	gettimeofday(&tv_start, NULL);

	switch (mode) {
	case MODE_LINEAR:
		process_linear();
		break;
	case MODE_REVERSE:
		process_reverse();
		break;
	case MODE_RANDOM:
		process_random();
		break;
	case MODE_LINEAR_WRITE:
		process_linear_write();
		break;
	case MODE_REVERSE_WRITE:
		process_reverse_write();
		break;
	case MODE_RANDOM_WRITE:
		process_random_write();
		break;
	default: die("unknown mode");
	}

	gettimeofday(&tv_end, NULL);

	/* calculate diff */
	subtime(&tv_start, &tv_end, &tv_res);

	double usec = ((double)(tv_res.tv_sec * 1000000 + tv_res.tv_usec)) / (iterations * access_no);


	fprintf(stdout, "%.4lf nsec\n", usec * 1000.0);

	return 0;
}
