#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"
#include "limits.h"

extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler()
static pte_t * walkpgdir(pde_t *pgdir, const void *va, int alloc);

// get a virtual address
// return the pte addres or 0
uint
getPTE(pde_t *pgdir, const void *va){
  return (uint)walkpgdir(pgdir,va,0);
}

// checks the refrence bit,
// clear it anyway and return the ref bit value
int checkPTE_A(uint va){
  uint accessed;
  pte_t *pte = walkpgdir(myproc()->pgdir, (void*)va, 0);
  if (!*pte)
    return -1;
  accessed = (*pte) & PTE_A;
  (*pte) &= ~PTE_A;
  return accessed;
}


//SELECTION=NFUA
//The page with the lowest counter should be removed
int select_nfua_swap(void){
  int lowest_counterP=INT_MAX; //lowest age 
  int page_index=0;

  //find the page with the lowest counter
  for (int i = 0; i < MAX_PSYC_PAGES; i++){
   if(myproc()->memoryPages[i].age<lowest_counterP && myproc()->memoryPages[i].age>-1){
    lowest_counterP=myproc()->memoryPages[i].age;
    page_index=i;
   }
  }
  return page_index;
}

//SELECTION=LAPA
//the page with the smallest number of "1"s will be removed. 
//If there are several such pages, the one with the lowest counter value should be removed.
int select_lapa_swap(void){
  int num_of_ones=INT_MAX;
  int lowest_counterP=INT_MAX; //lowest age 
  int page_index=0;
  uint a=0;

  //find the page w- lowest num of 1& if there are few, find the lowest age.
  for (int i = 0; i < MAX_PSYC_PAGES; i++){
    a = myproc()->memoryPages[i].age;
    int num=0;
    while (a){ //count num of 1
      num += a & 1;
      a = a >> 1;
    }
    if(num<num_of_ones){
      num_of_ones=num;
      lowest_counterP=myproc()->memoryPages[i].age;
      page_index=i;    
    }
    else if(num==num_of_ones){ 
      //find the one with smallest age
      if(myproc()->memoryPages[i].age<lowest_counterP){
        page_index=i;
        lowest_counterP=myproc()->memoryPages[i].age;
      }
    }
  }
  return page_index;
}

//SELECTION=SCFIFO
// find a page to swap out and return it's index in p->memoryPages[]
int select_scfifo_swap(){
  int found=0;
  int i=myproc()->headOfMemoryPages;

  //find first one with PTE_A=0
  while (!found)
  {
   if(checkPTE_A(myproc()->memoryPages[i].va)==0){
      return i;
   }
   // if didnt reach the end of the list
   else if(myproc()->memoryPages[i].next != -1){
     i=myproc()->memoryPages[i].next;
   }
   // if reached the end of the list
   else{
      cprintf("vm.c: select_scfifo_swap: PID %d, couldnt find page to swap in the memoryPages list, start over from head of the list\n",myproc()->pid);
      i=myproc()->headOfMemoryPages;
   }
  }
  return -1;
}

//SELECTION==AQ
//select the last inserted page in the memoryPages array
int select_aq_swap(){
  int i=myproc()->tailOfMemoryPages;

  return i;
}

// select a page from p->memoryPages[]
// and return its index.
// return -1 upon error
int
selectPageToSwap(){
  return select_scfifo_swap();
  //TODO:
  return -1;
}

