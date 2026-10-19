#pragma once
#include <cstdio> // fopen, fscanf
#include <cstdlib> // malloc, free
#include <sys/resource.h> // setrlimit
#include <sys/wait.h> // waitpid
#include <unistd.h> // fork, alarm, _exit, sysconf

/// The exit code of a child which RLIMIT_AS does not limit, as under qemu-user.
inline constexpr int NO_MEMORY_LIMIT = 3;

/// @brief Runs body() in a child process whose address space has 1 MB to spare, so a large allocation fails.
/// @returns 0 when body() returns true, NO_MEMORY_LIMIT when the limit does not apply, or -1 when a signal ended the child.
///          SIGALRM ends a child which does not end within a second.
template<class Body>
int run_with_little_memory(Body body)
{
    const pid_t child = fork();
    if (child == 0)
    {
        alarm(1);
        long pages = 0;
        FILE* statm = fopen("/proc/self/statm", "r");
        if (!statm || fscanf(statm, "%ld", &pages) != 1) _exit(2);
        fclose(statm);
        rlimit limit {};
        getrlimit(RLIMIT_AS, &limit);
        limit.rlim_cur = rlim_t(pages) * rlim_t(sysconf(_SC_PAGESIZE)) + (1 << 20);
        if (setrlimit(RLIMIT_AS, &limit) != 0) _exit(2);
        if (void* probe = malloc(4 << 20)) { free(probe); _exit(NO_MEMORY_LIMIT); }
        _exit(body() ? 0 : 1);
    }
    int status = 0;
    if (waitpid(child, &status, 0) != child || !WIFEXITED(status)) return -1;
    return WEXITSTATUS(status);
}
