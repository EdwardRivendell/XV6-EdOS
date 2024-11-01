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

#define NBUCKET 13
struct {
  struct spinlock lock[NBUCKET];
  struct spinlock global_lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //struct buf head;
  struct buf hashbucket[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;
  int i;

  initlock(&bcache.global_lock, "bcache");

  for(i = 0; i < NBUCKET; i++) {
    initlock(&bcache.lock[i], "bcache");
    bcache.hashbucket[i].prev = &bcache.hashbucket[i];//Initialize every hashbucket, making its prev and next point to itself
    bcache.hashbucket[i].next = &bcache.hashbucket[i];
  }

  // Create linked list of buffers and distribute them across hash buckets
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    int bucket = (b - bcache.buf) % NBUCKET;//to which hashbucket the block belongs
    b->next = bcache.hashbucket[bucket].next;
    b->prev = &bcache.hashbucket[bucket];
    initsleeplock(&b->lock, "buffer");
    bcache.hashbucket[bucket].next->prev = b;
    bcache.hashbucket[bucket].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)//panic sched-lock 不是bget的问题
{
  struct buf *b;
  int bucket = blockno % NBUCKET;

  acquire(&bcache.lock[bucket]);

  // Is the block already cached?
  for(b = bcache.hashbucket[bucket].next; b != &bcache.hashbucket[bucket]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){

      b->refcnt++;

      release(&bcache.lock[bucket]);
      acquiresleep(&b->lock);
      
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.hashbucket[bucket].prev; b != &bcache.hashbucket[bucket]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;

      b->refcnt = 1;

      release(&bcache.lock[bucket]);
      acquiresleep(&b->lock);
      
      return b;
    }
  }
  // If no buffer is found in the current hash bucket, search other buckets
  for(int i = 0; i < NBUCKET; i++) {
    if(i == bucket) continue; // Skip the current bucket

    printf("No problem until 102");
    release(&bcache.lock[bucket]);
    printf("Np until 104");
    acquire(&bcache.global_lock);//bug
    printf("Np until 106");

    acquire(&bcache.lock[i]);
    printf("Np until 109");
    acquire(&bcache.lock[bucket]);
    printf("Np until 111");

    //获取一个锁同时释放前面一个锁
    for(b = bcache.hashbucket[i].prev; b != &bcache.hashbucket[i]; b = b->prev) {
      if(b->refcnt == 0) {
        // Remove from old bucket
        b->next->prev = b->prev;
        b->prev->next = b->next;

        // Add to new bucket
        //这里可否不同时拥有两个锁？
        b->next = bcache.hashbucket[bucket].next;
        b->prev = &bcache.hashbucket[bucket];
        bcache.hashbucket[bucket].next->prev = b;
        bcache.hashbucket[bucket].next = b;

        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;

        b->refcnt = 1;

        release(&bcache.lock[bucket]);
        release(&bcache.lock[i]);
        release(&bcache.global_lock);
        acquiresleep(&b->lock);
        
        return b;
      }
    }

    release(&bcache.global_lock);
  }
  
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  
  int bucket = b->blockno % NBUCKET;
  acquire(&bcache.lock[bucket]);

  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hashbucket[bucket].next;
    b->prev = &bcache.hashbucket[bucket];
    bcache.hashbucket[bucket].next->prev = b;
    bcache.hashbucket[bucket].next = b;
  }
  
  release(&bcache.lock[bucket]);
}

void
bpin(struct buf *b) {
  int bucket = b->blockno % NBUCKET;
  acquire(&bcache.lock[bucket]);

  b->refcnt++;

  release(&bcache.lock[bucket]);
}

void
bunpin(struct buf *b) {
  int bucket = b->blockno % NBUCKET;
  acquire(&bcache.lock[bucket]);

  b->refcnt--;

  release(&bcache.lock[bucket]);
}