// gets a proc p and an virtual addres of page. 
// Findes the va index in p->memoryPages
// and remove it from the list
// and decrese pagesInMemory ccounter
int
removePageFromMemory(struct proc* p,uint va){
  int index =-1;
  int i=0;
  uint cleanVA = va & ~0xFFF;
  if( 0 ) cprintf("vm.c: removePageFromMemory:va = 0x%x cleanVA =0x%x\n",va,cleanVA);

  //find the index of va in p->memoryPages
  for(; i<MAX_PSYC_PAGES && index < 0 ;i++){
    if(p->memoryPages[i].is_occupied == 1 && p->memoryPages[i].va == cleanVA){
      index = i;
    }
  }
  if(index < 0){
    cprintf("vm.c: removePageFromMemory: PID %d: couldnt find va 0x%x in p->memoryPages\n",p->pid,cleanVA);
    return -1;
  }
  // if index is the only node in the list
  if(p->headOfMemoryPages == index && p->tailOfMemoryPages == index){
    p->headOfMemoryPages = -1;
    p->tailOfMemoryPages = -1;
  }
  // if the index is the head of the list (and not the only node in the list)
  else if(p->headOfMemoryPages == index){
    p->headOfMemoryPages = p->memoryPages[index].next;
    p->memoryPages[p->headOfMemoryPages].prev = -1;
  }
  // if the index is the the last on the list (and not the only node in the list)
  else if(p->tailOfMemoryPages == index){
    p->tailOfMemoryPages = p->memoryPages[index].prev;
    p->memoryPages[p->tailOfMemoryPages].next = -1;
  }
  // if the index is in the middle of the list
  else{
    int prev = p->memoryPages[index].prev;
    int next = p->memoryPages[index].next;
    p->memoryPages[prev].next=next;
    p->memoryPages[next].prev=prev;
  }

  //clear the p->memoryPage entry
  p->memoryPages[index].next = -1;
  p->memoryPages[index].prev = -1;
  p->memoryPages[index].va = 0;
  p->memoryPages[index].age = 0;
  p->memoryPages[index].is_occupied = 0;

  p->pagesInMemory --;

  return 0;
}

// gets process and a virtual address of page with valid
// pysic address, find empty slot in and p->memoryPages[], 
// and push it to p->memoryPages
// Return -1 if error eccured
int 
pushToMemoryPagesArray(struct proc* p, uint va){
  int index =-1;
  int i=0;
  // find empty slot in 
  for(; i < MAX_PSYC_PAGES && index < 0 ; i++){
    //check if the slot va is already addressing a page
    if(p->memoryPages[i].is_occupied == 0){
      index = i;
      p->memoryPages[i].is_occupied =1;
    }
  }
  if(index < 0){
    cprintf("vm.c: pushToMemoryPagesArray: couldnt find empty slot in the memoryPages Array\n");
    return -1;
  }

  //pushing the page to the end of the list

  p->memoryPages[index].prev = p->tailOfMemoryPages; // if the queue was empty than  p->tailOfMemoryPages = -1 and its fine.
  p->memoryPages[index].next = -1;

  //if queue was empty
  if(p->headOfMemoryPages == -1){
    //setting the head index
    p->headOfMemoryPages= index;
  }
  //if queue wasnt empty
  else{
    //last tail is pointing to new tail
    p->memoryPages[p->tailOfMemoryPages].next = index;
  }

  //setting the tail index
  p->tailOfMemoryPages=index;


  // updating the page virtual address
  p->memoryPages[index].va = va;
  // updating the page age
  p->memoryPages[index].age = 0; //TODO: ask maya
  
  return 0;
}

// gets index in p->swapPages
// Delete the page content in the swapfile,
// clear p->swapPages[index], 
// and decrease the pagesInSwap counter
void
deleteFromSwap(int index){
  struct proc* p=myproc();
  char buffer[]={0,0,0,0};
  char *bufPtr=buffer;
  int a=0;
  // the p->swapPages is organized by the order of the pages in the swapfile
  int pageOffsetInSwapFile = index*PGSIZE;

  // write 4 chars of zero at a time
  for(; a < PGSIZE ; a+=4){
    if(writeToSwapFile(p,bufPtr, pageOffsetInSwapFile + a,4) < 0){
      cprintf("vm.c: deleteFromSwap: failed to initilize the 4 byte at offset %d in page %d in swapFile\n",a,index);
    }
  }
  // clearing p->swapPages[index]
  p->swapPages[index].va=0;
  p->swapPages[index].is_occupied = 0;

  p->pagesInSwap--;
}

// return the index of the page related with va 
// in p->swapPages
// Return -1 upon failuer.
// va doesnt have to be paged aligned
int
findInSwap(struct proc* p,uint va){
  int index = -1;
  int i = 0;
  uint cleanVA = va & ~0xFFF;// the va to the first byte in the page

  for( ; i < MAX_SWAP_PAGES && index < 0 ; i++){
    if(p->swapPages[i].is_occupied && p->swapPages[i].va == cleanVA)
      index = i;
  }
  return index;
}

