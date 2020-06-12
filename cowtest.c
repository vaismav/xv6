#include "types.h"
#include "stat.h"
#include "user.h"

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
      if(oldNumOfPages!=newNumOfPages)
        printf(1,"the MOO is not working right - sould be the same num of free pages\n");
    exit();
  }
}