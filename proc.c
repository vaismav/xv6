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

int
getNumFreePages(void){
  return currFreePages();
}

// gets a proc and print its 
// memory pages queue from
// HEAD->to->TAIL
void
printMemoryPagesQueue(struct proc* p){
  int index = p->headOfMemoryPages;
  cprintf("proc.c: printMemoryPagesQueue: p->pid %d: head index is %d\n",p->pid,index);
  //check queue is empty
  if(index == -1){
    cprintf("proc.c: printMemoryPagesQueue: p->pid %d: QUEUE: empty\n",p->pid);
    return;
  }
  //print the head of the queue
  cprintf("proc.c:printMemoryPagesQueue:p->pid%d:HEAD: 0x%x",p->pid, p->memoryPages[index].va);
  index = p->memoryPages[index].next;
  //printing the queue
  while(index > 0 && index < 16){
    cprintf("->0x%x",p->memoryPages[index].va);
    if( index == p->memoryPages[index].next)
      panic("proc.c: printMemoryPagesQueue: link next point on itself");
    index = p->memoryPages[index].next;
  }
  //printing page fault's and page out's
  cprintf("page faults= %d, page outs= %d \n",p->numOfPagedFault, p->numOfPagedOut);
  //ending the line
  cprintf("\n");
}

//duplicate the mamoryPage array and the swapPage array of source proc to dest proc
void
duplicate_page_arrays(struct proc *source, struct proc *dest){
  for (int i = 0; i < MAX_PSYC_PAGES; i++){
      dest->swapPages[i].is_occupied=source->swapPages[i].is_occupied;
      dest->swapPages[i].va=source->swapPages[i].va; 
      dest->memoryPages[i].va=source->memoryPages[i].va;
      dest->memoryPages[i].prev=source->memoryPages[i].prev;
      dest->memoryPages[i].next=source->memoryPages[i].next;
      dest->memoryPages[i].age=source->memoryPages[i].age;
      dest->memoryPages[i].is_occupied=source->memoryPages[i].is_occupied;
      if(i==MAX_PSYC_PAGES-1){
        dest->swapPages[i+1].is_occupied=source->swapPages[i+1].is_occupied;
        dest->swapPages[i+1].va=source->swapPages[i+1].va; 
      }
    }
}

//initialie the mamoryPage and swapPage arrays
void 
init_page_arrays(struct proc *p){
  for (int i = 0; i < MAX_PSYC_PAGES; i++){
      p->swapPages[i].is_occupied=0;
      p->swapPages[i].va=-1;
      p->memoryPages[i].va=-1;
      p->memoryPages[i].prev=-1;
      p->memoryPages[i].next=-1;
      p->memoryPages[i].age=0;
      p->memoryPages[i].is_occupied=0;
      if(i==MAX_PSYC_PAGES-1){
        p->swapPages[i+1].is_occupied=0;
        p->swapPages[i+1].va=-1; 
      }
    }
}

