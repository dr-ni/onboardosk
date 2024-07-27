#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <execinfo.h>
#include <cxxabi.h>

#include "stacktrace.h"


static const struct {int id; const char* name;} signal_names[] =
{
    {SIGHUP, "SIGHUP"},
    {SIGINT, "SIGINT"},
    {SIGQUIT, "SIGQUIT"},
    {SIGILL, "SIGILL"},
    {SIGTRAP, "SIGTRAP"},
    {SIGABRT, "SIGABRT"},
    {SIGBUS, "SIGBUS"},
    {SIGFPE, "SIGFPE"},
    {SIGKILL, "SIGKILL"},
    {SIGUSR1, "SIGUSR1"},
    {SIGSEGV, "SIGSEGV"},
    {SIGUSR2, "SIGUSR2"},
    {SIGPIPE, "SIGPIPE"},
    {SIGALRM, "SIGALRM"},
    {SIGTERM, "SIGTERM"},
    {SIGSTKFLT, "SIGSTKFLT"},
    {SIGCHLD, "SIGCHLD"},
    {SIGCONT, "SIGCONT"},
    {SIGSTOP, "SIGSTOP"},
    {SIGTSTP, "SIGTSTP"},
    {SIGTTIN, "SIGTTIN"},
    {SIGTTOU, "SIGTTOU"},
    {SIGURG, "SIGURG"},
    {SIGXCPU, "SIGXCPU"},
    {SIGXFSZ, "SIGXFSZ"},
    {SIGVTALRM, "SIGVTALRM"},
    {SIGPROF, "SIGPROF"},
    {SIGWINCH, "SIGWINCH"},
    {SIGPOLL, "SIGPOLL"},
    {SIGIO, "SIGIO"},
    {SIGPWR, "SIGPWR"},
    {SIGSYS, "SIGSYS"},
};

static const char* signal_to_name(int signal_id)
{
    for(size_t i=0; i<sizeof(signal_names)/sizeof(*signal_names); ++i)
        if (signal_names[i].id == signal_id)
            return signal_names[i].name;
    return "";
}

static bool demangle_symbol(char* src,
                char* dst, size_t n_dst,
                size_t offsets[4])
{
    offsets[0] = 0;

    size_t i = 0;
    size_t j = 0;

    // copy location part
    for (;; i++, j++)
    {
        if (!src[i] || j >= n_dst)
            return false;
        if (src[i] == '(')
            break;
        dst[j] = src[i];
    }
    i++;
    dst[j++] = '\0';
    size_t name_begin = j;
    offsets[1] = name_begin;

    for (;;i++,j++)
    {
        if (!src[i] || j >= n_dst)
            return false;
        if (src[i] == '+')
             break;
        dst[j] = src[i];
    }
    dst[j++] = '\0';
    src[i++] = '\0';

    int status = 0;
    size_t size = n_dst - name_begin;
    abi::__cxa_demangle(src + name_begin, dst + name_begin,
                                    &size, &status);
    if (status == 0)
    {
        j = name_begin;
        for (;dst[j]; j++)
            if (j >= n_dst)
                return false;
        j++;
    }
    else if (status != -2)
        return false;

    // offset field
    offsets[2] = j;
    for (;; i++, j++)
    {
        if (!src[i] || j >= n_dst)
            return false;
        if (src[i] == ')')
            break;
        dst[j] = src[i];
    }
    dst[j++] = '\0';
    i++;

    // remaining line
    offsets[3] = j;
    for (;src[i]; i++, j++)
    {
        if (j >= n_dst)
            return false;
        dst[j] = src[i];
    }
    dst[j++] = '\0';

    return true;
}

void print_stack_trace(int depth_begin, int depth_end)
{
    const size_t BUF_SIZE = 2048;
    char buf[BUF_SIZE+1];
    size_t offsets[4];
    FILE* out = stderr;

    //fprintf(out, "backtrace:\n");

    const int MAX_STACK_FRAMES = 128;
    void* call_stack[MAX_STACK_FRAMES+1];
    int n = backtrace(call_stack, MAX_STACK_FRAMES);
    if (n)
    {
        char** symbols = backtrace_symbols(call_stack, n);
        for (int i = 0; i < n; i++)
        {
            //fprintf(out, "%i: %s\n", i, symbols[i]);
            if (i >= depth_begin &&
                (depth_end == 0 || i < depth_end))
            {
                if (demangle_symbol(symbols[i], buf,
                                    BUF_SIZE, offsets))
                    fprintf(out, "%i: %s %s +%s %s\n",
                            i,
                            buf + offsets[0], buf + offsets[1],
                            buf + offsets[2], buf + offsets[3]);
                else
                    fprintf(out, "%i: %s\n", i, symbols[i]);
            }
        }
        free(symbols);
    }
}

static void signal_handler(int signal_id)
{
    const char* name = signal_to_name(signal_id);
    fprintf(stderr, "caught signal %d (%s):\n", signal_id, name);
    print_stack_trace(0, 0);
    exit(signal_id);
}

void init_stack_trace()
{
    signal(SIGABRT, signal_handler);
    signal(SIGILL,  signal_handler);
    signal(SIGSEGV, signal_handler);
    signal(SIGFPE,  signal_handler);
    signal(SIGBUS,  signal_handler);
}

