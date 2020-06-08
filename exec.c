#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"

#define MAYACODE 1

#if !MAYACODE
int
exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint argc, sz, sp, ustack[3+MAXARG+1];
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pde_t *pgdir, *oldpgdir;
  struct proc *curproc = myproc();

  begin_op();

  if((ip = namei(path)) == 0){
    end_op();
    cprintf("exec: fail\n");
    return -1;
  }
  ilock(ip);
  pgdir = 0;

  // Check ELF header
  if(readi(ip, (char*)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;

  if((pgdir = setupkvm()) == 0)
    goto bad;

  // Load program into memory.
  sz = 0;
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if(1) cprintf("exec.c: Load program into memory: PID %d about to sz = allocuvm ,  current pages in memory=%d\n",curproc->pid,curproc->pagesInMemory);
    if((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    if(loaduvm(pgdir, (char*)ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
  sz = PGROUNDUP(sz);
  if(1) cprintf("exec.c: Allocate two pages at the next page boundary: PID %d about to sz = allocuvm ,  current pages in memory=%d\n",curproc->pid,curproc->pagesInMemory);

  if((sz = allocuvm(pgdir, sz, sz + 2*PGSIZE)) == 0)
    goto bad;
  clearpteu(pgdir, (char*)(sz - 2*PGSIZE));
  sp = sz;

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
    if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[3+argc] = sp;
  }
  ustack[3+argc] = 0;

  ustack[0] = 0xffffffff;  // fake return PC
  ustack[1] = argc;
  ustack[2] = sp - (argc+1)*4;  // argv pointer

  sp -= (3+argc+1) * 4;
  if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0)
    goto bad;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(curproc->name, last, sizeof(curproc->name));

  // Commit to the user image.
  oldpgdir = curproc->pgdir;
  curproc->pgdir = pgdir;
  curproc->sz = sz;
  curproc->tf->eip = elf.entry;  // main
  curproc->tf->esp = sp;
  switchuvm(curproc);
  freevm(oldpgdir);
  return 0;

 bad:
  if(pgdir)
    freevm(pgdir);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}
#endif

#if MAYACODE
int
exec(char *path, char **argv)
{
  int NONEisDefined =0;
  int pInMemory= 0;
  int pInSwap= 0;
  int headPM= 0;
  int tailPM = 0;
  struct memoryPages_e MP[MAX_PSYC_PAGES];  
  struct swap_e SP[17]; 
  #ifdef NONE
    NONEisDefined = 1;
  #endif

  char *s, *last;
  int i, off;
  uint argc, sz, sp, ustack[3+MAXARG+1];
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pde_t *pgdir, *oldpgdir;
  struct proc *curproc = myproc();
  

  begin_op();

  if((ip = namei(path)) == 0){
    end_op();
    cprintf("exec: fail\n");
    return -1;
  }
  ilock(ip);
  pgdir = 0;

  // Check ELF header
  if(readi(ip, (char*)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;

  if((pgdir = setupkvm()) == 0)
    goto bad;

  //backup the fields
  //if we have a policy such as AQ/ NFUA/ LAPA
  if(!NONEisDefined){  
    pInMemory= curproc->pagesInMemory;
    pInSwap= curproc->pagesInSwap;
    headPM= curproc->headOfMemoryPages;
    tailPM= curproc->tailOfMemoryPages;
 
    for (int i = 0; i < MAX_PSYC_PAGES; i++){
        SP[i].is_occupied=curproc->swapPages[i].is_occupied;
        curproc->swapPages[i].is_occupied=0;
        SP[i].va=curproc->swapPages[i].va;
        curproc->swapPages[i].va=0; 
        MP[i].va=curproc->memoryPages[i].va;
        curproc->memoryPages[i].va=0; //TODO: in case of lapa should be 0xffffffff;
        MP[i].prev=curproc->memoryPages[i].prev;
        curproc->memoryPages[i].prev=-1;
        MP[i].next=curproc->memoryPages[i].next;
        curproc->memoryPages[i].next=-1;
        MP[i].age=curproc->memoryPages[i].age;
        curproc->memoryPages[i].age=0;
        if(i==MAX_PSYC_PAGES-1){
          SP[i+1].is_occupied=curproc->swapPages[i].is_occupied;
          curproc->swapPages[i].is_occupied=0;
          SP[i+1].va=curproc->swapPages[i].va; 
          curproc->swapPages[i].va=0; 
        }
      }       
      curproc->pagesInMemory=0;
      curproc->pagesInSwap=0;
      curproc->headOfMemoryPages=-1;
      curproc->tailOfMemoryPages=-1;
  }

  // Load program into memory.
  sz = 0;
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if(1) cprintf("exec.c: Load program into memory: PID %d about to sz = allocuvm ,  current pages in memory=%d\n",curproc->pid,curproc->pagesInMemory);
    if((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    if(loaduvm(pgdir, (char*)ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
  sz = PGROUNDUP(sz);
if(1) cprintf("exec.c: Allocate two pages at the next page boundary: PID %d about to sz = allocuvm ,  current pages in memory=%d\n",curproc->pid,curproc->pagesInMemory);

  if((sz = allocuvm(pgdir, sz, sz + 2*PGSIZE)) == 0)
    goto bad;
  clearpteu(pgdir, (char*)(sz - 2*PGSIZE));
  sp = sz;

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
    if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[3+argc] = sp;
  }
  ustack[3+argc] = 0;

  ustack[0] = 0xffffffff;  // fake return PC
  ustack[1] = argc;
  ustack[2] = sp - (argc+1)*4;  // argv pointer

  sp -= (3+argc+1) * 4;
  if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0)
    goto bad;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(curproc->name, last, sizeof(curproc->name));

  //resetting the swap file 
  if(!NONEisDefined){
    if(!(curproc->pid==1 || curproc->parent->pid==1)){
      removeSwapFile(curproc);
      createSwapFile(curproc);
    }

  }


  // Commit to the user image.
  oldpgdir = curproc->pgdir;
  curproc->pgdir = pgdir;
  curproc->sz = sz;
  curproc->tf->eip = elf.entry;  // main
  curproc->tf->esp = sp;
  switchuvm(curproc);
  freevm(oldpgdir);
  return 0;

 bad:
  if(pgdir)
    freevm(pgdir);
  if(ip){
    iunlockput(ip);
    end_op();
  }

  if(!NONEisDefined){
    curproc->headOfMemoryPages=headPM;
    curproc->tailOfMemoryPages=tailPM;
    curproc->pagesInSwap=pInSwap;
    curproc->pagesInMemory=pInMemory;
    for (int i = 0; i < MAX_PSYC_PAGES; i++)
      {
        
        curproc->swapPages[i].is_occupied=SP[i].is_occupied;
        curproc->swapPages[i].va=SP[i].va;
        curproc->memoryPages[i].va=MP[i].va;
        curproc->memoryPages[i].prev=MP[i].prev;
        curproc->memoryPages[i].next=MP[i].next;
        curproc->memoryPages[i].age=MP[i].age;
        
        if(i==MAX_PSYC_PAGES-1){
          curproc->swapPages[i].is_occupied=SP[i+1].is_occupied;
          curproc->swapPages[i].va=SP[i+1].va; 
          
        }
      }       
  }
  
  return -1;
}

#endif