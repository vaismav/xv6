#include "types.h"
#include "stat.h"
#include "user.h"

int 
main(int argc, char *argv[])
{
    int i=(int)argv[0][1]; //the policy number tp change to
    if(i>=52 || i<=48) //default
        printf(1, "Error replacing policy, no such a policy number(%d)\n",i); 
    else if(i==49){ //1
        policy(1);
        printf(1, "Policy has been successfully changed to Default Policy\n");
    }
    else if(i==50){ //2
        policy(2);
        printf(1, "Policy has been successfully changed to Priority Policy\n");
    }
    else if(i==51){ //3
        policy(3);
        printf(1, "Policy has been successfully changed to CFS Policy\n");
    }

  exit(0);
}