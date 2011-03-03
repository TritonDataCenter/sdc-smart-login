#include <alloca.h>
#include <door.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/rsa.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_OOM(SZ)	fprintf(stderr, "Unable to alloca %d bytes\n", SZ)

static const char *DOOR = "/var/tmp/._joyent_sshd_key_is_authorized";
static const char *REQ_FMT_STR = "%s %d %s"; /* name uid fp */
static const int RETURN_SZ = 2;

static const int MAX_ATTEMPTS = 2;
static const int SLEEP_PERIOD = 1;

int
sshd_user_rsa_key_allowed(struct passwd *pw, RSA *key, const char *fp)
{
	int allowed = 0;
	int fd = -1;
	int blen = 0;
	int attempts = 0;
	char *buf = NULL;
	door_arg_t door_args = {0};

	if (pw == NULL || key == NULL)
		return (0);

	blen = snprintf(NULL, 0, REQ_FMT_STR, pw->pw_name, pw->pw_uid, fp) + 1;

	buf = (char *)alloca(blen);
	if (buf == NULL) {
		LOG_OOM(blen);
		return (0);
	}

	snprintf(buf, blen, REQ_FMT_STR, pw->pw_name, pw->pw_uid, fp);
	door_args.data_ptr = buf;
	door_args.data_size = blen;

	door_args.rsize = RETURN_SZ;
	door_args.rbuf = alloca(RETURN_SZ);
	if (door_args.rbuf == NULL) {
		fprintf(stderr, "Unable to alloca %d bytes\n", RETURN_SZ);
		return (0);
	}
	memset(door_args.rbuf, 0, RETURN_SZ);

	do {
		fd = open(DOOR, O_RDWR);
		if (fd < 0) {
			perror("smartplugn: open (of door FD) failed");
		}
		if (door_call(fd, &door_args) < 0) {
			perror("smartplugin: door_call failed");
		} else {
			allowed = atoi(door_args.rbuf);
			munmap(door_args.rbuf, door_args.rsize);
			break;
		}
		if (++attempts < MAX_ATTEMPTS)
			sleep(SLEEP_PERIOD);

	} while (attempts < MAX_ATTEMPTS);

	return (allowed);
}

#ifdef __cplusplus
}
#endif
