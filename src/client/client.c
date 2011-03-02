#include <alloca.h>
#include <door.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const char *DOOR_FILE = "/var/tmp/._joyent_smartlogin_new_zone";
static const int MAX_ATTEMPTS = 3;
static const int SLEEP_PERIOD = 2;

static void
usage(const char *program, boolean_t asked_for)
{
	printf("Usage: %s ZONE\n", program);
	printf("\nNotifies smartlogin agent of a new zone\n");
	exit(asked_for ? 0 : 1);
}

int
main(int argc, char **argv)
{
	door_arg_t door_args = {0};
	int attempt = 0;
	int fd = 0;

	if (argc < 2) {
		usage(argv[0], B_FALSE);
	}

	door_args.data_ptr = argv[1];
	door_args.data_size = strlen(argv[1]) + 1;

	door_args.rsize = NULL;
	door_args.rbuf = 0;

	while (attempt++ < MAX_ATTEMPTS) {
		fd = open(DOOR_FILE, O_RDWR);
		if (fd < 0) {
			perror("open of door file failed");
		}
		if (door_call(fd, &door_args) < 0) {
			perror("door_call failed");
		} else {
			break;
		}
		sleep(SLEEP_PERIOD);
	}

	return 0;
}
