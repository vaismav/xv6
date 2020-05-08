#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);






/**void sigret(void)
 * called implicitly when returning from user space after handling a signal. 
 * This system call is responsible for restoring the process to its original workflow
 */
void
sigret(void){
  struct proc *p= myproc();
  if(DEBUG && 1) cprintf("PID %d: proc.c: sigret: entered function \n",p->pid);
  memmove(p->tf,p->backup_tf,sizeof(struct trapframe));
  p->signal_Mask=p->signals_mask_backup;
}



/**uint sigprocmask(uint)
 * update the process signal mask, 
 * return the value of old mask
 */
uint
sigprocmask(uint newMask){
  // struct proc *p= myproc();
  // uint oldMask;
  // acquire(&ptable.lock);
  // oldMask=p->signal_Mask;
  // p->signal_Mask = newMask;
  // release(&ptable.lock);
  // return oldMask;

  //With CAS//TODO: show Avishay
  struct proc *p= myproc();
  uint oldMask;
  int changed_mask=1;
  do{
    oldMask=p->signal_Mask;
    if(p->signal_Mask!=newMask){
      changed_mask=0;
    }
  }while(changed_mask==0 && !cas(&p->signal_Mask,oldMask,newMask));
    p->signal_Mask = newMask;
  return oldMask;
}

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

//A process wishing to register a custom handler for a specific signal
//will use the following system call 
int
sigaction(int signum, const struct sigaction *act, struct sigaction *oldact){
  struct proc *p= myproc();
  
    if(signum==SIGKILL || signum==SIGSTOP)
      return -1;
    else{
      //With CAS TODO: show Avishay - Im not so sure
      int changed_signal_Handlers=1;
      int changed_signal_handlers_mask=1;
      uint old_sig_handl_mask;
      void * old_sig_handl;
    
      do{
        if(oldact!=0){
          if(p->signal_Handlers[signum]!=act->sa_handler){
            oldact->sa_handler = p->signal_Handlers[signum];
            changed_signal_Handlers=0;
          }
          if(p->siganl_handlers_mask[signum]!=act->sigmask){
            oldact->sigmask=p->siganl_handlers_mask[signum];
            changed_signal_handlers_mask=0;
          }
          old_sig_handl_mask=p->siganl_handlers_mask[signum];
          old_sig_handl=p->signal_Handlers[signum];
        }
      }while(!changed_signal_Handlers && !changed_signal_handlers_mask &&
      !cas(&p->siganl_handlers_mask[signum],old_sig_handl_mask,act->sigmask) &&
      !cas(&p->signal_Handlers[signum],(int)&old_sig_handl,(int)&act->sa_handler));

      p->signal_Handlers[signum]=act->sa_handler;
      p->siganl_handlers_mask[signum] = act->sigmask;

      //acquire(&ptable.lock);
      // if(oldact!=0){
      // oldact->sa_handler = p->signal_Handlers[signum];
      // oldact->sigmask=p->siganl_handlers_mask[signum];
      // }
      // p->signal_Handlers[signum]=act->sa_handler;
      // p->siganl_handlers_mask[signum] = act->sigmask;
      //release(&ptable.lock);
      return 0;
    }
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}




