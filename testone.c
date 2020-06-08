#include "types.h"
#include "stat.h"
#include "user.h"

void addPages(int pages){
    printf(1,"testone.c: add % pages\n",pages);
    char* buf = malloc(4096*pages);
    for(int i=0; i<pages;i++){
        buf[i*4096] = 'x';
    }

    for(int i=0; i<pages;i++){
        printf(1, "data: %c\n",buf[i*4096]);
    }

}

int
main(){
    printf(1,"testone.c: strating memory pages test...\n");
    
    addPages(4);
    addPages(4);
    addPages(4);
    addPages(4);
    return 0;
}