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



enum {
	MODE_LINEAR = 1,
	MODE_REVERSE,
	MODE_RANDOM,
};

uint32_t *data;
size_t data_len;

int mode;

#define DEFAULT_NO_ITERATIONS 1000
unsigned int iterations = DEFAULT_NO_ITERATIONS;


static void die(const char *msg)
{
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

static void process_linear(void)
{
	int i, j;
	uint32_t tmp;

	for (i = 0; i < iterations; i++) {
		for (j = 0; j < data_len; j++) {
			data[j] = tmp;
		}
	}
}


static void process_reverse(void)
{
	int i, j;
	uint32_t tmp;

	for (i = 0; i < iterations; i++) {
		for (j = data_len - 1; j >= 0; j--) {
			data[j] = tmp;
		}
	}
}


static void process_random(void)
{
	int i, j;
	uint32_t tmp;

	for (i = 0; i < iterations; i++) {
		for (j = 0; j < data_len; j++) {
			data[rand() % data_len] = tmp;
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
			{0, 0, 0, 0}
		};

		c = getopt_long(ac, av, "i:w:m:", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
			case 'i':
				iterations = atoi(optarg);
				break;
			case 'w':
				data_len = atoi(optarg);
				break;
			case 'm':
				if (!strcasecmp(optarg, "linear")) {
					mode = MODE_LINEAR;
				} else if (!strcasecmp(optarg, "reverse")) {
					mode = MODE_REVERSE;
				} else if (!strcasecmp(optarg, "random")) {
					mode = MODE_RANDOM;
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

	data = malloc(data_len * sizeof(uint32_t));
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
	default: die("unknown mode");
	}

	gettimeofday(&tv_end, NULL);

	/* calculate diff */
	subtime(&tv_start, &tv_end, &tv_res);

	double usec = ((double)(tv_res.tv_sec * 1000000 + tv_res.tv_usec)) / (iterations * data_len);


	fprintf(stdout, "%.4lf nsec\n", usec * 1000.0);

	return 0;
}