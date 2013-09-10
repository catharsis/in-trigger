#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <sys/inotify.h>
#include <fnmatch.h>
#include "dbg.h"
#include <assert.h>

#define NDEBUG
#define progress(EventName, M, ...) \
	fprintf(stdout, "%sTrigger (%s)%s: " M "\n", CLR_GREEN, EventName, CLR_RESET, ##__VA_ARGS__)

static int done;
static const char * command;
static void sigint_handler(int signum)
{
	done = 1;
}

#define WATCH_MASK IN_MODIFY
static uint32_t watch_mask = WATCH_MASK;
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

static const char * mask2str(int event_mask)
{
	if( IN_ACCESS & event_mask ) return "IN_ACCESS";
	else if( IN_ATTRIB & event_mask ) return "IN_ATTRIB";
	else if( IN_CLOSE_WRITE & event_mask ) return "IN_CLOSE_WRITE";
	else if( IN_CLOSE_NOWRITE & event_mask ) return "IN_CLOSE_NOWRITE";
	else if( IN_CREATE & event_mask ) return "IN_CREATE";
	else if( IN_DELETE & event_mask ) return "IN_DELETE";
	else if( IN_DELETE_SELF & event_mask ) return "IN_DELETE_SELF";
	else if ( IN_MODIFY & event_mask) return "IN_MODIFY";
	else if( IN_MOVE_SELF & event_mask ) return "IN_MOVE_SELF";
	else if( IN_MOVED_FROM & event_mask ) return "IN_MOVED_FROM";
	else if( IN_MOVED_TO & event_mask ) return "IN_MOVED_TO";
	else if( IN_OPEN & event_mask ) return "IN_OPEN";
	else {
		log_err("Unknown event in event_mask %d", event_mask);
		assert(!"Unknown event in event_mask");
	}
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
			debug("Triggering on event %s", mask2str(event->mask));
			progress(event->name, "Executing command '%s'", command);
			ret = system(command);
			if (WIFSIGNALED(ret) &&
					(WTERMSIG(ret) == SIGINT || WTERMSIG(ret) == SIGQUIT)) {
				progress(event->name, "Interrupted by signal %d", WTERMSIG(ret));
				break;
			}
			check((-1 != ret), "Failed to execute command");
			progress (event->name, "done.");

		}
	}
error:
	log_info("Terminating ...");
}

void print_usage()
{
	printf("Usage: in-trigger command\n");
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
