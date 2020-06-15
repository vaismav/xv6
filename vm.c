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

void
printPagesInSwap(struct proc* p){
  int i;
  cprintf("\nvm.c: printPagesInSwap: PID %d SWAP pages list:\n",p->pid);
  for(i=0;i<MAX_SWAP_PAGES;i++){
    cprintf("vm.c: printPagesInSwap: index: %d, is_occupied: %d, va: 0x%x \n",
        i, p->swapPages[i].is_occupied, p->swapPages[i].va);
  }
}

//print to terminal the entire p->memoryPages entries
// used for debugging
void
pritntProcMemoryPages(struct proc* p){
  int i;
  cprintf("\nvm.c: pritntProcMemoryPages: PID %d memory pages list:\n",p->pid);
  for(i=0;i<MAX_PSYC_PAGES;i++){
    cprintf("vm.c: pritntProcMemoryPages: index: %d, is_occupied: %d, va: 0x%x, age: %d, next: %d, prev: %d  \n",
        i, p->memoryPages[i].is_occupied, p->memoryPages[i].va, p->memoryPages[i].age, p->memoryPages[i].next, p->memoryPages[i].prev);
  }
}

// checks the refrence bit,
// clear it anyway and return the ref bit value
int checkPTE_A(struct proc* p,uint va){
  uint accessed;
  pte_t *pte = walkpgdir(p->pgdir, (void*)va, 0);
  if (!(*pte))
    return -1;
  accessed = (*pte) & PTE_A; //0x020
  (*pte) &= ~PTE_A;
  return accessed;
}


//SELECTION=NFUA
//The page with the lowest counter should be removed
int select_nfua_swap(struct proc* p){
  uint lowest_counterP=__UINT32_MAX__; //lowest age 
  int page_index=-1;

  //find the page with the lowest counter
  for (int i = 0; i < MAX_PSYC_PAGES; i++){
    if(1 && DEBUG) cprintf("vm.c: select_nfua_swap: p->pid %d index %d, va 0x%x \tage 0x%x\n",p->pid, i, p->memoryPages[i].va, p->memoryPages[i].age);
      if(p->memoryPages[i].age < lowest_counterP){
        lowest_counterP=p->memoryPages[i].age;
        page_index=i;
      }
  }
  if(page_index < 0 ) cprintf("vm.c: select_nfua_swap: p->pid %d: invalid index %d\n",p->pid, page_index);
  else if(1 && DEBUG) cprintf("vm.c: select_nfua_swap: p->pid %d: selected page 0x%x in index %d\n",p->pid, p->memoryPages[page_index].va, page_index);
  return page_index;
}

//SELECTION=LAPA
//the page with the smallest number of "1"s will be removed. 
//If there are several such pages, the one with the lowest counter value should be removed.
int select_lapa_swap(struct proc* p){
  int num_of_ones=__INT_MAX__;
  int lowest_counterP=__INT_MAX__; //lowest age 
  int page_index=-1;
  uint a=0;

  //find the page w- lowest num of 1& if there are few, find the lowest age.
  for (int i = 0; i < MAX_PSYC_PAGES; i++){
    if(p->memoryPages[i].is_occupied){
      if(1 && DEBUG) cprintf("vm.c: select_lapa_swap: p->pid %d index %d, va 0x%x \tage 0x%x\n",p->pid, i, p->memoryPages[i].va, p->memoryPages[i].age);
      a = p->memoryPages[i].age;
      int num=0;
      while (a){ //count num of 1
        num += a & 1;
        a = a >> 1;
      }
      if(num<num_of_ones){
        num_of_ones=num;
        lowest_counterP=p->memoryPages[i].age;
        page_index=i;    
      }
      else if(num==num_of_ones){ 
        //find the one with smallest age
        if(p->memoryPages[i].age<lowest_counterP){
          page_index=i;
          lowest_counterP=p->memoryPages[i].age;
        }
      }
    }
  }
  if(1 && DEBUG) cprintf("vm.c: select_lapa_swap: p->pid %d: selected page 0x%x in index %d\n",p->pid, p->memoryPages[page_index].va, page_index);
  return page_index;
}

