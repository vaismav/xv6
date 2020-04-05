#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "limits.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void 
updateCFSstatistics(){
  struct proc *p;
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
  switch (p->state)
    {
    case RUNNABLE:     
      p->retime ++;   //Increase the value of the time the process was ready to run 
      break;
    case SLEEPING:
      p->stime ++;  //Increase the value of the process sleep time indicator
      break;
    case RUNNING:
      p->rtime ++;  //Increase the value of the process running time indicator
      break;
      default:
      break;
    }
  }
  release(&ptable.lock);
}

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
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

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
    //cprintf("p state unused pid %d \n",p->pid);
      goto found;

  release(&ptable.lock);
  return 0;

found:

    if(DEBUGMODE) cprintf("p state embryo  pid %d \n",p->pid);
  p->state = EMBRYO;

  p->pid = nextpid++;
   if(DEBUGMODE) cprintf("p state embryo after ++ pid %d nextid:%d \n",p->pid, nextpid);

  //Handle Priority Scheduling fields
  p->ps_priority=5; //priority of a new process in priority schedualer
  if(DEBUGMODE) cprintf("before accumuletor in allocproc \n");
  //accumulator set  to a new process
  long long acc=LLONG_MAX;
  struct proc *q; 
  for(q = ptable.proc; q < &ptable.proc[NPROC]; q++){
    if(DEBUGMODE) cprintf("in for for q \n");
    if(q->state == RUNNABLE || q->state == RUNNING){
     if(q->accumulator<acc)
        acc=q->accumulator;
    }
  }
  p->accumulator=acc;
  if(DEBUGMODE) cprintf("after accumuletor in allocproc \n");

  if(DEBUGMODE) cprintf("before release ptable in allocproc \n");
  release(&ptable.lock);
  if(DEBUGMODE) cprintf("relesed ptanle in allocproc \n");
  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
     if(DEBUGMODE) cprintf("p faild to kalloc kstack pid %d \n",p->pid);
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;
  if(DEBUGMODE) cprintf("end of allocproc \n");
  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
  cprintf("initial proc enters allocproc  \n" );
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

  //Set Up initial process scheduling fields:
  //Priority Scheduling fields
  p->ps_priority = 5;
  p->accumulator = 0;
  //Comlily Fair Schedualing
  p->cfs_priority = 2;      //initialize the init process to normal defactor
  p->rtime = 0;
  p->retime = 0;
  p->stime = 0;

  //Insuring sched_type is 0 (although it shoud be with defined with 0 upon declaretion)
  sched_type = 0;
  

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);
 cprintf("p before turn to runnable oid %d \n",p->pid);
  p->state = RUNNABLE;

  release(&ptable.lock);
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

int
memsize(void){
  struct proc *p = myproc();
  return p->sz;
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

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }
  //Handle Completely Fair Scheduling fields
  np->cfs_priority = curproc->cfs_priority;
  np->retime = 0;
  np->rtime = 0;
  np->stime = 0;
   cprintf("p state unused in fork before if pid %d \n",np->pid);
  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
     cprintf("p state unused in fork in if pid %d \n",np->pid);
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;
  
  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(int status)
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
  curproc->status=status;

  acquire(&ptable.lock);

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
wait(int *status)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        if(status!=0)
          *status=p->status;
           cprintf("p state unused in wait pid %d \n",p->pid);
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

int    
set_ps_priority(int priority)     //priority 4.2
{
  struct proc *curproc = myproc();
  curproc->ps_priority=priority;
  return 0;
}

int
proc_info(struct perf *preformance){
  struct proc *curproc = myproc();
  preformance->retime=curproc->retime;
  preformance->rtime=curproc->rtime;
  preformance->stime=curproc->stime;
  preformance->ps_priority=curproc->ps_priority;
   
    return 0;
}

int
policy(int policy_num){   // TODO: is this right?
  sched_type=policy_num;
  return 0;
}

/**CSF priority for task 4.3 in assinment 1
 * 1. High priority – Decay factor = 0.75
 * 2. Normal priority – Decay factor = 1
 * 3. Low priority – Decay factor = 1.25 
 */
int 
set_cfs_priority(int priority){
  if(priority<1 || priority>3){
    cprintf("ERROR: set_cfs_priority, input out of range [1,3], input value = %d",priority);
    return -1;
  }
  struct proc *curproc = myproc();
  curproc->cfs_priority=priority;
  return 0;
}

