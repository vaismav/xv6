#include "types.h"
#include "stat.h"
#include "user.h"

void goPlay(){
  int i = 1000000;
    int dummy = 0;
    while(i--)
    dummy+=i;
}

void printStat(){
  struct perf stats;
  proc_info(&stats);
  printf(1,"%d\t%d\t%d\t%d\t%d\n", getpid(), stats.ps_priority, stats.stime, stats.retime, stats.rtime);
}

int 
main(int argc, char *argv[])
{
  printf(1,"PID PS_PRIORITY STIME RETIME RTIME\n");

  if(fork()==0){    //create first dummy child
    //setting first child priorites
    set_cfs_priority(10);
    set_ps_priority(1);
    //first dummy child start working
    goPlay();
    printStat();
  }
  else if(fork()==0){   //create second dummy child
    //setting first child priorites
    set_cfs_priority(5);
    set_ps_priority(2);

    //second dummy child start working
    goPlay();
    printStat();
  }
  else if(fork()==0){   //create third dummy child
    //setting third child priorites
    set_cfs_priority(1);
    set_ps_priority(3);

    //second dummy child start working
    goPlay();
    printStat();
  }
  else{
    //Father of all
    wait(null);
    wait(null);
    wait(null);
  }
  exit(0); 
} 

  

