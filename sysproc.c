#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}
//ours sigaction
int 
sys_sigaction(void){
  int p_signum;
  if(argint (0,&p_signum)<0)

    return -1;
  const struct sigaction *p_act;
  if(argptr(1,(char **)&p_act,sizeof(struct sigaction))<0)
    return -1;
  struct sigaction *p_oldact;
  if(argptr(2,(char **)&p_oldact,sizeof(struct sigaction))<0)
    return -1;

  return sigaction(p_signum,p_act,p_oldact);
  
}

/**void sigret(void)
 * called implicitly when returning from user space after handling a signal. 
 * This system call is responsible for restoring the process to its original workflow
 */
void
sys_sigret(void){
  return sigret(); //TODO: need it at all?
}

/**sys_sigprocmask
 * gets a uint value of the new signal mask
 * should return value of old signal mask
 */ 
uint
sys_sigprocmask(void){
  uint mask;
  if(argint(0, (int*)&mask) < 0)
    return -1;
  return sigprocmask(mask);
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;
  int signum;

  if(argint(0, &pid) < 0)
    return -1;
  if(argint(1, &signum) < 0)
    return -1;

  return kill(pid,signum);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}



// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