//

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
  // cprintf("main process got in scheduler\n");
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;

  //additional settings by Maya And Avishai
  struct proc *curr_proc;
  // long long acc_min=LLONG_MAX;
  // int canSwitch;                          //boolian to indicat if found a procces to run
  // int foundRUNNABLE;                         //boolian to indicat if schedualer found a RUNNABLE Process
  // double pRunTimeRatio;                      //will hold the value of the minimal time run ratio 
  // double currRunTimeRatio;                   //hold the value of the current process time run ratio 
  // double decayFactor[4]={0.0,0.75,1,1.25};   //array of CFS decay factors
  // p=ptable.proc;

  
  
  for(;;){
    //cprintf("got in infinity sched loop\n");
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);

    // canSwitch = 0; 
    // foundRUNNABLE = 0;
    // pRunTimeRatio = 9999999999.0;
    for(curr_proc = ptable.proc; curr_proc < &ptable.proc[NPROC]; curr_proc++){
    /*  //cprintf("outside for looop process got in scheduler\n");
       switch (sched_type)
       {
        case 1:
        cprintf("case 1 process got in scheduler\n");

            if(acc_min>(curr_proc->accumulator) && curr_proc->state == RUNNABLE){
              acc_min=curr_proc->accumulator;
              p=curr_proc;
            }
            foundRUNNABLE = 1;
            canSwitch = 1;
            break;
        case 2:
          if (curr_proc->state == RUNNABLE){
            foundRUNNABLE = 1;
            currRunTimeRatio =(((double)curr_proc->rtime)*decayFactor[curr_proc->cfs_priority])
                                      /(double)((curr_proc->retime) + (curr_proc->rtime) + (curr_proc->stime));
            if(currRunTimeRatio < pRunTimeRatio){
              p = curr_proc;
              pRunTimeRatio = currRunTimeRatio;
            }
          }
          //At the end of ptable.proc  
          if(curr_proc == &ptable.proc[NPROC-1] && foundRUNNABLE)
            canSwitch = 1;
          break;

        default:
        cprintf("choose defult sched, check pid:%d, state:%d \n",curr_proc->pid,(int)curr_proc->state);
            //the scheduler will update p only in the first time it reach a RUNNABLE process
            if(curr_proc->state == RUNNABLE && !foundRUNNABLE){ //in the default sched as round robin we want
              cprintf("found RUNNABLE process \n");
              p=curr_proc;                                      //to go through all the ptable to update the cfs statistics
              foundRUNNABLE = 1;                                //therefore if we found a RUNNABLE process we dont want to
            }                                                   //to switch to it until updating the intire ptable

            //At the end of ptable.proc  if we found a 
            cprintf("before end of ptable, foundRUNNABLE = %d  \n",foundRUNNABLE);
          if(curr_proc == &ptable.proc[NPROC-1] && foundRUNNABLE){
            cprintf("turning On canSwitch Flag \n");
            canSwitch = 1;
          }
            break;
       }

      cprintf("before not switch  \n");
       if(!canSwitch)
          continue;
        
      cprintf("choose process No %d \n",p->pid);
      */
      
      if(curr_proc->state != RUNNABLE)
        continue;
      p=curr_proc;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&(c->scheduler), p->context);
      
      switchkvm();
      p->accumulator=(p->accumulator)+(p->ps_priority);// accumulator update, process finished quantom
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
  release(&ptable.lock);

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
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan) //TODO fixxx
{
  struct proc *p;

  long long acc_min= LLONG_MAX;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == SLEEPING && p->chan == chan){
      p->state = RUNNABLE;
      //update accumulator
      struct proc *q; 
      int runnable_counter=0;
      for(q = ptable.proc; q < &ptable.proc[NPROC]; q++){
       if(q->state == RUNNABLE || q->state == RUNNING){
         if(q->state == RUNNABLE)
            runnable_counter=runnable_counter+1;
         if(q->accumulator<acc_min)
           acc_min=q->accumulator;
       }
      }
      if(runnable_counter==1)
        p->accumulator=0;
      else
         p->accumulator=acc_min;
    }
      
  }
  
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);


  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
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
     cprintf("p state unused in procdump pid %d \n",p->pid);
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