int 
allocpid(void) 
{
  // int pid;
  // acquire(&ptable.lock);
  // pid = nextpid++;
  // release(&ptable.lock);

//With CAS
  int oldpid;
  do{
     oldpid=nextpid;
   }
  while (!cas(&nextpid, oldpid, oldpid+1));
  return oldpid+1;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;
  int old_state;
  int still_unused=1;

  //acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED){
      //With CAS
      do{
        old_state=UNUSED;
        if(p->state!=UNUSED)
          still_unused=0;
      }while(still_unused && !cas(&p->state,old_state,EMBRYO));
      
      if(still_unused)
        goto found;
    }

  //release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  //release(&ptable.lock);

  p->pid = allocpid();
  //Assignment2:updating signals fields of struct proc
  
  int i;
  for(i=0;i<32;i++){
    p->signal_Handlers[i]=SIG_DFL;
  }
  p->pending_Signals= 0;
  p->signal_Mask=0;
  
  // Allocate backup trapframe 
  if((p->backup_tf = (struct trapframe*)kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  
  
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.n
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}




//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
  int old_state;
  int still_EMBRYO=1;

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.

  // acquire(&ptable.lock);
  // p->state = RUNNABLE;
  // release(&ptable.lock);

  //With CAS TODO: show Avishay
  do{
    old_state=p->state;
    if(p->state!=EMBRYO)
      still_EMBRYO=0;
  }while(still_EMBRYO && !cas(&p->state,old_state,RUNNABLE));
  p->state=RUNNABLE;
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();
  int old_state;
  int not_RUNNABLE=1;

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  //child inherits signal mask & signal handlers from parent
  np->signal_Mask=curproc->signal_Mask;
  for(int i=0;i<32;i++){
    np->signal_Handlers[i]=curproc->signal_Handlers[i];
  }
  

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  // acquire(&ptable.lock);
  // np->state = RUNNABLE; 
  // release(&ptable.lock);

  //With CAS TODO: show Avishay
  do{
    old_state=np->state;
    if(np->state==RUNNABLE)
      not_RUNNABLE=0;
  }while(not_RUNNABLE && !cas(&np->state,old_state,RUNNABLE));
  np->state=RUNNABLE;

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock); //TODO: consult Avishay

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock); //TODO: consult with Avishay
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock); //TODO: consult with Avishay
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  //With CAS TODO: show Avishay
  // int old_state;
  // int not_RUNNABLE=1;

  // do{
  //   old_state=myproc()->state;
  //   if(myproc()->state==RUNNABLE)
  //     not_RUNNABLE=0;
  // }while (not_RUNNABLE && !cas(&myproc()->state,old_state,RUNNABLE));
  // if(myproc()->state==RUNNABLE)
  //   sched();

  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock); //TODO: HOW TO USE CAS

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk); //TODO: HOW TO CHANGE TO CAS
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock); //TODO: HOW TO CHANGE TO CAS
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;
//With CAS TODO: show Avishay - may simplify the canges to cas - is the while replaces the if?
  // int old_state;
  // int still_SLEEPING=1;
  // int change_state=0;
  // for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  // do{
  //   old_state=p->state;
  //   if(p->state!=SLEEPING)
  //     still_SLEEPING=0;
  //   if(p->state == SLEEPING && p->chan == chan)
  //     change_state=1;
  // }while(still_SLEEPING && change_state && !cas(&p->state,old_state,RUNNABLE));
 // p->state=RUNNABLE; //TODO: need?

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock); 
  wakeup1(chan);
  release(&ptable.lock);
}

// int
// getProcSignalsData(struct procSignalsData *output){
//   struct proc* p=myproc();
//   acquire(&ptable.lock);
//   output->killed=&(p->killed);
//   output->pending_Signals = p->pending_Signals;           //32bit array, stored as type uint
//   output->signal_Mask = p->signal_Mask;               //32bit array, stored as type uint
//   *(output->signal_Handlers) = *(p->signal_Handlers);      //Array of size 32, of type void*
//   *(output->siganl_handlers_mask) = *(p->siganl_handlers_mask);  //Array of the signal mask of the signal handlers
//   release(&ptable.lock);
//   return 0;
// }

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid, int signum)
{
  if (DEBUG && 0) cprintf("proc.c: kill:  signum= %d sent to pid = %d, from proc:%d\n",signum,pid,myproc()->pid);
  struct proc *p;
  if(signum < 0 ||  31 < signum){ 
    cprintf("Error: proc.c: kill: ilegal signum value was sent to procces %d\n",pid);
    return -1;
  }
  acquire(&ptable.lock); //TODO: consult Avishay
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->pending_Signals |= 1U << signum;//turn on the 'signum'th bit in pending_Signals uint
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
