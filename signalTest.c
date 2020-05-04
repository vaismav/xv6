#include "types.h"
#include "stat.h"
#include "user.h"




int fibo(int n){
  if(n<=2 )
  return 1;
  return fibo(n-2) + fibo(n-1);
}


/** test state of new proc
 * checs if all of procSignalsData fields are initialized correctly
 * pre-condition: xv6 is running
 * test action: create new procees;
 * requsted result:
 * p->pending_Signals= 0;
 * p->signal_Mask= parentProc->signal_Mask;
 * for each sigHand in p->signal_Handlers=parent->signal_Handlers;
 */

int testStateOfNewProc(struct procSignalsData* parentProc, struct procSignalsData* childProc){ //checking new procSignalsData 
        if(childProc->pending_Signals!=0){
         printf(1,"new child p's pending signal not default");
            return -1;
        } 
        if(childProc->signal_Mask!=parentProc->signal_Mask){
               printf(1,"new child p's signal_mask not as parent's");
               return -1;
        }
        
        for(int i=0; i<32; i++){
            if(childProc->signal_Handlers[i]!=parentProc->signal_Handlers[i]){
                printf(1,"new child p's %d handler not default", i);
                return -1;
            }
        }
        return 0;
}

 /** exec
 * pr-condition: test state of new procSignalsData successeded
 * test action: exec
 * requested result: all handlres are default except DFL & IGN
 */

/** test sigaction
 * pre-condition: test state of new procSignalsData successeded
 * test action: sigaction with oldact= Null | oldact!=Null, try to modify SIGKILL & SIGSTOP.
 * requested result: sucess= 0, error = -1.
 *                   oldact=Null-> old->sa_handle= p->signal_Handlers[signum]; oldact->sigmask=p->siganl_handlers_mask[signum];
 *                   oldact!=NUll -> p->signal_Handlers[signum]=act->sa_handler; p->siganl_handlers_mask[signum] = act->sigmask;
 *                   SIGKILL & SIGSTOP cant be modified
 */

/** test user handle signal (signalHandle(not def) & sigret)
 * pre-condition:  test state of new procSignalsData successeded 
 * test-action: create struct trapframe*, give signals. sigret: check if tf has been restored at the end of it (?) 
 *              SIG_STOP and then SIGCONT, just SIGCONT
 * requested action: tf has been restored at the end
 *                   SIGSTOP - process will not be RUNNING, SIGCONT after SIGSTOP get him to continue, just SIGCONT will be reset.
 *                   and the opropriate signal will be turn on. 
 */

