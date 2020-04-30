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

int testUtilites(void){
    struct proc parentProc=*(myproc());
    cprintf("TEST: uint sigprocmask(uint): \n");
    if (fork()==0){
       struct proc childProc=*(myproc());
       uint old_mask=childProc.signal_Mask;
       uint new_mask = 0;
       new_mask |=1U<<3;
       if(sigprocmask(new_mask)!=old_mask){
            cprintf("TEST: uint sigprocmask(uint) doesnt return old mask \n");
            exit();
       }
       if(new_mask!=childProc.signal_Mask){
        cprintf("TEST: uint sigprocmask(uint) doesnt update mask\n");
        exit();
       }
       cprintf("TEST: uint sigprocmask(uint): PASSED \n");
    } 
    wait();

    cprintf("TEST: uint sigaction(): \n");
    if(fork()==0){
        struct proc childProc=*(myproc());
        //init sigaction *act
        struct sigaction *act;
        act->sa_handler=childProc.signal_Handlers[SIGCONT]; //?
        act->sigmask=childProc.siganl_handlers_mask[SIGCONT]; //sigcont is down
        //init sigaction *oldact
        struct sigaction *oldact;
        oldact->sa_handler=childProc.signal_Handlers[SIGCONT]; //?
        oldact->sigmask=childProc.siganl_handlers_mask[SIGCONT]; //sigcont is down

        //TEST: should not chage SIGKILL SIGSTOP
        if((sigaction(SIGKILL,act,null)==0)||(sigaction(SIGSTOP,act,null)==0)){
            cprintf("TEST: uint sigaction(): changes SIGKILL SIGSTOP\n");
            exit(); //needed?
        }
        //change childProc mask in SIGCONT
        uint new_mask = 0;
        new_mask |=1U<<19; //sigcont is up
        sigprocmask(new_mask);
        //TEST: oldact null
        if(sigaction(SIGCONT,act,null)!=0){
            cprintf("TEST: uint sigaction(): faild to set process fields from act\n");
                exit();
        }
        if(childProc.signal_Handlers[SIGCONT]!=act->sa_handler){
              cprintf("TEST: uint sigaction(): not changing process signal handlers\n");
              exit();
        }
        if(childProc.siganl_handlers_mask[SIGCONT]!= act->sigmask){ //sigcont should be down
            cprintf("TEST: uint sigaction(): not changing process signal handlers mask\n");
            exit();
        }
        //change childProc mask in SIGCONT 
        uint new_mask = 0;
        new_mask |=1U<<19; //sigcont is up
        sigprocmask(new_mask);
        //TEST: oldact not null
        if(sigaction(SIGCONT,act,oldact)!=0){
            cprintf("TEST: uint sigaction(): faild to set oldact fields\n");
            exit();
        }
        if(oldact->sa_handler != childProc.signal_Handlers[SIGCONT]){
            cprintf("TEST: uint sigaction(): not changing oldact signal handlers\n");
            exit();
        }
         if(oldact->sigmask!=childProc.siganl_handlers_mask[SIGCONT]){ //sigcont should be down
            cprintf("TEST: uint sigaction(): not changing oldact sigmask\n");
            exit();
         }
         cprintf("TEST: uint sigaction(): PASSED \n");
    } wait();
    
     cprintf("TEST: uint sigret(): \n"); //TODO: how to check
     if(fork()==0){
         struct proc childProc=*(myproc());


     } wait();

      cprintf("TEST: uint kill(int,int): \n"); 
     if(fork()==0){
         struct proc childProc=*(myproc());
         kill(0,SIGKILL);
         if((childProc.pending_Signals<<SIGKILL)==0){
             cprintf("TEST: uint kill(int,int): didnt get the SIGKILL \n");
             exit();
         }
         cprintf("TEST: uint kill(int,int): PASSED \n");
     } wait();
    return 0;
}


int main(int argc, char *argv[]){
    struct proc parentProc;
    struct proc childProc;
    parentProc=*(myproc());
    
    if(fork()==0){
        testUtilites();
        exit();  
    }
    else while(wait()!=-1){}
        
   
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