//handle changes in proc data at each clock tick
void
handle_aging_counter(struct proc* p){
  int nfuaOrLapa =0;
  #ifdef NFUA
    nfuaOrLapa=1;
  #endif
  #ifdef LAPA
    nfuaOrLapa=1;
  #endif
  
  if(nfuaOrLapa){
    if(1) cprintf("proc.c: handle_aging_counter:NFUA/LAPA: p->pid %d start aging\n",p->pid);

    acquire(&ptable.lock);
    for (int i = 0; i < MAX_PSYC_PAGES; i++){
      //not accessd - shift right by 1 bit
      if(p->memoryPages[i].is_occupied){
        if(0) cprintf("proc.c: handle_aging_counter:NFUA/LAPA: p->pid %d page 0x%x is in index %d \tage 0x%x\n",p->pid,p->memoryPages[i].va,i,p->memoryPages[i].age);   
        p->memoryPages[i].age = p->memoryPages[i].age >> 1;   
        //accessd - shift right by 1 bit & add 1 to msb
        if(checkPTE_A(p,p->memoryPages[i].va)){
          if(1) cprintf("proc.c: handle_aging_counter:NFUA/LAPA: p->pid %d page 0x%x was accsessed\n",p->pid,p->memoryPages[i].va);
          p->memoryPages[i].age |= 0x80000000; 
        }
      }
    }
    release(&ptable.lock);
  }
  
  #ifdef AQ //TODO: check if ok?
    acquire(&ptable.lock);
    //the queue starts tail-->head
    //takes the tail

    //debug
    if(1){
      cprintf("proc.c: handle_aging_counter:AQ: p->pid %d start passing on the pages queue\n",p->pid);
      printMemoryPagesQueue(p);
    }
    int index=p->headOfMemoryPages;
    //if PTE_A=1 change places w- the next
    while(index > 0 && index < 16){// when index= -1 we got to the tail
      int index_prev=p->memoryPages[index].prev;
      int index_next=p->memoryPages[index].next;
      if(p->memoryPages[index].is_occupied){
        if(checkPTE_A(p,p->memoryPages[index].va)){
          
          if(1) cprintf("proc.c: handle_aging_counter:AQ: p->pid %d page 0x%x was accsessed\n",p->pid,p->memoryPages[index].va);
          //as long as index isnt the tail, switching the link with its next
          if(index!=p->tailOfMemoryPages){

            // IF LINK is head
            if(index == p->headOfMemoryPages){
              //change my next and prev | update current link
              p->memoryPages[index].prev=index_next;
              p->memoryPages[index].next=p->memoryPages[index_next].next;
              
              // make next link head
              p->memoryPages[index_next].prev=-1;
              p->memoryPages[index_next].next=index;

              p->headOfMemoryPages=index_next;

            }
            else{
              // IF LINK is in the middle of the list
              //change index.prev | connect the prev link to the next link
              p->memoryPages[index_prev].next=index_next;
              
              //if not tail move the link 1 step forward to the tail
              //change my next and prev | update current link
              p->memoryPages[index].prev=index_next;
              p->memoryPages[index].next=p->memoryPages[index_next].next;
              
              //change next.prev | connect the next next 
              p->memoryPages[index_next].prev=index;

              //change next's next and prev to be mine
              p->memoryPages[index_next].prev=index_prev;
              p->memoryPages[index_next].next=index;

              //check if my index.next is tail
              if(index_next==p->tailOfMemoryPages){
                p->tailOfMemoryPages=index;
              }
            }
            
          }
          
          // if(1){
          //   cprintf("proc.c: handle_aging_counter:AQ: p->pid %d new queue state:\n",p->pid);
          //   printMemoryPagesQueue(p);
          // }
        }
        
      }
      //advancing to the next link
      index=index_next;
    }
    release(&ptable.lock);
  #endif
}
// 1-->2-->3-->4
// ref 2
// 1->3->2->4
// new 5 
// 3->2->4->5
// ref 3
// 2->3->4->5
// ref 4


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

