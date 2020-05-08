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
handleSignal(struct trapframe *tf);

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
trap(struct trapframe *tf) //tf= *[ebp+8] = parm1= address of esp*=777 ; &tf=778 = ebp+8
{
  
  if(tf->trapno == T_SYSCALL){
    if((DEBUG && 1) &&  (myproc()->pid == 4) && (tf->eax != 16)) cprintf("\nPID %d: trap.c: trap: ENTERED TRAP for SYSCALL with syscall %d \n",myproc()->pid,tf->eax);
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
      // we go trough trap so need to handle the signals
    if(DEBUG && 0) cprintf("\nPID %d: trap.c: trap: trapno:T_SYSCALL:try to enter handleSignal with  proc address=%x \n",myproc()->pid ,myproc());
    if(myproc()!=0){
      handleSignal(tf);
      if((DEBUG && 1)&& (myproc()->pid == 4) && ( tf->eax != 16)) cprintf("\nPID %d: trap.c: trap: EXIT TRAP after SYSCALL with syscall %d \n",myproc()->pid,tf->eax);
    }
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
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
    cprintf("\ncpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("\nunexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("\npid %d %s: trap %d err %d on cpu %d "
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
  
  if(myproc()!=0){     //prevent from tring to handle signals when xv6 initalizing
    if(DEBUG && 1 && (myproc()->pid == 4)) cprintf("\nPID %d: trap.c: trap: about to enter handleSignal after INTERRUPT \n",myproc()->pid);
    handleSignal(tf);
    if((DEBUG || 0)&& (myproc()->pid == 4)) cprintf("\nPID %d: trap.c: trap: EXIT TRAP after INTERRUPT  \n",myproc()->pid);
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Maya & Avishai's functions


void callSigret(void){
  asm("mov %0, %%eax"::"i"(24));
  asm("int %0"::"i"(64));
}
void endOfCallSigret(void){}

//resetting the pending signal bit
//NOTICE: it locks the p.table!
void
resetPendingSignal(struct proc *p ,uint signum){
  if(DEBUG && 1) cprintf("\nPID %d: trap.c: resetPendingSignal: RESET PENDING SIGNAL %d \n",p->pid,signum);
  p->pending_Signals ^=(1U<<signum);
}

void
handleSignal(struct trapframe *tf){
  struct proc *p= myproc();
  if(p!=0 && (DEBUG && 0)) cprintf("\nPID %d: trap.c: handleSignal: entered function\n",p->pid);
  for(int signum=0; signum<32;signum++){
    if(DEBUG && 0) cprintf("\nPID %d: trap.c: handleSignal:signals loop: checking signum %d \n",p->pid,signum);
    // checks if the signal is pending
    if(p->pending_Signals & (1U<<signum)){         
      if(DEBUG && 0) cprintf("\nPID %d: trap.c: handleSignal:%d is a pending signal\n",p->pid,signum);
      // checks if the signal is blocked
      if(p->signal_Mask & 1U<<signum){          
        //no need to reset SIG_IGN 
        if(DEBUG && 1) cprintf("\nPID %d: trap.c: handleSignal: %d is a pending and blocked signal\n",p->pid,signum);
      }
      else {
        //if signal is pendig and not blocked
        if(DEBUG && 1 ) cprintf("\nPID %d: trap.c: handleSignal:%d is a pending signal and not blocked. p->signal_Handlers[%d] = %x \n",p->pid,signum,signum,p->signal_Handlers[signum]);
        switch((int)p->signal_Handlers[signum]){ 
          case SIG_IGN:
            if(DEBUG && 1) cprintf("\nPID %d: trap.c: handleSignal: default handler for SIG_IGN\n",p->pid); 
            resetPendingSignal(p,SIG_IGN); //if the handler is SIG_IGN, then the bit should be reset - forum
            break;
          case SIG_DFL: //defult signal handling, if signal is pendig, not blocked and without specifc handler
            if(DEBUG && 1) cprintf("\nPID %d: trap.c: handleSignal:defaulr handler for SIG_DFL \n",p->pid);
            switch(signum){

              case SIGSTOP:
              if(DEBUG && 1) cprintf("\nPID %d: trap.c: handleSignal:defaulr handler for SIGSTOP\n",p->pid);
                while(!(p->pending_Signals & 1U<<SIGCONT)){ //got sigcount
                if(DEBUG && 1) cprintf("\nPID %d: trap.c: handleSignal:defaulr handler for SIGSTOP. wait for SIGCONT \n",p->pid);
                  //give up cpu for another process
                  yield();  
                }
                if(DEBUG && 1) cprintf("\nPID %d: trap.c: handleSignal:defaulr handler for SIG_DFL with SIGCONT \n",p->pid);
                resetPendingSignal(p,SIGCONT); //reset the SIGCONT
                resetPendingSignal(p,SIGSTOP); //reset the SIGSTOP
                break;
              
              case SIGCONT:
              if(DEBUG && 0) cprintf("\nPID %d: trap.c: handleSignal: default handler for SIG_CONT \n",p->pid); 
                resetPendingSignal(p,SIGCONT); //SIGCONT on a non stopped process will have no affect
                break;
              
              default: //default signal be
                if(DEBUG && 0) cprintf("\nPID %d: trap.c: handleSignal: default handler for signal:%d\n",p->pid,signum); 
                resetPendingSignal(p, (uint)signum);
                p->killed = 1;
                // Wake process from sleep if necessary. TODO: need to ass CAS usage?
                if(p->state == SLEEPING)
                  p->state = RUNNABLE;
                return;
              
            }
            break;
          
          default:  //for every specific sig handler
          
            if(DEBUG && 1) cprintf("\nPID %d: trap.c: handleSignal: handling user handler for signal:%d \n",p->pid,signum); 
            //reseting the signal pending bit
            resetPendingSignal(p, (uint)signum);
            
            //backing up current signals mask
            p->signals_mask_backup =p->signal_Mask;
            
            //changing proc->signal_Mask to the handler mask
            p->signal_Mask=p->siganl_handlers_mask[signum];
            if(DEBUG && 1) cprintf("\nPID %d:trap.c: handleSignal: p->signal_Mask= 0x%x \n",p->pid,p->signal_Mask);
            
            //backing up user trapframe
            memmove(p->backup_tf,tf,sizeof(struct trapframe));

            //pushing the callSigret function code to the stack
            int codesize=endOfCallSigret-callSigret;
            if(DEBUG && 1) cprintf("\n1 codesize=%d, sigretCallEnd= 0x%x, sigretCall= 0x%x , tf->esp= 0x%x\n",codesize,endOfCallSigret,callSigret,tf->esp);
            tf->esp -=codesize;
            if(DEBUG && 1) cprintf("\n2\n");
            memmove((void*)(tf->esp),callSigret,codesize);
            if(DEBUG && 1) cprintf("\n3\n");
            uint retAddress=tf->esp;
            if(DEBUG && 1) cprintf("\n4 retAddress=0x%x\n",retAddress);

            //pushing pointer for user esp and pushing the signum
            if(DEBUG && 1) cprintf("\nPID %d:trap.c: handleSignal: before pushing sgnum to esp signum = %d \n",p->pid,signum);
            tf->esp -=sizeof(int);
            memmove((void*)(tf->esp),&signum,sizeof(uint));
            if(DEBUG && 1) cprintf("\nPID %d: trap.c: handleSignal: after pushing sgnum to esp signum = %d \n",p->pid,signum);
            
            //pushing the signal handler user function
            tf->esp -= sizeof(uint);
            memmove((void*)(tf->esp),&retAddress,4);
            

            //changing the instruction pointer (eip) to the signal handler 
            //so it will jump to the handler once exiting the trap
            if(DEBUG && 1) cprintf("\nPID %d: trap.c: handleSignal: about to change eip: signum = %d \n",p->pid,signum);
            if(DEBUG && 1) cprintf("\nPID %d: trap.c: handleSignal: putting  p->signal_Handlers[signum] = 0x%x  in tf->eip \n",p->pid,p->signal_Handlers[signum]);
            tf->eip=(uint)p->signal_Handlers[signum]; 
            
            //exitig to exit trap()
            if(DEBUG && 1) cprintf("\nPID %d: trap.c: handleSignal: exit back to trap with tf->eip = 0x%x \n",p->pid,tf->eip); 
            return;
        }
      }
    
    }
    if(DEBUG && 0) cprintf("\nPID %d: trap.c: handleSignal: finish handling signum %d, moving to the next signal\n",p->pid,signum); 
  }
  if(p!=0 && DEBUG && 0) cprintf("\nPID %d: trap.c: handleSignal: exit back to trap\n",p->pid); 
}
