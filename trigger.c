#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <sys/inotify.h>
#include <fnmatch.h>
#define NDEBUG
#include "dbg.h"

static int done;
static const char * command;
static void sigint_handler(int signum)
{
	done = 1;
}

static uint32_t watch_mask = IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MODIFY;
static void add_watch(int fd, const char *pathname)
{
	int wd;
	wd = inotify_add_watch(fd, pathname, watch_mask);
	check(wd >= 0, "Unable to add watch for '%s'", pathname);
	return;
error:
	exit(1);
}

static int open_inotify_fd()
{
	int fd;
	fd = inotify_init();
	check(fd >= 0, "Unable to initialize inotify instance!");
	return fd;
error:
	exit(1);
}


static int should_trigger(struct inotify_event *event)
{
	if ( event->len ) {
		if (!fnmatch("*.[ch]", event->name, FNM_PATHNAME)) {
			return 1;
		}
		else {
			debug("Ignoring %s in accordance with pattern", event->name);
		}
	}
	else {
		debug("Ignoring event with no name length");
	}
	return 0;

}
static void process_events(int fd)
{
	struct inotify_event *event;
	int ret, len, bufsize = sizeof(event) + NAME_MAX + 1;
	char buffer[bufsize];
	done = 0;
	signal(SIGINT, sigint_handler);
	while (!done) {
		signal(SIGINT, SIG_DFL);
		len = read(fd, buffer, bufsize);
		signal(SIGINT, sigint_handler);
		check(len >= 0, "Invalid read from inotify instance");
		event = (struct inotify_event * ) buffer;
		if (should_trigger(event)) {
			printf ("Trigger (%s): Executing command '%s'\n", event->name, command);
			ret = system(command);
			if (WIFSIGNALED(ret) &&
					(WTERMSIG(ret) == SIGINT || WTERMSIG(ret) == SIGQUIT)) {
				printf("Trigger (%s): Interrupted by signal %d", event->name, WTERMSIG(ret));
				break;
			}
			check((-1 != ret), "Failed to execute command");
			printf ("Trigger (%s): done.\n", event->name);

		}
	}
error:
	log_info("Terminating ...");
}

void print_usage()
{
	printf("Usage: blah blah\n");
}
int main (int argc, char **argv)
{
	int inotify_fd, wd;
	char *path;
	path = getcwd(NULL, 0);
	check(path, "Failed to get current working directory");
	check(argc == 2, "Wrong number of arguments");
	check_mem(command = strdup(argv[1]));
	inotify_fd = open_inotify_fd();
	log_info("Inotify instance initialized ...");
	add_watch(inotify_fd, path);
	log_info("Watch added for '%s' ...", path);
	process_events(inotify_fd);
	log_info("Stopped watching '%s' ...", path);
	return 0;
error:
	print_usage();
	return 1;
}