// checks if p is valid user process for the paging system
// returns 0 if (p == 0 || p->pid <= 2 ) and 1 if p->pid > 2
int
isValidUserProc(struct proc*p){
  if( p == 0 /*kernal*/ || p->pid <= 2 /*shell & int procs*/)
    return 0;
  else return 1;
}



// find empty slot in the swapped pages array and Return it's index
int
findEmptyPageInSwap(struct proc *p){
  int i=0;
  int index = -1;

  // find unoccupied slot in swapPages
  for( ; i < MAX_SWAP_PAGES && index < 0 ; i++){
    if(p->swapPages[i].is_occupied == 0){
      index = i;
      p->swapPages[i].is_occupied = 1;
    }
  }
  return index;
}

// swap 1 page to swapfile.
// if failed return -1
// the page to swap is selected by the SELECT
// swap algorithem.
int
swapOut(){
  struct proc* p = myproc(); //only valid process calls swapOut() so p is valid
  int indexInMemoryPages,indexInSwapPages;
  pte_t *pte;

  //check if swap is not full
  if(!(p->pagesInSwap < MAX_SWAP_PAGES)){
    cprintf("vm.c: swap: PID %d: pages in swap >= 17, p->pagesInSwap = %d",p->pid,p->pagesInSwap);
    return -1;
  }
  //select page to swap out
  indexInMemoryPages=selectPageToSwap();
  if(indexInMemoryPages < 0){
    cprintf("vm.c: swap: PID %d: invalid indexInMemoryPages = %d",p->pid,indexInMemoryPages);
    return -1;
  }
  //find empty slot in swap
  indexInSwapPages=findEmptyPageInSwap(p);
  if(indexInSwapPages < 0){
    cprintf("vm.c: swap: PID %d: invalid indexInSwapPages = %d",p->pid,indexInSwapPages);
    return -1;
  }
  
  // copy page data to swapFile
  writeToSwapFile(p, (char*)p->memoryPages[indexInMemoryPages].va, indexInSwapPages*PGSIZE, PGSIZE);
  
  //update pte flags
  pte=walkpgdir(p->pgdir, (void*)p->memoryPages[indexInMemoryPages].va, 0);
  if(pte == 0){
    cprintf("vm.c: swap: PID %d: invalid indexInSwapPages = %d",indexInSwapPages);
    return -1;  
  }
  // Turinig on the PGfault flag
  *pte |= PTE_PG;
  // Clearing the PTE_P flag
  *pte &= ~PTE_P;

  //free the page on the pysic space
  kfree(P2V(PTE_ADDR(*pte)));
  //refresh RC3 (TLB)
  lcr3(V2P(p->pgdir));
  //remove from p->memoryPages[]
  removePageFromMemory(p,indexInMemoryPages);
  
  return 0;
}