/** test kill
 * pre-conditions: test state of new procSignalsData successeded
 * test-action: kill process
 * requested action: process will have pending kill signal
 */
 void handler(int signum){ //handler to change to 
        printf(1,"Hello$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
        exit();
    }

void handler1(int signum){ //handler to change to 
    printf(1,"Hello#####################################################################################################\n");
    kill(getpid(),SIGKILL);
    exit();
}

int testUtilites(void){
    int cpid1,cpid2;
    uint dummy1=0,dummy2=0;
    printf(1,"TEST: uint sigprocmask(uint): \n");
    if (fork()==0){
       struct procSignalsData childProc;
       getProcSignalsData(&childProc);
       uint old_mask=childProc.signal_Mask;
       uint new_mask = 0;
       new_mask |= 1U<<3;
       if(sigprocmask(new_mask)!=old_mask){
            printf(1,"TEST: uint sigprocmask(uint) doesnt return old mask \n");
            exit();
       }
       if(childProc.signal_Mask!=new_mask){
        printf(1,"TEST: uint sigprocmask(uint) doesnt update mask\n");
        exit();
       }
       printf(1,"TEST: uint sigprocmask(uint): PASSED \n");
       exit();
    } 
    wait();

    printf(1,"TEST: uint sigaction(): \n");
    if(fork()==0){
   
        struct procSignalsData childProc;
        getProcSignalsData(&childProc);
        //init sigaction act
        struct sigaction act;
        //memset (&act, 0, sizeof (act)); //TODO:
        act.sa_handler=&handler;
        act.sigmask=4; //sigcont is down
        //init sigaction oldact
        struct sigaction oldact;
        //memset (&oldact, 0, sizeof (oldact)); //TODO:
        oldact.sa_handler=&handler;
        oldact.sigmask=childProc.siganl_handlers_mask[SIGCONT]; //sigcont is down

        //TEST: try to chage SIGKILL SIGSTOP, should fail
        if((sigaction(SIGKILL,&act,null)==0)||(sigaction(SIGSTOP,&act,null)==0)){
            printf(1,"TEST: uint sigaction(): changes SIGKILL SIGSTOP\n");
            exit(); 
        }
        //change childProc mask in SIGCONT
        uint new_mask = 0;
        new_mask |=1U<<19; //sigcont is up
        sigprocmask(new_mask);

        //TEST: oldact null
        if(sigaction(SIGCONT,&act,null)!=0){
            printf(1,"TEST: uint sigaction(): faild to set process fields from act\n");
                exit();
        }
        if(childProc.signal_Handlers[SIGCONT]!=act.sa_handler){ //should be handler
              printf(1,"TEST: uint sigaction(): not changing process signal handlers\n");
              exit();
        }
        if(childProc.siganl_handlers_mask[SIGCONT]!= act.sigmask){ //sigcont should be down
            printf(1,"TEST: uint sigaction(): not changing process signal handlers mask\n");
            exit();
        }
        //change childProc mask in SIGCONT 
        uint new_mask1 = 0;
        new_mask1 |=1U<<19; //sigcont is up
        sigprocmask(new_mask1);
        //TEST: oldact not null
        if(sigaction(SIGCONT,&act,&oldact)!=0){
            printf(1,"TEST: uint sigaction(): faild to set oldact fields\n");
            exit();
        }
        if(oldact.sa_handler != childProc.signal_Handlers[SIGCONT]){ //should as child's
            printf(1,"TEST: uint sigaction(): not changing oldact signal handlers\n");
            exit();
        }
         if(oldact.sigmask!=childProc.siganl_handlers_mask[SIGCONT]){ //sigcont should be down
            printf(1,"TEST: uint sigaction(): not changing oldact sigmask\n");
            exit();
         }
         printf(1,"TEST: uint sigaction(): PASSED \n");
         exit();
    } 
    wait();


    printf(1,"TEST: uint kill(int,int): \n"); 
    
    cpid1=fork();
     if(cpid1==0){
        while(1){
            dummy1 = !dummy1;
        }
        
     } 
     cpid2=fork();
     if(cpid2==0){
         printf(1,"TEST: uint kill(int,int): Sent SIGKILL to PID:%d \n",cpid1);
        kill(cpid1,SIGKILL);
        
        while(1){
            dummy2 = !dummy2;
        }
     }
     int died=wait();
     printf(1,"TEST: uint kill(int,int): PASSED cpid(from wait)=%d  \n",died);   
     kill(cpid2,SIGKILL);
     wait();
     printf(1,"FINISHED UTILITIES TESTS !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! \n"); 
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
    struct procSignalsData curr_procces;
    getProcSignalsData(&curr_procces);
    for(int i=2; i<32; i++){            
        if(i==0 || i==1){
            if(curr_procces.signal_Handlers[i]!=&handler){
                printf(1,"exec didnt changed SIG_IGN | SIG_DFL\n");
                exit();
            }
        }
        if(curr_procces.signal_Handlers[i]!=SIG_DFL){
            printf(1,"exec didnt change handlers to SIG_DFL\n");
            exit();
        }
        printf(1,"TEST 3: PASSED\n");
        exit();
   }
}




int main(int argc, char* argv[]){
    if(argc>1){
        if(argv[1][0]=='e' &&
           argv[1][1]=='x' &&
           argv[1][2]=='e' &&
           argv[1][3]=='c'   ){
            check_exec();
        }
    }
    // struct procSignalsData parentProc;
    // struct procSignalsData childProc;

    int cpid1,cpid2;
    uint dummy1=3;
/*
    getProcSignalsData(&parentProc);

    resetInitProc();
    if(fork()==0){
        testUtilites();
        exit();  
    }
    else while(wait()!=-1){}
     
   
   printf(1,"TEST 1: create new procSignalsData from default procSignalsData settings\n");
    if(fork()==0){
        getProcSignalsData(&childProc);
        if(testStateOfNewProc(&parentProc,&childProc)!=0){ //basic
            printf(1,"proc fields are NOT initialized correctly after fork ");
            return -1;
        } 
        printf(1,"TEST 1 PASSED\n");
        exit();
    }
    wait();
    printf(1,"TEST 2: create new procSignalsData with custom signal mask and custom signal handlers\n");
    //chenging the parentProc
    
    struct sigaction sig_act;
    memset (&sig_act, 0, sizeof (sig_act));
    sig_act.sa_handler=&handler;
    sig_act.sigmask=DFL_SIGMASK;
    uint new_mask2 = 0;
    for(int i=1; i<32; i++){ 
        sigaction(i,&sig_act,null);
        if(i!=9)
            new_mask2 |=1U<<i; //sigcont is up
    }
    sigprocmask(new_mask2);
    
    if(fork()==0){
        getProcSignalsData(&childProc);
        if(testStateOfNewProc(&parentProc,&childProc)!=0){ //basic
            printf(1,"proc fields are NOT initialized correctly after fork ");
            return -1;
        } 
        printf(1,"TEST 2 PASSED\n");
        printf(1,"TEST 3: create new procSignalsData with custom signal mask and custom signal handlers\n");
        //call exec "siganlTest --exec"
        //char* arg ="exec";
        //exec("signalTest",&arg);
        exit();
    }
     wait();
    resetInitProc();
*/
/*
    int i,finishedFirst,stopTheLoop;
    
    printf(1,"TEST 4: check SIGSTOP and SIGCONT behavor\n");
    stopTheLoop=0;
    for(i=0; i<10 && !stopTheLoop;i++){
        cpid1=fork();
        if(cpid1==0){
            fibo(25);
            exit();
        }
        //PROC2 will stop PROC1
        if(kill(cpid1,SIGSTOP)!=0){
            //main porcess kill cpid1 and start new test
            printf(1,"TEST 4: run N0 %d: couldnt sent SIGSTOP to cpid1\n",i);
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
                    printf(1,"TEST 4: couldnt sent SIGCONT to cpid1\n");
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
            printf(1,"TEST 4: wasnt able to do fork()\n"); 
        }
        //check if cpid2 finished before cpid1
        else if(finishedFirst==cpid2){
            printf(1,"TEST 4: run N0 %d PASSED (cpid1 finished after cpid2\n",i);
        }
        //FAILD: cpid1 finished before cpid2 (means cpid2 didnt stop cpid1)
        else if(finishedFirst==cpid1){
            
            printf(1,"TEST 4: cpid1 finished before cpid2\n"); 
            stopTheLoop=1;
        }
        //unpredictable pid 
        else{ 
            printf(1,"TEST 4: PANIC: something accured and PID of finishedFirst=%d (cpid1=%d, cpid2=%d)\n",finishedFirst,cpid1,cpid2);
        }  
    }
    //if error accured in the test
    if(stopTheLoop){
        printf(1,"TEST 4: FAILED\n");
        exit();
    }
    //if passed TEST 4
    else{
        printf(1,"TEST 4: PASSED\n");
    }
    
    */
    //TEST 5: handler & sigret
    printf(1,"TEST 5: check user handler behavor\n");
    cpid1=fork();
    if(cpid1==0){
        //canghe handler of 5
        struct sigaction act;
        act.sa_handler=&handler1;
        act.sigmask=0;
        struct procSignalsData this;
        getProcSignalsData(&this);
        printf(1,"TEST 5: PID:%d sigmask =  %x \n",this.signal_Mask);
        printf(1,"TEST 5: act->sigmask =  %x .  \n",act.sigmask);
        printf(1,"TEST 5: act.sa_handler =  %x .  \n",act.sa_handler);
        
        sigaction(5,&act,null);
        getProcSignalsData(&this);
        printf(1,"TEST 5:post sigaction: PID:%d sigmask =  %x \n",this.signal_Mask);
        cpid1=getpid();
        cpid2 = fork();
        if(cpid2==0){
            printf(1,"TEST 5: sending signal '5' from pid: %d TO pid: %d \n",getpid(),cpid1);
            while(kill(cpid1,5)!=0){
               printf(1,"TEST 5: failed to send kill(cpid1,5) \n",getpid(),cpid1); 
            }
            
            exit();
        }
        while(1){
            dummy1 = !dummy1;
        }
    }
    wait();
    wait();
    return 0;
}