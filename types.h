typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;
typedef uint pde_t;


//NEW
#define MAX_TOTAL_PAGES 32
#define MAX_PSYC_PAGES 16   // pages in the physical memory
#define MAX_SWAP_PAGES 17   // pages in the physical memory
#define AL_PAGES 32  // pages of process
#define OCCUPIED 1
#define UNOCCUPIED 0

#if 0
os202@os202-lubuntu:~/os202/ass3/xv6$ make qemu

1+0 records out
512 bytes copied, 0.000652819 s, 784 kB/s
dd if=kernel of=xv6.img seek=1 conv=notrunc
392+1 records in
392+1 records out
200820 bytes (201 kB, 196 KiB) copied, 0.00118924 s, 169 MB/s
qemu-system-i386 -serial mon:stdio -drive file=fs.img,index=1,media=disk,format=raw -drive file=xv6.img,index=0,media=disk,format=raw -smp 2 -m 512 
xv6...
cpu1: starting 1
proc.c: allocproc: PID 1 start initializing proc  current pages in memory=0
cpu0: starting 0
sb: size 1000 nblocks 941 ninodes 200 nlog 30 logstart 2 inodestart 32 bmap start 58
exec.c: Load program into memory: PID 1 about to sz = allocuvm ,  current pages in memory=0
vm.c: allocuvm: PID 1 added page(0x0) to memory, pages in memory=1
exec.c: Allocate two pages at the next page boundary: PID 1 about to sz = allocuvm ,  current pages in memory=1
vm.c: allocuvm: PID 1 added page(0x1000) to memory, pages in memory=2
vm.c: allocuvm: PID 1 added page(0x2000) to memory, pages in memory=3
vm.c: freevm: PID 1 about to deallocuvm 
vm.c: deallocuvm: PID 1 removed page(0x0) from memory, pages in memory=2
init: starting sh
proc.c: allocproc: PID 2 start initializing proc  current pages in memory=0
proc.c: fork: PID 2 allocated process, pages in memory=0
proc.c: fork: PID 2 copyied process state from proc, pages in memory=0
proc.c: fork: PID 2 before start of copy pages data, pages in memory=0
proc.c: fork: PID 2 parent id = 1
proc.c: fork: PID 2 duplicated process, pages in memory=2
exec.c: Load program into memory: PID 2 about to sz = allocuvm ,  current pages in memory=0
vm.c: allocuvm: PID 2 added page(0x0) to memory, pages in memory=1
vm.c: allocuvm: PID 2 added page(0x1000) to memory, pages in memory=2
exec.c: Allocate two pages at the next page boundary: PID 2 about to sz = allocuvm ,  current pages in memory=2
vm.c: allocuvm: PID 2 added page(0x2000) to memory, pages in memory=3
vm.c: allocuvm: PID 2 added page(0x3000) to memory, pages in memory=4
vm.c: freevm: PID 2 about to deallocuvm 
vm.c: deallocuvm: PID 2 removed page(0x0) from memory, pages in memory=3
vm.c: deallocuvm: PID 2 removed page(0x1000) from memory, pages in memory=2
vm.c: deallocuvm: PID 2 removed page(0x2000) from memory, pages in memory=1
$ echo hello
proc.c: allocproc: PID 3 start initializing proc  current pages in memory=0
proc.c: allocproc: PID 3 about to initialize p->pagesInMemory=0 ,  current pages in memory=0
proc.c: fork: PID 3 allocated process, pages in memory=0
vm.c: freevm: PID 1 about to deallocuvm 
vm.c: deallocuvm: PID 1 removed page(0x0) from memory, pages in memory=1
vm.c: deallocuvm: PID 1 removed page(0x1000) from memory, pages in memory=0
vm.c: deallocuvm: PID 1 removed page(0x2000) from memory, pages in memory=-1
vm.c: deallocuvm: PID 1 removed page(0x3000) from memory, pages in memory=-2
init: starting sh
proc.c: allocproc: PID 4 start initializing proc  current pages in memory=1
proc.c: allocproc: PID 4 about to initialize p->pagesInMemory=0 ,  current pages in memory=1
proc.c: fork: PID 4 allocated process, pages in memory=0
proc.c: fork: PID 4 copyied process state from proc, pages in memory=0
proc.c: fork: PID 4 before start of copy pages data, pages in memory=0
proc.c: fork: PID 4 parent id = 1
proc.c: fork: PID 4 duplicated process, pages in memory=-2
exec.c: Load program into memory: PID 4 about to sz = allocuvm ,  current pages in memory=0
vm.c: allocuvm: PID 4 enter allocuvm
vm.c: allocuvm: PID 4 added page(0x0) to memory, pages in memory=1
vm.c: allocuvm: PID 4 added page(0x1000) to memory, pages in memory=2
exec.c: Allocate two pages at the next page boundary: PID 4 about to sz = allocuvm ,  current pages in memory=2
vm.c: allocuvm: PID 4 enter allocuvm
vm.c: allocuvm: PID 4 added page(0x2000) to memory, pages in memory=3
vm.c: allocuvm: PID 4 added page(0x3000) to memory, pages in memory=4
vm.c: freevm: PID 4 about to deallocuvm 
vm.c: removePageFromMemory: PID 4 removed page(0x0) from memory, pages in memory=3
vm.c: removePageFromMemory: PID 4 removed page(0x1000) from memory, pages in memory=2
vm.c: removePageFromMemory: PID 4 removed page(0x2000) from memory, pages in memory=1
#endif

