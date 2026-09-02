#include <signal.h>
#include <string.h>

#include "defines/lbv_defines.h"
#include "signals.h"

volatile sig_atomic_t g_stop;

static void on_sigchld(int sig)
{
	(void)sig;
}

static void on_stop(int sig)
{
	(void)sig;
	g_stop = 1;
}

int start_signals(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sigemptyset(&sa.sa_mask);

	sa.sa_handler = on_sigchld;
	if (sigaction(SIGCHLD, &sa, NULL) != 0) return LBV_EXIT_ERROR;

	sa.sa_handler = on_stop;
	if (sigaction(SIGINT, &sa, NULL) != 0) return LBV_EXIT_ERROR;
	if (sigaction(SIGTERM, &sa, NULL) != 0) return LBV_EXIT_ERROR;

	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &sa, NULL) != 0) return LBV_EXIT_ERROR;

	return LBV_EXIT_OK;
}