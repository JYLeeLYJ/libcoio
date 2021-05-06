#ifndef COIO_CPU_AFFINITY
#define COIO_CPU_AFFINITY

#include <unistd.h>
#include <sched.h>
#include <sys/syscall.h>

namespace coio{

static inline bool reset_cpu_affinity(int cpuno){
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpuno , &mask);

    auto tid = syscall(SYS_gettid);
    return sched_setaffinity(tid , sizeof(mask) , &mask) != -1;
}

}

#endif