#include "types.h"
#include "stat.h"
#include "user.h"

void goPlay(){
  
  int i = 100000;
    int dummy = 0;
    while(i--){
    dummy+=1;
    }
  
}

void printStat(){
  struct perf stats;
  proc_info(&stats);
  printf(1,"%d \t %d \t %d \t %d \t %d\n", getpid(), stats.ps_priority, stats.stime, stats.retime, stats.rtime);
}

int 
main(int argc, char *argv[])
{
  printf(1,"Running Sanity test for scheduling...\nPID PS_PRIORITY STIME RETIME RTIME\n");

  if(fork()==0){    //create first dummy child
    //setting first child priorites
    set_cfs_priority(1);
    set_ps_priority(10);
    //first dummy child start working
    goPlay();
    printStat();
  }
  else if(fork()==0){   //create second dummy child
    //setting first child priorites
    set_cfs_priority(2);
    set_ps_priority(5);

    //second dummy child start working
    goPlay();
    printStat();
  }
  else if(fork()==0){   //create third dummy child
    //setting third child priorites
    set_cfs_priority(3);
    set_ps_priority(1);

    //second dummy child start working
    goPlay();
    printStat();
  }
  else{
    //Father of all
    wait(null);
    wait(null);
    wait(null);
    printStat();
    
  }
 
  

  exit(0); 
} 

  

