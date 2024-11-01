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
static void kfree_first(void *pa, int cpu_id);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  char lock_name[10];
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit()
{
  for(int i = 0; i < NCPU; i++){
    snprintf(kmem[i].lock_name, sizeof(kmem[i].lock_name), "kmem_%d", i);
    initlock(&kmem[i].lock, kmem[i].lock_name);
    
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  int i = 0;
  p = (char*)PGROUNDUP((uint64)pa_start);

  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    //allocating the physical memory to the freelists of all the cpus equally
    i++;
    kfree_first(p, i % NCPU);
  }
}

static void kfree_first(void *pa, int cpu_id)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem[cpu_id].lock);
  r->next = kmem[cpu_id].freelist;
  kmem[cpu_id].freelist = r;
  release(&kmem[cpu_id].lock);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  int cpu_id = cpuid();

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling references.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem[cpu_id].lock);
  r->next = kmem[cpu_id].freelist;
  kmem[cpu_id].freelist = r;
  release(&kmem[cpu_id].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  //acquire the current cpu
  int cpu_id = cpuid();

  acquire(&(kmem[cpu_id].lock));
  r = kmem[cpu_id].freelist;
  if(r)
    kmem[cpu_id].freelist = r->next;
  release(&(kmem[cpu_id].lock));

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk, just indicating that the memory is allocated.
    return (void*)r;
  }
  else {//Memory Stealing
    for(int other_cpu = 0; other_cpu < NCPU; other_cpu++){
      if(other_cpu == cpu_id)
        continue;
      acquire(&(kmem[other_cpu].lock));
      r = kmem[other_cpu].freelist;
      if(r){
        kmem[other_cpu].freelist = r->next;
      }
      release(&(kmem[other_cpu].lock));
      if(r){
        memset((char*)r, 5, PGSIZE); // fill with junk, just indicating that the memory is allocated.
        return (void*)r;
      }
    }
    return (void*)0;
  }
}
