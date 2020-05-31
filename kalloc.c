// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

#define MAXPAGES  (PHYSTOP / PGSIZE)

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct run {
  struct run *next;
  int ref; //refrence counter
};

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist; 
  struct run refrences[MAXPAGES]; //store the refrence count
} kmem;

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.


//OURS

//icrement the refrences
void
kinc(char *v)
{
  struct run *r;

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = &kmem.refrences[(V2P(v) / PGSIZE)];
  r->ref += 1;
  if(kmem.use_lock)
    release(&kmem.lock);
}

//decrement the refrences
void
kdec(char *v)
{
  struct run *r;

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = &kmem.refrences[(V2P(v) / PGSIZE)];
  r->ref -= 1;
  if(kmem.use_lock)
    release(&kmem.lock);
}

//get the refrence count of pages
int
getRefs(char *v)
{
  struct run *r;

  r = &kmem.refrences[(V2P(v) / PGSIZE)];
  return r->ref;
}

int
count_free_pages(void){
  int sum=0;

  if(kmem.use_lock)
    acquire(&kmem.lock);
  
  struct run *curr = kmem.freelist;
  while(curr!=0){
    sum++;
    curr=curr->next;
  }

  if(kmem.use_lock)
    release(&kmem.lock);
  return sum;
}

void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree2(p); 
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

  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  if(kmem.use_lock)
    acquire(&kmem.lock);
  //r = (struct run*)v;
  r=&kmem.refrences[(V2P(v) / PGSIZE)];
  if(r->ref != 1)
    panic("ref");
    
  r->next = kmem.freelist;
  r->ref=0;
  kmem.freelist = r;
  if(kmem.use_lock)
    release(&kmem.lock);
}

void
kfree2(char *v)
{
  struct run *r;

  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  if(kmem.use_lock)
    acquire(&kmem.lock);
  //r = (struct run*)v;
  r=&kmem.refrences[(V2P(v) / PGSIZE)];

  r->next = kmem.freelist;
  r->ref=0;
  kmem.freelist = r;
  if(kmem.use_lock)
    release(&kmem.lock);
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
    r->ref=0;
  }
  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}