// gets a virtual address to 
// load to pysic memory from swapFile.
// if there are already 16 page in memory, swapping 1 page out 
// Return 0 on success, -1 on error
int
loadPageToMemory(uint address){
  struct proc* p= myproc();
  uint page_va=address & ~0xFFF; //point to the begining of the page va
  pte_t *pte;
  char buffer[PGSIZE]="";
  char *bufPtr = buffer;
  char *mem;
  int indexInSwap;
  //check if need to swap out
  if(p->pagesInMemory >=16){
    if(swapOut() < 0){
      cprintf("vm.c: loadPageToMemory: PID %d: failed to swap out page,  = %d",p->pid,p->pagesInMemory);
      return -1;  
    }
  }
  // load page: get the page index is p->swapPages[]
  indexInSwap=findInSwap(p,address);
  if(indexInSwap < 0){
    cprintf("vm.c: loadPageToMemory: PID %d: failed to find address (0x%x) in swapPages",p->pid,address);
    return -1;
  }
  //alocating page in pysic memory
  mem=kalloc();
  if(mem == 0){
    cprintf("vm.c: loadPageToMemory: PID %d:  failed to allocate pysic memory\n",p->pid);
    return -1;
  }
  // reseting the page
  memset(mem, 0, PGSIZE);
  // copy data from swap to pysc memory
  // copy from swap to buffer
  if(readFromSwapFile(p, bufPtr, indexInSwap*PGSIZE, PGSIZE) < 0){
    cprintf("vm.c: loadPageToMemory: PID %d:  failed to copy from swapfile to buffer\n",p->pid);
    kfree(mem);
    return -1;
  }
  // copy from buffer to pysicl memory
  memmove(mem,bufPtr,PGSIZE);
  //push page to p->memoryPages[]
  if(pushToMemoryPagesArray(p,page_va) < 0){
    cprintf("vm.c: loadPageToMemory: PID %d:  failed to push page to p->memoryPages[]\n",p->pid);
    kfree(mem);
    return -1;
  }
  //update pte
  pte=(pte_t*)walkpgdir(p->pgdir,(void*)address,0);
  if(pte == 0){
    cprintf("vm.c: loadPageToMemory: PID %d:  couldnt find the address pte\n",p->pid);
    kfree(mem);
    return -1;
  }
  //put pysc address in pte with pte current plags and set PTE_P
  *pte = V2P(mem) /*pa*/ | (*pte & 0xFFF) /*current flags*/ | PTE_P;
  //clear PTE_PG
  *pte &= ~PTE_PG;
  return 0;
}

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
void
seginit(void)
{
  struct cpu *c;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  c = &cpus[cpuid()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
  lgdt(c->gdt, sizeof(c->gdt));
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
  } else {
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;

  a = (char*)PGROUNDDOWN((uint)va);
  last = (char*)PGROUNDDOWN(((uint)va) + size - 1);
  for(;;){
    if((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    if(*pte & PTE_P)
      panic("remap");
    *pte = pa | perm | PTE_P;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
} kmap[] = {
 { (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space
 { (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
 { (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
 { (void*)DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

// Set up kernel part of a page table.
pde_t*
setupkvm(void)
{
  pde_t *pgdir;
  struct kmap *k;

  if((pgdir = (pde_t*)kalloc()) == 0)
    return 0;
  memset(pgdir, 0, PGSIZE);
  if (P2V(PHYSTOP) > (void*)DEVSPACE)
    panic("PHYSTOP too high");
  for(k = kmap; k < &kmap[NELEM(kmap)]; k++)
    if(mappages(pgdir, k->virt, k->phys_end - k->phys_start,
                (uint)k->phys_start, k->perm) < 0) {
      freevm(pgdir);
      return 0;
    }
  return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
  kpgdir = setupkvm();
  switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
  lcr3(V2P(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p)
{
  if(p == 0)
    panic("switchuvm: no process");
  if(p->kstack == 0)
    panic("switchuvm: no kstack");
  if(p->pgdir == 0)
    panic("switchuvm: no pgdir");

  pushcli();
  mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
                                sizeof(mycpu()->ts)-1, 0);
  mycpu()->gdt[SEG_TSS].s = 0;
  mycpu()->ts.ss0 = SEG_KDATA << 3;
  mycpu()->ts.esp0 = (uint)p->kstack + KSTACKSIZE;
  // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
  // forbids I/O instructions (e.g., inb and outb) from user space
  mycpu()->ts.iomb = (ushort) 0xFFFF;
  ltr(SEG_TSS << 3);
  lcr3(V2P(p->pgdir));  // switch to process's address space
  popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);
  memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz)
{
  uint i, pa, n;
  pte_t *pte;

  if((uint) addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
      panic("loaduvm: address should exist");
    pa = PTE_ADDR(*pte);
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, P2V(pa), offset+i, n) != n)
      return -1;
  }
  return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  
  char *mem;
  uint a;
  struct proc* p=myproc();
  if( 1 && isValidUserProc(p) ) cprintf("vm.c: allocuvm: PID %d enter allocuvm\n",p->pid);
  if(newsz >= KERNBASE)
    return 0;
  if(newsz < oldsz)
    return oldsz;
  
  a = PGROUNDUP(oldsz);
  for(; a < newsz ; a += PGSIZE){
    // the section inside the if
    // doesnt apply on kernal and PID <= 2
    if(isValidUserProc(p)){
      if( 0 ) cprintf("vm.c: allocuvm: PID %d is valid, checking for num of pages in memory\n",p->pid);
      //checks if there are already 16 pages in pysc memory
      if(p->pagesInMemory == MAX_PSYC_PAGES){
        cprintf("vm.c: allocuvm: PID %d 16 pages in memory, swaping one page out\n");
        //swap out 1 page to free space for the new one
        if(swapOut() == -1){
          // if swap failed, act as the all allocuvm failed.
          cprintf("vm.c: allocuvm: failed to swap page out of PID:%d\n",p->pid);
          deallocuvm(pgdir, newsz, oldsz);
          return 0;
        }
      }
      //checks if process exceed 16 pages in memory
      else if(p->pagesInMemory > MAX_PSYC_PAGES){
        cprintf("vm.c: allocuvm: PID %d: has %d pages in pysical memory counter",p->pid,p->pagesInMemory);
        panic("vm.c: allocuvm: p is valid but has more than 16 pages");
      }
      //if process doesnt have 16 pages or more, it proceed.
    }

    mem = kalloc();
    //checks if p is initialized
    // if it does, increase the pages count of the process
    // we do it here because de
    
    
    if(mem == 0){
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
      cprintf("allocuvm out of memory (2)\n");
      deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return 0;
    }
     //if successfuly allocat the page and p-Pid > 2 update the coresponding entry
    if(isValidUserProc(p)){
      if(pushToMemoryPagesArray(p,a) <0 ){
        panic("vm.c: allocuvm: failed to push page to memory pages array");
      }
    }
    // update the proc page counter.
    if(p != 0){
       p->pagesInMemory++;
       if(1) cprintf("vm.c: allocuvm: PID %d added page(0x%x) to memory, pages in memory=%d\n",p->pid,a,p->pagesInMemory);
    }
    
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  pte_t *pte;
  uint a, pa;
  struct proc* p =myproc();
  if(newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for(; a  < oldsz; a += PGSIZE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(!pte)
      a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
    else if((*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte);
      if(pa == 0)
        panic("kfree");
      char *v = P2V(pa);
      kfree(v);
      //decrease the pages count if p is initialized
      // and the page is present
      if(isValidUserProc(p))
        removePageFromMemory(p,a);
      else if(p != 0)
        p->pagesInMemory--;
      *pte = 0;
    }
    // checks if the page is in swap
    else if((*pte & PTE_PG)){
      // if the page is in swap clear the swap
      int index = findInSwap(p,a);
      if(index < 0)
        cprintf("vm.c: deallocuvm: PTE_P is off, PTE_PG is on but couldnt find page in p->swapPages.\n");
      else{
        deleteFromSwap(index);
      }
      // clearing pte flags
      *pte = 0;
    }
  }
  return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir)
{
  uint i;

  if(pgdir == 0)
    panic("freevm: no pgdir");
  deallocuvm(pgdir, KERNBASE, 0);
  for(i = 0; i < NPDENTRIES; i++){
    if(pgdir[i] & PTE_P){
      char * v = P2V(PTE_ADDR(pgdir[i]));
      kfree(v);
    }
  }
  kfree((char*)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if(pte == 0)
    panic("clearpteu");
  *pte &= ~PTE_U;
}

// Given a parent process's page table, create a copy
// of it for a child.
pde_t*
copyuvm(pde_t *pgdir, uint sz)
{
  pde_t *d;
  pte_t *pte;
  uint pa, i, flags;
  char *mem;

  if((d = setupkvm()) == 0)
    return 0;
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
      panic("copyuvm: pte should exist");
    if(!(*pte & PTE_P))
      panic("copyuvm: page not present");
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto bad;
    memmove(mem, (char*)P2V(pa), PGSIZE);
    if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {
      kfree(mem);
      goto bad;
    }
  }
  return d;

bad:
  freevm(d);
  return 0;
}

//PAGEBREAK!
// Map user virtual address to kernel address.
char*
uva2ka(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if((*pte & PTE_P) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  return (char*)P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
  char *buf, *pa0;
  uint n, va0;

  buf = (char*)p;
  while(len > 0){
    va0 = (uint)PGROUNDDOWN(va);
    pa0 = uva2ka(pgdir, (char*)va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if(n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

