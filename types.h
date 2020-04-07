typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;
typedef uint pde_t;
#define null ((void*)0)

struct perf {
    int ps_priority;
    int stime;
    int retime;
    int rtime;
};

//DEBUG FLAGS
#define DEBUGMODE 0
#define DEBUG_PRINT_PTABLE 0
#define DEBUG_CURRENT_RUNNING_PROC 0