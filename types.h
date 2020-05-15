typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;
typedef uint pde_t;
#define null 0 
#define false 0
#define true 1

#define DEBUG 0

#define OCCUPIED 1
#define UNOCCUPIED 0

#define SIG_DFL 0  //default signal handeling
#define SIG_IGN 1  //ignor signal
#define SIGKILL 9
#define SIGSTOP 17
#define SIGCONT 19

#define DFL_SIGMASK 0
struct sigaction{
  void (*sa_handler)(int);
  uint sigmask;
};

struct procSignalsData{
  int* killed;                    //pointer to the proc killed status
  uint pending_Signals;           //32bit array, stored as type uint
  uint signal_Mask;               //32bit array, stored as type uint
  void* signal_Handlers[32];      //Array of size 32, of type void*
  uint siganl_handlers_mask[32];  //Array of the signal mask of the signal handlers
};



