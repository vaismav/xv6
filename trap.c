#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  int defineNONE = 0;
  #ifdef NONE
    defineNONE=1;
  #endif

  uint address;
  pte_t *pte;
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
       #ifndef NONE
        //andle the aging macanisem in NONE is not defined.
        if( isValidUserProc(myproc()) ){
          if(1) cprintf("\ntrap.c: trap: PID %d NONE is NOT defined, about to handle_aging_counter\n",myproc()->pid);
          handle_aging_counter(myproc()); 
        }
      #endif
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;
  // handleing page fault
  
  case T_PGFLT:
    myproc()->numOfPagedFault++;
    #ifndef NONE
        //andle the aging macanisem in NONE is not defined.
        if( isValidUserProc(myproc()) ){
          if(1) cprintf("\ntrap.c: trap: PID %d NONE is NOT defined, about to handle_aging_counter\n",myproc()->pid);
          handle_aging_counter(myproc()); 
        }
    #endif
    if(!defineNONE){
      address = rcr2();
      if(1 && myproc() != 0) cprintf("trap.c: trap: PID %d: T_PGFLT on address 0x%x\n ",myproc()->pid,address);
    //1) in swap file - need to swap back to pysical memory
    //2) not it pgdir - need to create
    //3) RO - first p try to write -> make writeable copy
    //        second p try to write -> make W and try writung again
      pte = (pte_t*)getPTE(myproc()->pgdir,(void*)address);
      if (*pte & PTE_P){
        //COW
        // //if page is present
        // //checks if page is writable
        // if(!(PTE_W & PTE_FLAGS(pte[PTX(address)]))){
        //   myproc()->tf = tf;
        //   handle_write_fault();
        //   if(myproc()->killed)
        //     exit();
        //   break;
        // }
      }
      // if page is not present but in swap
      else if(*pte & PTE_PG){
        if(0 && myproc() != 0) cprintf("trap.c: trap: PID %d: pagefault, page is in swap on address 0x%x\n ",myproc()->pid, address);
        loadPageToMemory(address);
        break;
      }
      //we dont break this case
      // so if T_PGFAULT isnt becaus the page is in swap, than the algorithem behave normally
    }


  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
