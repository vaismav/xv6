#include "types.h"
#include "stat.h"
#include "user.h"


void addPages(int pages){
    int i;
    printf(1,"testone.c:  start test: add %d pages\n",pages);
    char* buf = sbrk(4096*pages);
    printf(1,"testone.c:  addPages: created buf = sbrk(4096*pages), pages = %d\n",pages);
    for(i=0; i<pages;i++){
        printf(1,"testone.c: addPages: putting 'x' at address 0x%x\n",&buf[i*4096]);
        buf[i*4096] = 'x';
    }

    for(i=0; i<pages;i++){
        printf(1, " round 1 data at address 0x%x : %c\n",&buf[i*4096], buf[i*4096]);
    }
    
    for(i=0; i<pages;i++){
        printf(1, " round 2 data: %c\n",buf[i*4096]);
    }

}

int
main(){

    // #if SELECTION == SCFIFO
    //     printf(1,"testone.c: SELLECTION == SCFIFO\n");
    // #endif

    // #if SELECTION == LAPA
    //     printf(1,"testone.c: SELECTION == LAPA\n");
    // #endif

    // #if SELECTION == NONE
    //     printf(1,"testone.c: SELECTION == NONE\n");
    // #endif

    // #ifndef SELECTION
    //     printf(1,"testone.c: #ifndef SELECTION\n");
    // #endif

    #ifndef NONE
        printf(1,"testone.c: #ifndef NONE\n");
    #endif

    #ifdef SCFIFO
        printf(1,"testone.c: #ifdef SCFIFO\n");
    #endif

    #ifdef LAPA
        printf(1,"testone.c: #ifdef LAPA\n");
    #endif

    #ifdef AQ
        printf(1,"testone.c: #ifdef AQ\n");
    #endif

    #ifdef NONE
        printf(1,"testone.c: #ifdef NONE\n");
    #endif

    
    printf(1,"testone.c: strating memory pages test...\n");
    
    addPages(29);
    // addPages(4);
    printf(1,"#########################\n@@@@@@@@@@@@@@@@@@@@@\nSUCCESSFULY FINISHED TESTONE!!!!!!!!!!!!!!!!!!!!!!!!!\n@@@@@@@@@@@@@@@@@@@@@\n########################\n");
    exit();
    return 0;
}