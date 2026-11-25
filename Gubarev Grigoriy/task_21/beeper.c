#define _POSIX_C_SOURCE 200809L
#ifdef __sun
#define __EXTENSIONS__ 1 /* ECHOCTL in <termios.h> */
#endif
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static volatile sig_atomic_t beep_count = 0;
static volatile sig_atomic_t quit_requested = 0;

static struct termios saved_termios;
static int term_modified = 0;

static void restore_terminal(void)
{
    if (term_modified) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
        term_modified = 0;
    }
}

/* Disable ECHOCTL so Ctrl+C / Ctrl+\ do not print ^C / ^\ on screen. */
static void disable_echoctl(void)
{
#ifdef ECHOCTL
    struct termios t;

    if (!isatty(STDIN_FILENO))
        return;
    if (tcgetattr(STDIN_FILENO, &saved_termios) == -1)
        return;
    t = saved_termios;
    t.c_lflag &= ~ECHOCTL;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &t) == 0) {
        term_modified = 1;
        atexit(restore_terminal);
    }
#endif
}

static void sigint_handler(int sig)
{
    (void)sig;
    /* write() is async-signal-safe; '\a' (ASCII 7) rings the terminal bell. */
    if (write(STDOUT_FILENO, "\a", 1) == -1) {
        /* ignore write error inside signal handler */
    }
    ++beep_count;
}

static void sigquit_handler(int sig)
{
    (void)sig;
    quit_requested = 1;
}

int main(void)
{
    struct sigaction sa_int;
    struct sigaction sa_quit;

    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        perror("sigaction(SIGINT)");
        return EXIT_FAILURE;
    }

    memset(&sa_quit, 0, sizeof(sa_quit));
    sa_quit.sa_handler = sigquit_handler;
    sigemptyset(&sa_quit.sa_mask);
    sa_quit.sa_flags = 0; /* let pause() return EINTR on SIGQUIT */
    if (sigaction(SIGQUIT, &sa_quit, NULL) == -1) {
        perror("sigaction(SIGQUIT)");
        return EXIT_FAILURE;
    }

    disable_echoctl();

    printf("Beeper started!\n");
    printf("Press Ctrl+C (SIGINT) to ring the bell.\n");
    printf("Press Ctrl+\\ (SIGQUIT) to print the count and exit.\n");
    fflush(stdout);

    while (!quit_requested)
        pause();

    printf("\nTotal beeps: %d\n", (int)beep_count);
    printf("Exited on SIGQUIT.\n");
    return EXIT_SUCCESS;
}
