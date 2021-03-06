#include <libgen.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <sys/inotify.h>
#include <fnmatch.h>
#include "dbg.h"
#include <assert.h>

#undef NDEBUG
#define progress(EventName, M, ...) \
	fprintf(stdout, "%sTrigger (%s)%s: " M "\n", CLR_GREEN, EventName, CLR_RESET, ##__VA_ARGS__)

static int done;
static char * command;
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


static int should_trigger(struct inotify_event *event, const char *pattern)
{
	if ( event->len ) {
		if ( !pattern || !fnmatch(pattern, event->name, FNM_PATHNAME)) {
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
	/* FIXME: Handle events with multiple event types */
	if( IN_ACCESS & event_mask ) return "IN_ACCESS";
	else if( IN_ATTRIB & event_mask ) return "IN_ATTRIB";
	else if( IN_CLOSE_WRITE & event_mask ) return "IN_CLOSE_WRITE";
	else if( IN_CLOSE_NOWRITE & event_mask ) return "IN_CLOSE_NOWRITE";
	else if( IN_CREATE & event_mask ) return "IN_CREATE";
	else if( IN_DELETE & event_mask ) return "IN_DELETE";
	else if( IN_DELETE_SELF & event_mask ) return "IN_DELETE_SELF";
	else if( IN_MODIFY & event_mask) return "IN_MODIFY";
	else if( IN_MOVE_SELF & event_mask ) return "IN_MOVE_SELF";
	else if( IN_MOVED_FROM & event_mask ) return "IN_MOVED_FROM";
	else if( IN_MOVED_TO & event_mask ) return "IN_MOVED_TO";
	else if( IN_OPEN & event_mask ) return "IN_OPEN";
	else {
		log_err("Unknown event in event_mask %d", event_mask);
		assert(!"Unknown event in event_mask");
	}
}

static void process_events(int fd, const char * pattern)
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
		if (should_trigger(event, pattern)) {
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

/*
 * Determine the path for which to add a watch. If the pattern looks like
 * a path, it probably is, in which case we use the dirname() of the pattern
 * as the path and modify pattern accordingly with basename(). If it does not
 * look like a path or if we don't have a pattern, we use the current working
 * directory.
 */
static char * determine_watch_path(char **pattern)
{
	char *p = NULL;
	char *dirc = NULL;
	char *basec= NULL;
	char *dname= NULL;
	char *bname = NULL;
	if (pattern != NULL && *pattern != NULL) {
		check_mem(dirc= strdup(*pattern));
		check_mem(basec= strdup(*pattern));
		bname = basename(dirc);
		dname = dirname(basec);
		if (0 != strcmp(dname, ".")) {
			check_mem(p = strdup(dname));
			free(*pattern);
			check_mem(*pattern = strdup(bname));
		}
		free(dirc);
		free(basec);
	}
	if (p == NULL) {
		if ((p = getcwd(p, 0)) == NULL ) {
			log_err("Failed to get current working directory: %s", strerror(errno));
			return NULL;
		}
	}
	return p;
error:
	return NULL;
}

static void print_usage()
{
	printf("Usage: in-trigger command [pattern]\n");
}

int main (int argc, char **argv)
{
	int inotify_fd, wd;
	char *path = NULL;
	char *pattern = NULL;

	check((argc > 1 && argc < 4), "Wrong number of arguments");
	if (strcmp(argv[1], "-h") == 0) {
		print_usage();
		exit(0);
	}
	check_mem(command = strdup(argv[1]));
	if (argc == 3)
		check_mem(pattern = strdup(argv[2]));

	path = determine_watch_path(&pattern);
	check(path, "Failed to determine directory to watch");
	inotify_fd = open_inotify_fd();
	log_info("Inotify instance initialized ...");
	add_watch(inotify_fd, path);
	log_info("Watch pattern '%s' added for '%s' ...", pattern, path);
	process_events(inotify_fd, (const char *) pattern);
	log_info("Stopped watching '%s' ...", path);
	free(command);
	free(path);
	free(pattern);
	return 0;
error:
	print_usage();
	return 1;
}
