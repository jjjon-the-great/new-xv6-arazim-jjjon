// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct memdata{
  struct spinlock lock;
  struct run *freelist;
};

struct memdata kmem;

struct memdata cpu_kmem[NCPU];

struct run * steal(int cur){
  //printf("starting to steal!\n");
  int selected = -1;
  for (int c = 0; c < NCPU; c++){
    if (c == cur) {continue;}
    acquire(&(cpu_kmem[c].lock));
    if (cpu_kmem[c].freelist) {
      selected = c;
      //release(&(cpu_kmem[selected].lock));
      break;
    }
    release(&(cpu_kmem[c].lock));
  }
  //printf("decided on %d\n", selected);
  if (selected == -1) return 0;
  struct run * new_block = 0;
  //acquire(&(cpu_kmem[selected].lock));
  if (cpu_kmem[selected].freelist){
    new_block = cpu_kmem[selected].freelist;
    cpu_kmem[selected].freelist = new_block->next;
  }
  release(&(cpu_kmem[selected].lock));
  //printf("stealing done! allocated %p. new one is %p\n", new_block, cpu_kmem[selected].freelist);
  return new_block;
}


void
kinit()
{ 
  //initlock(&kmem.lock, "kmem");
  for (int c = 0; c < NCPU; c++){
    initlock(&(cpu_kmem[c].lock), "kmem");
  }
  freerange(end, (void*)PHYSTOP);
  //printf("all initialized!\n");
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)

int getcpu(){
  push_off();
  int c = cpuid();
  pop_off();
  return c;
  return 0;
}

void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  int c = getcpu();
  acquire(&(cpu_kmem[c].lock));
  r->next = cpu_kmem[c].freelist;
  cpu_kmem[c].freelist = r;
  release(&(cpu_kmem[c].lock));
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int c = getcpu();

  acquire(&(cpu_kmem[c].lock));
  r = cpu_kmem[c].freelist;
  if (!r){
    r = steal(c);
    if (r) r->next = 0;
  }
  if(r)
    cpu_kmem[c].freelist = r->next;
  release(&(cpu_kmem[c].lock));

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
