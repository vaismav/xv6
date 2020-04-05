#include "types.h"
#include "stat.h"
#include "user.h"

int main(void)
{
	char *str;
	int s=memsize();
	printf(1, "The process is using: %d B\n", s);
	str=(char *)malloc(2000); //alocating 2KB
	printf(1, "Allocating more memory\n");
	s=memsize();
	printf(1, "The process is using: %d B\n", s);
	free(str);
	printf(1, "Freeing memory\n");
	s=memsize();
	printf(1, "The process is using: %d B\n", s);

    exit(0);
}