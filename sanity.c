#include "types.h"
#include "stat.h"
#include "user.h"

int useSuperDummy =0;

int fibo(int n){
  if(n<=2 )
  return 1;
  return fibo(n-2) + fibo(n-1);
}

void calcfibo(int n){
  int i = fibo(n);
  if(i==1)
    i=2;
}

void dummy(){
  int i = 1000000;
  int dummy = 0;
  while(i--)
    dummy+=i;
}

void goPlay(){
  if(useSuperDummy)
    calcfibo(37);
  else 
    dummy();
}

void printStat(){
  struct perf stats;
  proc_info(&stats);
  printf(1,"%d \t%d \t\t%d \t%d \t%d\n", getpid(), stats.ps_priority, stats.stime, stats.retime, stats.rtime);
}


/**
 * In order to really see the scheduling effect, 
 * change @par numOfProcesses to 30 
 * and global @par useSuperDummy to 1
 */
int 
main(int argc, char *argv[])
{


  printf(1,"PID\tPS_PRIORITY\tSTIME\tRETIME\tRTIME\n");


  int numOfProcesses =3;
  int cpid = -1;
  int i=0;
  for(i=0;i<numOfProcesses && cpid != 0;i++){
    cpid = fork();
    if(cpid==0){
      switch(i % 3){
        case 0:
          //setting first child priorites
          set_cfs_priority(3);
          set_ps_priority(10);
          break;
        case 1:
          //setting first child priorites
          set_cfs_priority(2);
          set_ps_priority(5);
          break;
        case 2:
          //setting third child priorites
          set_cfs_priority(1);
          set_ps_priority(1);
          break;
      }
      goPlay();
      printStat();
      exit(0);
    }
    
  }
  
  for(i=0;i<numOfProcesses;i++)
    wait(null); 
  // printf(1,"sanity.c: exiting sanity process\n");
  // printStat();
  exit(0); 
} 

  