//pagefault
#if 0
gcc -fno-pic -static -fno-builtin -fno-strict-aliasing -O2 -Wall -MD -ggdb -m32 -Werror -fno-omit-frame-pointer -fno-stack-protector -fno-pie -no-pie   -c -o vm.o vm.c
ld -m    elf_i386 -T kernel.ld -o kernel entry.o bio.o console.o exec.o file.o fs.o ide.o ioapic.o kalloc.o kbd.o lapic.o log.o main.o mp.o picirq.o pipe.o proc.o sleeplock.o spinlock.o string.o swtch.o syscall.o sysfile.o sysproc.o trapasm.o trap.o uart.o vectors.o vm.o  -b binary initcode entryother
objdump -S kernel > kernel.asm
objdump -t kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$/d' > kernel.sym
dd if=/dev/zero of=xv6.img count=10000
10000+0 records in
10000+0 records out
5120000 bytes (5.1 MB, 4.9 MiB) copied, 0.0419991 s, 122 MB/s
dd if=bootblock of=xv6.img conv=notrunc
1+0 records in
1+0 records out
512 bytes copied, 0.000126023 s, 4.1 MB/s
dd if=kernel of=xv6.img seek=1 conv=notrunc
392+1 records in
392+1 records out
200844 bytes (201 kB, 196 KiB) copied, 0.00294486 s, 68.2 MB/s
qemu-system-i386 -serial mon:stdio -drive file=fs.img,index=1,media=disk,format=raw -drive file=xv6.img,index=0,media=disk,format=raw -smp 2 -m 512 
xv6...
cpu1: starting 1
proc.c: allocproc: PID 1 start initializing proc  current pages in memory=0
cpu0: starting 0
sb: size 1000 nblocks 941 ninodes 200 nlog 30 logstart 2 inodestart 32 bmap start 58
exec.c: Load program into memory: PID 1 about to sz = allocuvm ,  current pages in memory=0
vm.c: allocuvm: PID 1 added page(0x0) to memory, pages in memory=1
exec.c: Allocate two pages at the next page boundary: PID 1 about to sz = allocuvm ,  current pages in memory=1
vm.c: allocuvm: PID 1 added page(0x1000) to memory, pages in memory=2
vm.c: allocuvm: PID 1 added page(0x2000) to memory, pages in memory=3
vm.c: freevm: PID 1 about to deallocuvm 
vm.c: deallocuvm: PID 1 enter deallocuvm, pages in memory=3
vm.c: deallocuvm: PID 1 removed page(0x0) from memory, pages in memory=2
init: starting sh
proc.c: allocproc: PID 2 start initializing proc  current pages in memory=0
proc.c: fork: PID 2 allocated process, pages in memory=0
proc.c: fork: PID 2 copyied process state from proc, pages in memory=0
proc.c: fork: PID 2 before start of copy pages data, pages in memory=0
proc.c: fork: PID 2 parent id = 1
proc.c: fork: PID 2 duplicated process, pages in memory=2
trap.c: trap: PID 1: T_PGFLT
 trap.c: trap: PID 1: T_PGFLT
 lapicid 0: panic: acquire
#endif