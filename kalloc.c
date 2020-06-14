// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

#define AVISHAIISCRAZY 1

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld
int tFreePages=0;
int freePagesCounter=0;

struct run {
  struct run *next;
  int refrences;
};

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
  struct run ref[MAXPAGES]; //num of refrences for each page
} kmem;


//manage total free pages count - OURS for part 4
int
totalFreePages(void){
  return tFreePages;
}

int 
currFreePages(void){
  return freePagesCounter;
}

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  freerange(vstart, vend);
  //update totalFreePages MAYAs changges
  tFreePages+= (PGROUNDDOWN((uint)vend) - PGROUNDUP((uint)vstart))/PGSIZE; 
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
  //update totalFreePages
  tFreePages+= (PGROUNDDOWN((uint)vend) - PGROUNDUP((uint)vstart))/PGSIZE;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree(p);
}

//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;
#if AVISHAIISCRAZY
  //decrese the ref counter
  if(kmem.use_lock)
    acquire(&kmem.lock);
  r=&kmem.ref[V2P(v)/PGSIZE]; //r is the struct of the page
  r->refrences--;
  if(kmem.use_lock)
    release(&kmem.lock);
  if((1 &&  DEBUG) && KDEBUG ) cprintf("kalloc.c: kfree: decreased pa 0x%x, currnet ref number = %d\n",V2P(v),r->refrences);
  if(r->refrences <=0){
#endif

  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  if(kmem.use_lock)
    acquire(&kmem.lock);
  // r=&kmem.ref[V2P(v)/PGSIZE]; //r is the refrences for v page
  r = (struct run*)v;
  r->next = kmem.freelist;
  r->refrences=0; //init the refrence count for this page
  kmem.freelist = r;
  //update counter
  freePagesCounter++; 
  if(kmem.use_lock)
    release(&kmem.lock);
#if AVISHAIISCRAZY
  }
#endif
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;
  if(r){
    kmem.freelist = r->next;
     r->refrences=1; //allocated page has 1 refrence to it
  }
  //update counter
   freePagesCounter--;
  if(kmem.use_lock)
    release(&kmem.lock);

  if((1 &&  DEBUG)|| KDEBUG ) cprintf("kalloc.c: kalloc: create first ref to pa 0x%x, currnet ref number = %d\n",V2P(r),r->refrences);

  return (char*)r;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kallocWithRef(void)
{
  struct run *r;
  // char* v = kalloc();
  // if(kmem.use_lock)
  //   acquire(&kmem.lock);
  // r=&kmem.ref[V2P(v)/PGSIZE]; //r is the refrences for v page
  // r->refrences=1;
  // if(kmem.use_lock)
  //   release(&kmem.lock);
  

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;
  if(r){
    kmem.freelist = r->next;
    r->refrences=1; //allocated page has 1 refrence to it
  }
  //update counter
   freePagesCounter--;
  if(kmem.use_lock)
    release(&kmem.lock);

  if((1 &&  DEBUG)|| KDEBUG ) cprintf("kalloc.c: kallocWithRef: create first ref to pa 0x%x, currnet ref number = %d\n",V2P(r),r->refrences);

  return (char*)r;
}

//get virtual adress to increase refrences
//increase num of refrences for a page
void
kIncRef(char *v){
  struct run *r;
  
  if(kmem.use_lock)
    acquire(&kmem.lock);
  r=&kmem.ref[V2P(v)/PGSIZE]; //r is the refrences for v page
#if AVISHAIISCRAZY
  if(r->refrences < 1)
    r->refrences=1;
#endif
  r->refrences++;
  if(kmem.use_lock)
    release(&kmem.lock);
  
  if((1 &&  DEBUG)|| KDEBUG ) cprintf("kalloc.c: kIncRef: increased pa 0x%x, currnet ref number = %d\n",V2P(v),r->refrences);
}

//  get virtual address to decrese the ref count of the 
//  physical address relate to it
//  decrease num of refrences for a page
//  if after decreasing, ref count = 0 than free the physical page
void
kDecRef(char *v){
    struct run *r;

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r=&kmem.ref[V2P(v)/PGSIZE]; //r is the struct of the page
  r->refrences--;
  if(kmem.use_lock)
    release(&kmem.lock);
  if((1 &&  DEBUG)|| KDEBUG ) cprintf("kalloc.c: kDecRef: decreased pa 0x%x, currnet ref number = %d\n",V2P(v),r->refrences);
  if(r->refrences <= 0){
    if((1 &&  DEBUG)|| KDEBUG ) cprintf("kalloc.c: kDecRef: free pa 0x%x, currnet ref number = %d\n",V2P(v),r->refrences);
    kfree(v);
  }
}

//get virtual address to return how many refrences 
//get refrences for a spesific page
int
kGetRef(char *v){
  struct run *r;
  r=&kmem.ref[V2P(v)/PGSIZE]; //r is the refrences for v page
  return r->refrences;
}