//SELECTION=SCFIFO
// find a page to swap out and return it's index in p->memoryPages[]
int select_scfifo_swap(struct proc* p){
  int found=0;
  int i=p->headOfMemoryPages;

  //find first one with PTE_A=0
  while (!found)
  {
   if(checkPTE_A(p, p->memoryPages[i].va)==0){
      return i;
   }
   // if didnt reach the end of the list
   else if(p->memoryPages[i].next != -1){
     i=p->memoryPages[i].next;
   }
   // if reached the end of the list
   else{
      if(DEBUG) cprintf("vm.c: select_scfifo_swap: PID %d, couldnt find page to swap in the memoryPages list, start over from head of the list\n",p->pid);
      i=p->headOfMemoryPages;
   }
  }
  return -1;
}

//SELECTION==AQ
//select the last inserted page in the memoryPages array
int select_aq_swap(struct proc* p){
  int i=p->headOfMemoryPages;
  if(i < 0)
    panic("vm.c: select_aq_swap: head index is < 0");
  return i;
}

// select a page from p->memoryPages[]
// and return its index.
// return -1 upon error
int
selectPageToSwap(struct proc* p){
  #ifdef NFUA
    if(1 && DEBUG) cprintf("vm: selectPageToSwap: PID %d: select page by NFUA policy \n",p->pid);
    return select_nfua_swap(p);
  #endif

  #ifdef LAPA
    if(1 && DEBUG) cprintf("vm: selectPageToSwap: PID %d: select page by LAPA policy \n",p->pid);
    return select_lapa_swap(p);
  #endif

  #ifdef SCFIFO
    if(1 && DEBUG) cprintf("vm: selectPageToSwap: PID %d: select page by SCFIFO policy \n",p->pid);
    return select_scfifo_swap(p);
  #endif

  #ifdef AQ
    if(1 && DEBUG) cprintf("vm: selectPageToSwap: PID %d: select page by AQ policy \n",p->pid);
    return select_aq_swap(p);
  #endif
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
  uint pageVA = va & ~0xFFF;
  if( 0 ) cprintf("vm.c: removePageFromMemory:va = 0x%x pageVA =0x%x\n",va,pageVA);

  //find the index of va in p->memoryPages
  for(; i<MAX_PSYC_PAGES && index < 0 ;i++){
    if(p->memoryPages[i].is_occupied == 1 && p->memoryPages[i].va == pageVA){
      index = i;
    }
  }
  if(index < 0){
    cprintf("vm.c: removePageFromMemory: PID %d: couldnt find va 0x%x in p->memoryPages\n",p->pid,pageVA);
    pritntProcMemoryPages(p);
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
  p->memoryPages[index].va = -1;
  p->memoryPages[index].age = 0;
  p->memoryPages[index].is_occupied = 0;

  p->pagesInMemory --;
  if(1 && DEBUG) cprintf("vm.c: removePageFromMemory: PID %d removed page(0x%x) from memory,p->memoryPages[index].is_occupied =%d, pages in memory=%d\n",p->pid,va,p->memoryPages[index].is_occupied,p->pagesInMemory);


  return 0;
}

// gets process and a virtual address of page with valid
// pysic address, find empty slot in and p->memoryPages[], 
// and push it to p->memoryPages
// and increase p->pagesInMemory counter
// Return -1 if error eccured
int 
pushToMemoryPagesArray(struct proc* p, uint va){
  int index =-1;
  int i=0;
  if(1 && DEBUG) cprintf("vm.c: pushToMemoryPagesArray: enter with p->pid %d, p->pagesInMemory = %d \n",p->pid,p->pagesInMemory);
  // find empty slot in
  for(; i < MAX_PSYC_PAGES && index < 0 ; i++){
    if(1 && DEBUG) cprintf("vm.c: pushToMemoryPagesArray: p->pid %d index %d va=0x%x is occupied=%d \n",p->pid ,i, p->memoryPages[i].va, p->memoryPages[i].is_occupied);

    //check if the slot va is already addressing a page
    if(p->memoryPages[i].is_occupied == 0){
      index = i;
      // p->memoryPages[index].is_occupied =1;
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
  #ifdef NFUA
  //for NFUA policy age =0 is ok
  p->memoryPages[index].age = 0; 
  #endif
  #ifdef LAPA
      //only LAPA policy needs to reset age to 0xFFFFFFFF
      p->memoryPages[index].age = __UINT32_MAX__;
  #endif

  p->memoryPages[index].is_occupied =1;
  p->pagesInMemory++;
  
  return 0;
}

//gets p and index in its p->pagesInSwap[] 
// and remove the data from the array
void
removeFromPagesInSwap(struct proc* p, int index){
  p->swapPages[index].va=-1;
  p->swapPages[index].is_occupied = 0;
  p->pagesInSwap--;
}

// gets index in p->swapPages
// Delete the page content in the swapfile,
// clear p->swapPages[index], 
// and decrease the pagesInSwap counter
void
deleteFromSwap(struct proc* p, int index){
  char buffer[]={0,0,0,0};
  char *bufPtr=buffer;
  int a=0;
  // the p->swapPages is organized by the order of the pages in the swapfile
  int pageOffsetInSwapFile = index*PGSIZE;

  // write 4 chars of zero at a time
  if(1 && DEBUG) cprintf("vm.c: deleteFromSwap: PID %d about to initialize page %d in swapFile by writing 0s\n",p->pid,index);
  for(; a < PGSIZE ; a+=4){
    if(writeToSwapFile(p,bufPtr, pageOffsetInSwapFile + a,4) < 0){
      cprintf("vm.c: deleteFromSwap: failed to initilize the 4 byte at offset %d in page %d in swapFile\n",a,index);
    }
  }
  if(1 && DEBUG) cprintf("vm.c: deleteFromSwap: PID %d finished to initialize page %d in swapFile by writing 0s\n",p->pid,index);
  // clearing p->swapPages[index]
  removeFromPagesInSwap(p,index);
}

// return the index of the page related with va 
// in p->swapPages
// Return -1 upon failuer.
// va doesnt have to be paged aligned
int
findInSwap(struct proc* p,uint va){
  int index = -1;
  int i = 0;
  uint pageVA = va & ~0xFFF;// the va to the first byte in the page

  for( ; i < MAX_SWAP_PAGES && index < 0 ; i++){
    if(p->swapPages[i].is_occupied && p->swapPages[i].va == pageVA)
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
swapOut(struct proc* p){
  //only valid process calls swapOut() so p is valid
  if(1 && DEBUG) cprintf("vm.c: swapOut: enter swapOut for p->pid %d\n",p->pid);

  int indexInMemoryPages,indexInSwapPages;
  pte_t *pte;

  //check if swap is not full
  if(!(p->pagesInSwap < MAX_SWAP_PAGES)){
    cprintf("vm.c: swapOut: PID %d: pages in swap >= 17, p->pagesInSwap = %d\n",p->pid,p->pagesInSwap);
    printPagesInSwap(p);
    pritntProcMemoryPages(p);
    panic("vm.c: swapOut: pages in swap >= 17");
    return -1;
  }
  //select page to swap out
  indexInMemoryPages=selectPageToSwap(p);
  if(indexInMemoryPages < 0){
    cprintf("vm.c: swapOut: PID %d: invalid indexInMemoryPages = %d\n",p->pid,indexInMemoryPages);
    return -1;
  }

  if(1 && DEBUG) cprintf("vm.c: swapOut: PID %d: chose page in index %d to swap out\n",p->pid,indexInMemoryPages);
  //find empty slot in swap
  indexInSwapPages=findEmptyPageInSwap(p);
  if(indexInSwapPages < 0){
    cprintf("vm.c: swapOut: PID %d: invalid indexInSwapPages = %d\n",p->pid,indexInSwapPages);
    printPagesInSwap(p);
    pritntProcMemoryPages(p);
    return -1;
  }

  // copy page data to swapFile  
  if(1 && DEBUG) cprintf("vm.c: swapOut: PID %d about copy page 0x%x to swapFile at index %d\n",p->pid,p->memoryPages[indexInMemoryPages].va, indexInSwapPages);
  if(writeToSwapFile(p, (char*)p->memoryPages[indexInMemoryPages].va, indexInSwapPages*PGSIZE, PGSIZE) != PGSIZE){
    cprintf("vm.c: swapOut: PID %d: faild to write PGSIZE bytes of page 0x%x to swapFile\n",p->pid,p->memoryPages[indexInMemoryPages].va);
    panic("vm.c: swapOut: faild to write to swapFile");
    return -1;
  }
  if(1 && DEBUG) cprintf("vm.c: swapOut: PID %d finished copy page 0x%x to swapFile at index %d\n",p->pid,p->memoryPages[indexInMemoryPages].va, indexInSwapPages);
  
  //update pte flags
  pte=walkpgdir(p->pgdir, (void*)p->memoryPages[indexInMemoryPages].va, 0);
  if(pte == 0){
    cprintf("vm.c: swapOut: PID %d: invalid indexInSwapPages = %d\n",p->pid,indexInSwapPages);
    return -1;  
  }
  // set page data in p->swapPages
  p->swapPages[indexInSwapPages].is_occupied = 1;
  p->swapPages[indexInSwapPages].va = p->memoryPages[indexInMemoryPages].va;
  p->pagesInSwap++;
  p->numOfPagedOut++;

  // Turinig on the PGfault flag
  *pte |= PTE_PG;
  // Clearing the PTE_P flag
  *pte &= ~PTE_P;
  // Clearing the PTE_A flag
  *pte &= ~PTE_A;
  if(COW){
    //seting W bit
    *pte |= PTE_W;
    //clearing the COW flag
    *pte &= ~PTE_COW; //TODO:
  }
  
  //free the page on the pysic space
  if(COW)
    kDecRef(P2V(PTE_ADDR(*pte)));
  else
    kfree(P2V(PTE_ADDR(*pte)));

  //refresh RC3 (TLB)
  lcr3(V2P(p->pgdir));
  //remove from p->memoryPages[]
  if(1 && DEBUG) cprintf("vm.c: swapOut: PID %d: removing page in index %d from p->memoryPages\n",p->pid,indexInMemoryPages);
  if(removePageFromMemory(p,p->memoryPages[indexInMemoryPages].va) < 0){
    cprintf("vm.c: swapOut: p->pid %d FAILED to removed page in index %d from memory \n",p->pid,indexInMemoryPages);
    return -1;
  }
  if(1 && DEBUG) cprintf("vm.c: swapOut: p->pid %d removed page in index %d from memory \n",p->pid,indexInMemoryPages);
  
  return 0;
}

// called from trap.c
// gets a virtual address to 
// load to pysic memory from swapFile.
// if there are already 16 page in memory, swapping 1 page out 
// Return 0 on success, -1 on error
int
loadPageToMemory(uint address){
  struct proc* p= myproc();
  uint page_va=address & ~0xFFF; //point to the begining of the page va
  pte_t *pte;

  char *mem;
  int indexInSwap;

   if(1 && DEBUG && p!=0) cprintf("vm.c: loadPageToMemory: PID %d: enter loadPageToMemory of page 0x%x for va 0x%x ",p->pid,page_va,address);

  //check if need to swap out
  if(p->pagesInMemory == MAX_PSYC_PAGES){
    if(swapOut(p) < 0){
      cprintf("vm.c: loadPageToMemory: PID %d: failed to swap out page,  = %d",p->pid,p->pagesInMemory);
      panic("swapOut FAILED");
      return -1;  
    }
  }
  else if(p->pagesInMemory >MAX_PSYC_PAGES ){
    panic("vm.c: loadPageToMemory: PID %d : p->pagesInMemory > MAX_PSYC_PAGES");
  }
  // load page: get the page index is p->swapPages[]
  indexInSwap=findInSwap(p,page_va);
  if(indexInSwap < 0){
    cprintf("vm.c: loadPageToMemory: PID %d: failed to find address (0x%x) in swapPages",p->pid,page_va);
    printPagesInSwap(p);
    return -1;
  }
  //alocating page in pysic memory
  if(COW) 
    mem = kallocWithRef();
  else
    mem = kalloc();
    
  if(mem == 0){
    cprintf("vm.c: loadPageToMemory: PID %d:  failed to allocate pysic memory\n",p->pid);
    return -1;
  }
  // reseting the page
  memset(mem, 0, PGSIZE);
  // copy data from swap to pysc memory

  // copy from swap to buffer
    if(readFromSwapFile(p, mem, indexInSwap*PGSIZE , PGSIZE)  != PGSIZE){
      cprintf("vm.c: loadPageToMemory: PID %d:  failed to copy from swapfile to buffer\n",p->pid);
      if(COW)
        kDecRef(mem);
      else
        kfree(mem);
      return -1;
    }

  
  //update pte
  pte=(pte_t*)walkpgdir(p->pgdir,(void*)address,0);
  if(pte == 0){
    cprintf("vm.c: loadPageToMemory: PID %d:  couldnt find the address pte\n",p->pid);
    if(COW)
      kDecRef(mem);
    else
      kfree(mem);
    return -1;
  }
  //put pysc address in pte with pte current plags and set PTE_P
  *pte = V2P(mem) /*pa*/ | (*pte & 0xFFF) /*current flags*/ | PTE_P | PTE_A;
  //clear PTE_PG
  *pte &= ~PTE_PG;

  //push page to p->memoryPages[]
  if(1 && DEBUG) cprintf("vm.c: loadPageToMemory: PID %d:  about to pushToMemoryPagesArray va =0x%x\n",p->pid,page_va);
  if(pushToMemoryPagesArray(p,page_va) < 0){
    cprintf("vm.c: loadPageToMemory: PID %d:  failed to push page to p->memoryPages[]\n",p->pid);
    if(COW)
      kDecRef(mem);
    else
      kfree(mem);
    return -1;
  }
  if(1 && DEBUG) cprintf("vm.c: loadPageToMemory: PID %d:  SUCCESSFULY pushToMemoryPagesArray va =0x%x\n",p->pid,page_va);
  //if successfuly brought the page frome swap to memory,
  // clear the entry in p->swapPages
  if(1 && DEBUG) cprintf("vm.c: loadPageToMemory: PID %d: about removeFromPagesInSwap index %d va =0x%x\n",p->pid, indexInSwap, p->swapPages[indexInSwap].va);
  removeFromPagesInSwap(p,indexInSwap);
  if(1 && DEBUG) cprintf("vm.c: loadPageToMemory: PID %d: SUCCESSFULY removed removeFromPagesInSwap index %d \n",p->pid, indexInSwap);
  if(1 && DEBUG) cprintf("vm.c: loadPageToMemory: PID %d: SUCCESSFULY loadPageToMemory pageVA 0x%x , exit the func with 0\n",p->pid, page_va);
  return 0;
}

//gets a uint of the faulting page
void
handle_write_fault(uint va){
  pte_t *pte; //entry in page table of the faulting page
  uint pa;    //physical addres of faulting page
  uint pan;   //physihandle_page_faultcal addres of new page
  //AVISHAI// uint va = rcr2(); //faulting virtual address 
  char *v;  //begining of virtual addres of faulting page
  uint flags;
  char *mem; // will be the adress of our new page if neseccery

  char *a = (char*)PGROUNDDOWN((uint)va); //start of the faulty page
  //if in kernal - kill
  if(va >= KERNBASE || (pte = walkpgdir(myproc()->pgdir, a, 0)) == 0){
    myproc()->killed = 1;
    panic("vm.c: handle_write_fault: kernal page");
    return;
  }
  //writable page fault
  if(FEC_WR){ 
    //if not dealing with COW - dont care
    if(!(*pte & PTE_COW)){ 
      myproc()->killed = 1;
      panic("vm.c: handle_write_fault: not COW");
      return;
    }
    else{ 
      //if dealing with COW
      pa = PTE_ADDR(*pte); 
      v = P2V(pa);
      flags = PTE_FLAGS(*pte);
      //check how many refeerance exisit to the page
      int refrences =kGetRef(v);
      //not just me
      if(refrences>1){ 
        //make a copy for myself
        mem = kalloc(); //new page
        memmove(mem, v, PGSIZE);
        pan=V2P(mem); 
        *pte = pan | flags | PTE_P | PTE_W; //writable 
        lcr3(V2P(myproc()->pgdir)); 
        // decresing the referance counter for v
        kDecRef(v);
      }
      //only I have refrence to this page - make it writable
      else{
        *pte |= PTE_W; //writable
        *pte &= ~PTE_COW;
        lcr3(V2P(myproc()->pgdir)); 
      }
    }
  }
  //not writable page fault - dont care
  else{ 
      return;
    }

}

void
handle_COW_write_fault(struct proc *p, uint va){
  cprintf("vm.c: handle_COW_write_fault: PID %d: entered function with va 0x%x\n",p->pid,va);
  pte_t *pte; //entry in page table of the faulting page
  uint pa;    //physical addres of faulting page
  uint pan;   //physical addres of new page
  char *v;  //begining of virtual addres of faulting page
  uint flags;
  char *mem; // will be the adress of our new page if neseccery

  char *a = (char*)PGROUNDDOWN((uint)va); //start of the faulty page
  //if in kernal - kill
  if(va >= KERNBASE || (pte = walkpgdir(p->pgdir, a, 0)) == 0){
    p->killed = 1;
    panic("vm.c: handle_write_fault: kernal page");
    return;
  }
  
  //if dealing with COW
  pa = PTE_ADDR(*pte); 
  v = P2V(pa);
  flags = PTE_FLAGS(*pte);
  //check how many refeerance exisit to the page
  int refrences =kGetRef(v);
  //not just me
  if(refrences > 1){ 
    //make a copy for myself
    mem = kallocWithRef(); //new page
    memmove(mem, v, PGSIZE);
    pan=V2P(mem); 
    *pte = pan | flags | PTE_P | PTE_W; //writable 
    *pte &= ~PTE_COW; 
    lcr3(V2P(p->pgdir)); 
    // decresing the referance counter for v
    kDecRef(v);
  }
  //only I have refrence to this page - make it writable
  else if (refrences == 1){
    *pte |= PTE_W; //writable
    *pte &= ~PTE_COW;
    lcr3(V2P(p->pgdir)); 
  }
  else{
    cprintf("vm.c: handle COW write fault: PID %d: about to panic on page va 0x%x , *pte= 0x%x pysical address=0x%x ref =%d\n",myproc()->pid,a,*pte, V2P(v),refrences);
    panic("vm.c: handle COW write fault: pa ref < 1");
  }

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
    if(*pte & PTE_P){
      if(0){
        cprintf("vm.c: mappages: ");
        // if(myproc() != 0) cprintf("PID %d :",myproc()->pid);
        // else cprintf("myproc() == 0 :");
        struct proc *p=procOfpgdir(pgdir);
        if(p != 0) cprintf("p->pid of pgdir %d :",p->pid);
        else cprintf("no proc connected to pgdir :");
        cprintf("va=0x%x, *pte = 0x%x, about to panic\n",a,*pte);
      }
      panic("remap");
    }
    *pte = pa | perm | PTE_P;
    if(0){
      cprintf("vm.c: mappages: ");
      // if(myproc() != 0) cprintf("PID %d :",myproc()->pid);
      // else cprintf("myproc() == 0 :");
      struct proc *p=procOfpgdir(pgdir);
      if(p != 0) cprintf("p->pid of pgdir %d :",p->pid);
      else cprintf("no proc connected to pgdir :");
      cprintf("va=0x%x, *pte = 0x%x\n",a,*pte);
    }
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
  if(COW)
    mem=kallocWithRef();
  else
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
  int defineNONE = 0;
  #ifdef NONE
    defineNONE=1;
  #endif

  char *mem;
  uint a;
  struct proc* p=procOfpgdir(pgdir);
  if(1 && DEBUG && isValidUserProc(p) && myproc() !=0 ) cprintf("vm.c: allocuvm: PID %d enter allocuvm with pgdir of p->pid %d\n",myproc()->pid,p->pid);
  if(newsz >= KERNBASE)
    return 0;
  if(newsz < oldsz)
    return oldsz;
  
  a = PGROUNDUP(oldsz);
  for(; a < newsz ; a += PGSIZE){
    // the section inside the if
    // doesnt apply on kernal and PID <= 2
    if(isValidUserProc(p) && !defineNONE){
      if( 0 ) cprintf("vm.c: allocuvm: PID %d is valid, checking for num of pages in memory\n",p->pid);
      //checks if there are already 16 pages in pysc memory
      if(p->pagesInMemory == MAX_PSYC_PAGES){
        if(1 && DEBUG) cprintf("vm.c: allocuvm: PID %d has MAX_PSYC_PAGES (p->pagesInMemory = %d) pages in memory, swaping one page out\n",p->pid,p->pagesInMemory);
        //swap out 1 page to free space for the new one
        if(swapOut(p) == -1){
          // if swap failed, act as the all allocuvm failed.
          cprintf("vm.c: allocuvm: failed to swap page out of PID:%d\n",p->pid);
          panic("vm.c: allocuvm: failed to swap page out");
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

    if(COW) 
      mem = kallocWithRef();
    else
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
      if(COW)
        kDecRef(mem);
      else
        kfree(mem);
      return 0;
    }
     //if successfuly allocat the page and p-Pid > 2 update the coresponding entry
     
    if(isValidUserProc(p) && !defineNONE){
      if(1 && DEBUG) cprintf("vm.c: allocuvm: PID %d:  about to pushToMemoryPagesArray va =0x%x\n",p->pid,a);
      if(pushToMemoryPagesArray(p,a) <0 ){
        panic("vm.c: allocuvm: failed to push page to memory pages array");
      }
    }
    else if(p != 0 && !defineNONE){
      //increase init and shel proc pages counter
       p->pagesInMemory++;
       if(1 && DEBUG) cprintf("vm.c: allocuvm: PID %d added page(0x%x) to memory, pages in memory=%d\n",p->pid,a,p->pagesInMemory);
    }
    
  }
  if(1 && DEBUG && p != 0) cprintf("vm.c: allocuvm: PID %d exit allocuvm ,p->pagesInMemory=%d, p->pagesInSwap=%d\n",p->pid,p->pagesInMemory,p->pagesInSwap);
  if(1 && DEBUG && p!=0 && p->pid > 2 && !defineNONE){
    printPagesInSwap(p);
    pritntProcMemoryPages(p);
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
  int defineNONE = 0;
  #ifdef NONE
    defineNONE=1;
  #endif

  pte_t *pte;
  uint a, pa;
  
  struct proc* p =procOfpgdir(pgdir);
  if(DEBUG){
    if(1 && p != 0) cprintf("vm.c: deallocuvm: PID %d enter deallocuvm p->pid=%d, pages in memory=%d, p->pagesInSwap = %d\n",myproc()->pid,p->pid,p->pagesInMemory,p->pagesInSwap);
    if(1 && p != 0) printPagesInSwap(p);
    if(1 && p != 0) pritntProcMemoryPages(p);
  }
  

  if(newsz >= oldsz){
    if(1 && DEBUG && p != 0) cprintf("vm.c: deallocuvm: PID %d exit deallocuvm BECAUSE newsz >= oldsz\n",p->pid);
    return oldsz;
  }

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
      if(COW){
        //if there is only one refrence - can be deleted
        kDecRef(v);   
      }else{
        kfree(v);
      }
      //decrease the pages count if p is initialized
      // and the page is present
      if(isValidUserProc(p) && !defineNONE)
        removePageFromMemory(p,a);
      else if(p != 0 && !defineNONE){
        p->pagesInMemory--;
        if(1 && DEBUG) cprintf("vm.c: deallocuvm: PID %d removed page(0x%x) from memory, pages in memory=%d\n",p->pid,a,p->pagesInMemory);
      }
      *pte = 0;
    }
    // checks if the page is in swap
    else if((*pte & PTE_PG) && !defineNONE){
      // if the page is in swap clear the swap
      if(1 && DEBUG) cprintf("vm.c: deallocuvm: PID %d PTE_P is off, PTE_PG is on for address 0x%x.\n",p->pid,a);
      int index = findInSwap(p,a);
      if(index < 0)
        cprintf("vm.c: deallocuvm: PTE_P is off, PTE_PG is on but couldnt find page in p->swapPages.\n");
      else{
        //deleteFromSwap(p,index);
        removeFromPagesInSwap(p,index);
      }
      // clearing pte flags
      *pte = 0;
    }
  }
  if(1 && DEBUG && p != 0) cprintf("vm.c: deallocuvm: PID %d exit deallocuvm\n",p->pid);
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
  if(1 && DEBUG) cprintf("vm.c: freevm: PID %d about to deallocuvm \n",myproc()->pid);
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


pde_t*
cowuvm(pde_t *pgdir, uint sz)
{
  pde_t *d;
  pte_t *pte;
  uint pa, i, flags;


  if((d = setupkvm()) == 0)
    return 0;
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
      panic("cowuvm: pte should exist");
    if(*pte & PTE_P){
      //clear PTE_W and put PTE_COW
      *pte |= PTE_COW;
      *pte &= ~PTE_W;
      pa = PTE_ADDR(*pte);
      flags = PTE_FLAGS(*pte);
      lcr3(V2P(pgdir)); //reinstall the page table
      // if((mem = kalloc()) == 0)
      //   goto bad;
      // memmove(mem, (char*)P2V(pa), PGSIZE);
      if(mappages(d, (void*)i, PGSIZE, pa, flags) < 0) {
        goto bad;
      }
      char *v=P2V(pa);
      kIncRef(v);
    }
    //copy pte of page in swapFile
    else if(*pte & PTE_PG){
      pa = -1;// an address in the end of KERNBASE which is imposible to reach for the user
      flags = PTE_FLAGS(*pte);
      //if page is in swap, just copy its flags
      if(mappages(d, (void*)i, PGSIZE, pa, flags) < 0) {
        goto bad;
      }
    }
    else{
      panic("cowuvm: page not present");
    }

  }
  lcr3(V2P(pgdir));
  return d;

bad:
  freevm(d);
  lcr3(V2P(pgdir));
  return 0;
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
  lcr3(V2P(pgdir));
  return d;

bad:
  freevm(d);
  lcr3(V2P(pgdir));
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

