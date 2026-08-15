// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct bcache{
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
  int in_use;
} bcache;

/*struct bucket{
  struct buf bufs[BUCKETSIZE];
  struct spinlock lock;
};*/
struct bhash{
  struct bcache bucks[NBUCKET];
} bhash;

int bucket_hash(int blockno){
  return (blockno*17)%NBUCKET;
}

struct bcache * get_bucket(int blockno){
  //printf("calcuting hash\n");
  //printf("num is %d\n", bucket_hash(blockno));
  //printf("we have %p\n", bhash.bucks);
  return bhash.bucks + bucket_hash(blockno);
}


void bucketinit(struct bcache * buck , int bnum){
  struct buf *b;
  //char bname[10] = "bcache";
  //bname[6] = bnum;
  //bname[7] = '\n'; 
  //char bufname[10] = "buffer";
  //bufname[6] = bnum;
  //bufname[7] = '\n'; 
  //printf("buck is %p, bcache is %p, int is %d, bhash is %p\n", &(buck->lock), &bcache, bnum, &bhash);
  initlock(&buck->lock, "bcaches");
  //return;
  // Create linked list of buffers
  buck->head.prev = &buck->head;
  buck->head.next = &buck->head;
  buck->in_use=0;
  for(b = buck->buf; b < buck->buf+BUCKETSIZE; b++){
    b->next = buck->head.next;
    b->prev = &buck->head;
    initsleeplock(&b->lock, "buffer");
    buck->head.next->prev = b;
    buck->head.next = b;
  }
}

void hashinit(void){
  for (int i = 0; i < NBUCKET; i++ ){
    bucketinit(bhash.bucks + i, i);
    //printf("done!\n");
  }
  //printf("exiting hashinit\n");
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  //experimental
  hashinit();

  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  //printf("exiting trueinit\n");
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{ 
  //printf("bget enter\n");
  struct bcache * buck = get_bucket(blockno);
  //printf("WE HAVE HASH!\n");
  struct buf *b;
  //printf("bget enter\n");
  acquire(&buck->lock);

  // Is the block already cached?
  for(b = buck->head.next; b != &buck->head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&buck->lock);
      acquiresleep(&b->lock);
      //printf("bget exit\n");
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = buck->head.prev; b != &buck->head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&buck->lock);
      acquiresleep(&b->lock);
      //printf("bget exit\n");
      return b;
    }
  }
  //printf("bget exit\n");
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;
  //printf("bread enter\n");
  b = bget(dev, blockno);
  //printf("bret!\n");
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  //printf("bread exit\n");
  return b;
  
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  //printf("bwrite enter\n");
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
  //printf("bwrite exit\n");
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  struct bcache * buck = get_bucket(b->blockno);
  //printf("brelse enter\n");
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&buck->lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = buck->head.next;
    b->prev = &buck->head;
    buck->head.next->prev = b;
    buck->head.next = b;
  }
  
  release(&buck->lock);
  //printf("brelse exit\n");
}

void
bpin(struct buf *b) {
  //printf("bpin enter\n");
  struct bcache * buck = get_bucket(b->blockno);
  acquire(&buck->lock);
  b->refcnt++;
  release(&buck->lock);
  //printf("bpin exit\n");
}

void
bunpin(struct buf *b) {
  struct bcache * buck = get_bucket(b->blockno);
  //printf("bunpin enter\n");
  acquire(&buck->lock);
  b->refcnt--;
  release(&buck->lock);
  //printf("bunpin exit\n");
}


