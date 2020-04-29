#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"


/** test state of new proc
 * checs if all of proc fields are initialized correctly
 * pre-condition: xv6 is running
 * test action: create new procees;
 * requsted result:
 * p->pending_Signals= 0;
 * p->signal_Mask= parentProc->signal_Mask;
 * for each sigHand in p->signal_Handlers: sigHand=SIG_DFL;
 */

int testStateOfNewProc(struct proc* parentProc, struct proc* childProc){ //checking new proc 
        if(childProc->pending_Signals!=0){
         cprintf("new child p's pending signal not default");
            return -1;
        } 
        if(childProc->signal_Mask!=parentProc->signal_Mask){
               cprintf("new child p's signal_mask not as parent's");
               return -1;
        }
        
        for(int i=0; i<32; i++){
            if(childProc->signal_Handlers[i]!=SIG_DFL){
                cprintf("new child p's %d handler not default", i);
                return -1;
            }
        }
        return 0;
}

 /** exec
 * pr-condition: test state of new proc successeded
 * test action: exec
 * requested result: all handlres are default except DFL & IGN
 */

/** test sigaction
 * pre-condition: test state of new proc successeded
 * test action: sigaction with oldact= Null | oldact!=Null, try to modify SIGKILL & SIGSTOP.
 * requested result: sucess= 0, error = -1.
 *                   oldact=Null-> old->sa_handle= p->signal_Handlers[signum]; oldact->sigmask=p->siganl_handlers_mask[signum];
 *                   oldact!=NUll -> p->signal_Handlers[signum]=act->sa_handler; p->siganl_handlers_mask[signum] = act->sigmask;
 *                   SIGKILL & SIGSTOP cant be modified
 */

/** test user handle signal (signalHandle(not def) & sigret)
 * pre-condition:  test state of new proc successeded 
 * test-action: create struct trapframe*, give signals. sigret: check if tf has been restored at the end of it (?) 
 *              SIG_STOP and then SIGCONT, just SIGCONT
 * requested action: tf has been restored at the end
 *                   SIGSTOP - process will not be RUNNING, SIGCONT after SIGSTOP get him to continue, just SIGCONT will be reset.
 *                   and the opropriate signal will be turn on. 
 */

/** test kill
 * pre-conditions: test state of new proc successeded
 * test-action: kill process
 * requested action: process will have pending kill signal
 */




int main(int argc, char *argv[]){
    struct proc parentProc;
    struct proc childProc;
    parentProc=*(myproc());
    

    testUtilites();
   
   cprintf("TEST 1: create new proc from default proc settings\n");
    if(fork()==0){
        childProc=*(myproc());
        if(testStateOfNewProc(&parentProc,&childProc)!=0){ //basic
            cprintf("proc fields are NOT initialized correctly after fork ");
            return -1;
        } 
        cprintf("TEST 1 PASSED\n");
        exit();
    }
    wait();
    cprintf("TEST 2: create new proc with custom signal mask and custom signal handlers\n");

    
    

        
    
    //TEST 2: create new proc with custom signal mask and custom signal handlers

    //TEST 3: sigaction & sigret 

    return 0;
}