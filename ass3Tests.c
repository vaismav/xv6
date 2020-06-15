#include "types.h"
#include "stat.h"
#include "user.h"
#define MAXPYSCPAGES 0xE000000

void changePages(char* buf,int pages){
    int i;
    printf(1,"testone.c: PID %d  start test: add %d pages\n",getpid(),pages);
    
    printf(1,"testone.c: PID %d  addPages: created buf = sbrk(4096*pages), pages = %d\n",getpid(),pages);
    for(i=0; i<pages;i++){
        printf(1,"testone.c: PID %d addPages: putting 'x' at address 0x%x\n",getpid(),&buf[i*4096]);
        buf[i*4096] = 'x';
    }

    for(i=0; i<pages;i++){
        printf(1, " PID %d round 1 data at address 0x%x : %c\n",getpid(),&buf[i*4096], buf[i*4096]);
    }
    
    for(i=0; i<pages;i++){
        printf(1, " PID %d round 2 data: %c\n",getpid(),buf[i*4096]);
    }

}



void forkIT(int n){
  printf(1,"forking the %d chiled, \n", n);
  if(n == 0){
    return;
  }
  
  int cpid = fork();
  if(cpid < 0){
        printf(1,"fork %d failed\n",n);
        exit();
  }else if(cpid == 0){
    forkIT(n-1);
    exit();
  }
  wait();
  exit();
}

int
main()
{
  int pid;
  int oldNumOfPages = getNumFreePages();
  int newNumOfPages;
  printf(1,"num of free pages is %d, \n", oldNumOfPages);



  pid = fork();
  if(pid < 0){
    printf(1,"fork failed\n");
    exit();
  }else if(pid==0){
      newNumOfPages=getNumFreePages();
      printf(1,"num of free pages after first fork %d, \n", newNumOfPages);
      int cpid2=fork();
      if(cpid2 < 0){
        printf(1,"fork failed\n");
        exit();
      }else if(cpid2 == 0){
        int num=getNumFreePages();
        printf(1,"num of free pages after first fork %d, \n", num);
        exit();
      }
    wait();

    exit();
  }
  wait();
   printf(1,"ass3Tests: finished first test\n");
  char* buff = sbrk(MAXPYSCPAGES/2);
  buff[0] = 'x';
  
  int numOfFork = 3;
  
  int cpid2 =fork();
   if(cpid2 < 0){
    printf(1,"fork failed. ending test\n");
    exit();
  }
  if(cpid2==0){
    forkIT(numOfFork);
    exit();
  }
  wait();
  printf(1,"ass3Tests: finished 2nd test\n");
  int pages = 29;
  char* buf = sbrk(4096*pages);
  int cpid3 = fork();
  if(cpid3 < 0){
    printf(1,"fork failed. ending test\n");
    exit();
  }
  changePages(buf,pages);
  if(cpid3 == 0)
    exit();
  wait();
  printf(1,"ass3Tests: finished 3rd test\n");
  exit();
}