typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;
typedef uint pde_t;
#define null; //TODO: needed?

#define SIG_DFL 0  //default signal handeling
#define SIG_IGN 1  //ignor signal
#define SIGKILL 9
#define SIGSTOP 17
#define SIGCONT 19

struct sigaction{
  void (*sa_handler)(int);
  uint sigmask;
};