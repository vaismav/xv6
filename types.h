typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;
typedef uint pde_t;

//NEW
#define MAX_PSYC_PAGES 16   // pages in the physical memory
#define MAX_TOTAL_PAGES 32  // pages of process
#define OCCUPIED 1
#define UNOCCUPIED 0

// each page is saved in the p->swapPages struct with the
// virtual address of the first byte in the page.
// therefore every virtual address in the p->swapPages struct
// that is related to the process pages is ending with 0x000(the 12 LSB)
// which point to the first byte in the array
#define GET_PGTAB_VA 0xFFC00001