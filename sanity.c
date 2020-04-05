#include "types.h"
#include "stat.h"
#include "user.h"

int 
main(int argc, char *argv[])
{
  int pid[3];
  int cpid =fork();   //create first dummy child
  if(cpid==0){
    //setting first child priorites
    set_cfs_priority(10);
    set_ps_priority(1);
    //first dummy child start working
    int i0 = 1000000;
    int dummy0 = 0;
    while(i0--)
    dummy0+=i0;
  }
  else{
    pid[1] = cpid;
    cpid =fork();   //create second dummy child
    if(cpid==0){
      //setting first child priorites
      set_cfs_priority(5);
      set_ps_priority(2);

      //second dummy child start working
      int i1 = 1000000;
      int dummy1 = 0;
      while(i1--)
      dummy1+=i1;
    }
    else{
      pid[2] = cpid;
      cpid =fork();   //create second dummy child
      if(cpid==0){
        //second dummy child start working
        int i2 = 1000000;
        int dummy2 = 0;
        while(i2--)
        dummy2+=i2;
      }
    } 
  } 
}
