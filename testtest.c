#include "types.h"
#include "stat.h"
#include "user.h"


// A program used to test task 3
int
main(int argc, char *argv[])
{
    int status;
    if (fork() == 0)
        exit(5);
    else
        wait(&status);
    if (status == 5)
        printf(1, "Success!\n");
    else
        printf(1, "Fail :(\n");
    printf(1, "Child exit status is %d\n", status);
    exit(0);
}