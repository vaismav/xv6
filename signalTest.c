#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

int fibo(int n){
  if(n<=2 )
  return 1;
  return fibo(n-2) + fibo(n-1);
}


/** test state of new proc
 * checs if all of proc fields are initialized correctly
 * pre-condition: xv6 is running
 * test action: create new procees;
 * requsted result:
 * p->pending_Signals= 0;
 * p->signal_Mask= parentProc->signal_Mask;
 * for each sigHand in p->signal_Handlers=parent->signal_Handlers;
 */

int testStateOfNewProc(struct proc* parentProc, struct proc* childProc){ //checking new proc 
        if(childProc->pending_Signals!=0){
         fprintf(1,"new child p's pending signal not default");
            return -1;
        } 
        if(childProc->signal_Mask!=parentProc->signal_Mask){
               fprintf(1,"new child p's signal_mask not as parent's");
               return -1;
        }
        
        for(int i=0; i<32; i++){
            if(childProc->signal_Handlers[i]!=parentProc->signal_Handlers[i]){
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
 void handler(int signum){ //handler to change to 
        fprintf(1,"Hello\n");
    }

int testUtilites(void){
    struct proc parentProc=*(myproc());
    fprintf(1,"TEST: uint sigprocmask(uint): \n");
    if (fork()==0){
       struct proc childProc=*(myproc());
       uint old_mask=childProc.signal_Mask;
       uint new_mask = 0;
       new_mask |=1U<<3;
       if(sigprocmask(new_mask)!=old_mask){
            fprintf(1,"TEST: uint sigprocmask(uint) doesnt return old mask \n");
            exit();
       }
       if(new_mask!=childProc.signal_Mask){
        fprintf(1,"TEST: uint sigprocmask(uint) doesnt update mask\n");
        exit();
       }
       fprintf(1,"TEST: uint sigprocmask(uint): PASSED \n");
       exit();
    } 
    wait();

    fprintf(1,"TEST: uint sigaction(): \n");
    if(fork()==0){
   
        struct proc *childProc=myproc();
        //init sigaction act
        struct sigaction act;
        memset (&act, 0, sizeof (act)); 
        act.sa_handler=&handler;
        act.sigmask=childProc->siganl_handlers_mask[SIGCONT]; //sigcont is down
        //init sigaction oldact
        struct sigaction oldact;
        memset (&oldact, 0, sizeof (oldact));
        oldact.sa_handler=&handler;
        oldact.sigmask=childProc->siganl_handlers_mask[SIGCONT]; //sigcont is down

        //TEST: try to chage SIGKILL SIGSTOP, should fail
        if((sigaction(SIGKILL,&act,null)==0)||(sigaction(SIGSTOP,&act,null)==0)){
            fprintf(1,"TEST: uint sigaction(): changes SIGKILL SIGSTOP\n");
            exit(); 
        }
        //change childProc mask in SIGCONT
        uint new_mask = 0;
        new_mask |=1U<<19; //sigcont is up
        sigprocmask(new_mask);

        //TEST: oldact null
        if(sigaction(SIGCONT,&act,null)!=0){
            fprintf(1,"TEST: uint sigaction(): faild to set process fields from act\n");
                exit();
        }
        if(childProc->signal_Handlers[SIGCONT]!=act.sa_handler){ //should be handler
              fprintf(1,"TEST: uint sigaction(): not changing process signal handlers\n");
              exit();
        }
        if(childProc->siganl_handlers_mask[SIGCONT]!= act.sigmask){ //sigcont should be down
            fprintf(1,"TEST: uint sigaction(): not changing process signal handlers mask\n");
            exit();
        }
        //change childProc mask in SIGCONT 
        uint new_mask = 0;
        new_mask |=1U<<19; //sigcont is up
        sigprocmask(new_mask);
        //TEST: oldact not null
        if(sigaction(SIGCONT,&act,&oldact)!=0){
            fprintf(1,"TEST: uint sigaction(): faild to set oldact fields\n");
            exit();
        }
        if(oldact.sa_handler != childProc->signal_Handlers[SIGCONT]){ //should as child's
            fprintf(1,"TEST: uint sigaction(): not changing oldact signal handlers\n");
            exit();
        }
         if(oldact.sigmask!=childProc->siganl_handlers_mask[SIGCONT]){ //sigcont should be down
            fprintf(1,"TEST: uint sigaction(): not changing oldact sigmask\n");
            exit();
         }
         fprintf(1,"TEST: uint sigaction(): PASSED \n");
         exit();
    } 
    wait();


    fprintf(1,"TEST: uint kill(int,int): \n"); 
     if(fork()==0){
         struct proc *childProc=myproc();
         kill(0,SIGKILL);
         if(!(childProc->killed)){
             fprintf(1,"TEST: uint kill(int,int): didnt get the SIGKILL \n");
             exit();
         }
        fprintf(1,"TEST: uint kill(int,int): PASSED \n");
        exit();
     } 
     wait();
    return 0;
}



void resetInitProc(void){
    struct sigaction backup_sigact;
    backup_sigact.sa_handler=SIG_DFL;
    backup_sigact.sigmask=DFL_SIGMASK;
    sigprocmask(DFL_SIGMASK);
    for(int i=0; i<32; i++){
        sigaction(i,&backup_sigact,null);
    }
}

void check_exec(){
    struct proc curr_procces=*(myproc());
    for(int i=2; i<32; i++){            
        if(i==0 || i==1){
            if(curr_procces.signal_Handlers[i]!=&handler)
                fprintr(1,"exec didnt changed SIG_IGN | SIG_DFL");
                exit();
            }
        if(curr_procces.signal_Handlers[i]!=SIG_DFL){
            fprintr(1,"exec didnt change handlers to SIG_DFL");
            exit();
        }
        fprintr(1,"TEST 3: PASSED");
        exit();
   }
}




int main(int argc, char *argv[]){
    if(argc>1){
        if(argv[1]=="exec"){
            check_exec();
        }
    }
    struct proc parentProc;
    struct proc childProc;
    int i,cpid1,cpid2,finishedFirst,stopTheLoop;

    parentProc=*(myproc());
    resetInitProc();
    if(fork()==0){
        testUtilites();
        exit();  
    }
    else while(wait()!=-1){}
        
   
   fprintf(1,"TEST 1: create new proc from default proc settings\n");
    if(fork()==0){
        childProc=*(myproc());
        if(testStateOfNewProc(&parentProc,&childProc)!=0){ //basic
            fprintf(1,"proc fields are NOT initialized correctly after fork ");
            return -1;
        } 
        fprintf(1,"TEST 1 PASSED\n");
        exit();
    }
    wait();
    fprintf(1,"TEST 2: create new proc with custom signal mask and custom signal handlers\n");
    //chenging the parentProc
    
    struct sigaction sig_act;
    memset (&sig_act, 0, sizeof (sig_act));
    sig_act.sa_handler=&handler;
    sig_act.sigmask=DFL_SIGMASK;
    uint new_mask = 0;
    for(int i=1; i<32; i++){ 
        sigaction(i,&sig_act,null);
        if(i!=9)
            new_mask |=1U<<i; //sigcont is up
    }
    sigprocmask(new_mask);
    
    if(fork()==0){
        childProc=*(myproc());
        if(testStateOfNewProc(&parentProc,&childProc)!=0){ //basic
            fprintf(1,"proc fields are NOT initialized correctly after fork ");
            return -1;
        } 
        fprintf(1,"TEST 2 PASSED\n");
        fprintf(1,"TEST 3: create new proc with custom signal mask and custom signal handlers\n");
        //call exec "siganlTest --exec"
        char* arg ='exec';
        exec("signalTest",&arg);
     
    }
     wait();


    resetInitProc();
    fprintf(1,"TEST 4: check SIGSTOP and SIGCONT behavor\n");
    stopTheLoop=0;
    for(i=0; i<10 & !stopTheLoop;i++){
        cpid1=fork();
        if(cpid1==0){
            fibo(25);
            exit();
        }
        //PROC2 will stop PROC1
        if(kill(cpid1,SIGSTOP)!=0){
            //main porcess kill cpid1 and start new test
            cprintf("TEST 4: run N0 %d: couldnt sent SIGSTOP to cpid1\n",i);
            //start next step of the for loop
            continue; 
        }
        else{
            //SIGSTOP sent succussfuly
            //strat running of PROC2
            cpid2=fork();
            if(cpid2==0){
                fibo(37);
                if(kill(cpid1,SIGCONT)!=0){
                    //failed to send SIGCONT
                    fprintf(1,"TEST 4: couldnt sent SIGCONT to cpid1\n");
                }
                exit();                
            }
        }
        //main process checks who finished first 
        finishedFirst=wait();
        wait();
        //check if one of the 2 fork action has failed
        if(cpid1 == -1 || cpid2 ==-1){
            stopTheLoop=1;
            fprintf(1,"TEST 4: wasnt able to do fork()\n"); 
        }
        //check if cpid2 finished before cpid1
        else if(finishedFirst==cpid2){
            cprintf("TEST 4: run N0 %d PASSED (cpid1 finished after cpid2\n",i);
        }
        //FAILD: cpid1 finished before cpid2 (means cpid2 didnt stop cpid1)
        else if(finishedFirst==cpid1){
            
            fprintf(1,"TEST 4: cpid1 finished before cpid2\n"); 
            stopTheLoop=1;
        }
        //unpredictable pid 
        else{ 
            cprintf("TEST 4: PANIC: something accured and PID of finishedFirst=%d (cpid1=%d, cpid2=%d)\n",finishedFirst,cpid1,cpid2);
        }  
    }
    //if error accured in the test
    if(stopTheLoop){
        fprintf(1,"TEST 4: FAILED\n");
        exit();
    }
    //if passed TEST 4
    else{
        fprintf(1,"TEST 4: PASSED\n");
    }
    
    
    //TEST 3: handler & sigret

    return 0;
}