// gets pointer to pgdir and
// return pointer to the proc
// which hold that pgdir.
// return 0 on error.
struct proc*
procOfpgdir(pde_t *pgdir){
  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state != UNUSED && p->pgdir==pgdir)
      return p;
  }
  return (struct proc*)0;
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
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);
  if(0) cprintf("proc.c: allocproc: PID %d start initializing proc  current pages in memory=%d\n",p->pid,p->pagesInMemory);

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
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  #ifndef NONE
    // Initialize all the paging structs
    // initialze for every proc which is not 1 or 2
    if(p->pid > 2){
      // initializing swapFile
      // upon failure free kstack and return 0
      if(createSwapFile(p)!=0){
        kfree(p->kstack);
        return 0;
      }
      //initialize
      init_page_arrays(p);

      p->pagesInMemory=0;
      p->pagesInSwap=0;
      // we use -1 to know the queue is empty
      p->headOfMemoryPages=-1;
      p->tailOfMemoryPages=-1;
      p->numOfPagedFault=0;
      p->numOfPagedOut=0;

      if(0) cprintf("proc.c: allocproc: PID %d about to initialize p->pagesInMemory=0 ,  current pages in memory=%d\n",p->pid,p->pagesInMemory);
    }
  #endif

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

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
  acquire(&ptable.lock);

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
    if(0) cprintf("proc.c: growproc: PID %d about to deallocuvm ,  current pages in memory=%d\n",curproc->pid,curproc->pagesInMemory);
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

  // Allocate process.
  if((np = allocproc()) == 0){
    if(0) cprintf("proc.c: fork: PID %d Failed to allocproc() new proc \n",curproc->pid);
    return -1;
  }
  if(0) cprintf("proc.c: fork: PID %d allocated process, pages in memory=%d\n",np->pid,np->pagesInMemory);

  // Copy process state from proc.
  if(COW){ //cow algorithm
    if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
      if(0) cprintf("proc.c: fork: PID %d FAILED to copyuvm to np->pgdir, np->pid=5d \n",curproc->pid,np->pid);

      kfree(np->kstack);
      np->kstack = 0;
      np->state = UNUSED;
      return -1;
    }
  }else{
    if((np->pgdir = cowuvm(curproc->pgdir, curproc->sz)) == 0){
      if(0) cprintf("proc.c: fork: PID %d FAILED to copyuvm to np->pgdir, np->pid=5d \n",curproc->pid,np->pid);

      kfree(np->kstack);
      np->kstack = 0;
      np->state = UNUSED;
      return -1;
    }
  }
  
  if(0) cprintf("proc.c: fork: PID %d copyied process state from proc, pages in memory=%d\n",np->pid,np->pagesInMemory);
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
  #ifndef NONE
    if(0) cprintf("proc.c: fork: PID %d before start of copy pages data, pages in memory=%d\n",np->pid,np->pagesInMemory);
    // START OF copy pages data
    np->pagesInMemory=curproc->pagesInMemory;
    np->pagesInSwap=curproc->pagesInSwap;
    np->headOfMemoryPages=curproc->headOfMemoryPages;
    np->tailOfMemoryPages=curproc->tailOfMemoryPages;
    np->numOfPagedOut=0;
    np->numOfPagedFault=0;
    //the swapFile of child is like the swapFile of parent
      
    if(0) cprintf("proc.c: fork: PID %d np->pid = %d\n",curproc->pid,np->pid);

    // copy the entire swapfile to the new proc
    if(curproc->pid > 2){
      char buffer[PGSIZE/2]=""; 
      if(0) cprintf("proc.c: fork: size of buffer in fork: %d\n", sizeof(buffer));
      int numRead=0;
      int placeOnFile=0;
      while((numRead=readFromSwapFile(curproc,buffer,placeOnFile,sizeof(buffer)))){ //have something to read
      if(np->pid>2){
        if(writeToSwapFile(np,buffer,placeOnFile,numRead)==-1) 
          return -1;
        placeOnFile+=numRead;
        } 
      }
      //duplicate the process
      duplicate_page_arrays(curproc,np);
    }
  #endif

  // END OF copy pages data
  if(0) cprintf("proc.c: fork: PID %d SUCCESSFULY created new proc np->pid =%d, np->pagesInMemory=%d\n",curproc->pid,np->pid,np->pagesInMemory);
  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

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
  //closeing swapfile
  #ifndef NONE  
    if(!(curproc->pid==1||curproc->parent->pid==1)){//NOT shell or init
      if(0) cprintf("proc.c: exit: PID %d about to remove swapFile\n",curproc->pid);

      if(curproc->pid > 2 && removeSwapFile(curproc)!=0)
          panic("proc.c: exit: couldnt remove swapFile");
    } //TODO: need to reset the arrays?
  #endif

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
wait(void)
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
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        if(0) cprintf("proc.c: wait: PID %d about to enter freevm for p->pid= %d, p->pagesInMemory=%d\n",curproc->pid, p->pid,p->pagesInMemory);

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
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      // #ifndef NONE
      //   //andle the aging macanisem in NONE is not defined.
      //   if(p->state != UNUSED && p->pid > 2 ){
      //     if(1) cprintf("proc.c: scheduler: p->pid %d NONE is NOT defined, about to handle_aging_counter\n",p->pid);
      //     handle_aging_counter(p); 
      //   }
      // #endif

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
wakeup1(void *chan)
{
  struct proc *p;

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
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    #ifdef FALSE
      cprintf("%d %s %s", p->pid, state, p->name);
    #endif
    #ifdef TRUE //new poutput line if VERBOSE_PRINT=TRUE
      int totalPaged = p->pagesInMemory + p->pagesInSwap;
      cprintf("%d %s %d %d %d %d %s ", p->pid, state, totalPaged, p->pagesInSwap, p->numOfPagedFault, p->numOfPagedOut, p->name);
    #endif
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
    #ifdef TRUE
      cprintf(" %d / %d  free page frames in the system /n", currFreePages(), totalFreePages()); 
    #endif
  }
}
