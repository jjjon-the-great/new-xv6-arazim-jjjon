#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgaccess(void){
  //definitions:
  uint64 first_page;
  int num_pages;
  uint64 answer_address;
  struct proc *p = myproc();
  uint answer = 0;

  //argument parsing
  argaddr(0, &first_page);
  argint(1, &num_pages);
  argaddr(2, &answer_address);
  if (num_pages>32) num_pages = 32;

  //magic
  for (int i = 0; i < num_pages; i ++){
    pte_t * page = walk(p->pagetable, (first_page+i*PGSIZE), 0);
    if ((*page) & (PTE_A)){
      answer += ((1L << i));
      *page = (*page) & (~PTE_A);
    }
  }

  //output
  if (copyout(p->pagetable, answer_address, (char*)&answer, sizeof(uint)) < 0) return -1;
  
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

