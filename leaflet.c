#include "leaflet.h"
#include <stdlib.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

LEAF *LEAF_init(Lfloat sr, void *memory, size_t memorysize, Lfloat(*random)(void)) {
  // a single, static LEAF instance that is shared by everybody
  static LEAF leaf_;
  LEAF *leaf = &leaf_;
  leaf->_internal_mempool.leaf = leaf;
  leaf_pool_init(leaf, memory, memorysize);
  leaf->sampleRate = sr;
  leaf->invSampleRate = 1.0f/sr;
  leaf->twoPiTimesInvSampleRate = leaf->invSampleRate * TWO_PI;
  leaf->random = random;
  leaf->clearOnAllocation = 0;
  leaf->errorCallback = &LEAF_defaultErrorCallback;
  for (int i = 0; i < LEAFErrorNil; ++i)
    leaf->errorState[i] = 0;
  leaf->allocCount = 0;
  leaf->freeCount = 0;
  return leaf;
}

void leaf_pool_init(LEAF* const leaf, void* memory, size_t size) {
  mpool_create(memory, size, &leaf->_internal_mempool);
  leaf->mempool = &leaf->_internal_mempool;
}


void LEAF_defaultErrorCallback(LEAF* const leaf, LEAFErrorType whichone) {}

void LEAF_internalErrorCallback(LEAF* const leaf, LEAFErrorType whichone) {
  leaf->errorState[whichone] = 1;
  leaf->errorCallback(leaf, whichone);
}

// private functions
static inline size_t mpool_align(size_t size);
static inline mpool_node_t* create_node(void* block_location, mpool_node_t* next, mpool_node_t* prev, size_t size, size_t header_size);
static inline void delink_node(mpool_node_t* node);

/**
 * allocate memory from memory pool
 */
void* mpool_alloc(size_t asize, tMempool *pool) {
  pool->leaf->allocCount++;
#if LEAF_DEBUG
  DBG("alloc " + String(asize));
#endif
#if LEAF_USE_DYNAMIC_ALLOCATION
  void *temp =  malloc(asize);
  if (temp == NULL) {
    // allocation failed, exit from the program
    fprintf(stderr, "Out of memory.\n");
    exit(1);
  }
  if (pool->leaf->clearOnAllocation > 0) {
    memset(temp, 0, asize);
  }
  return temp;
#else
  // If the head is NULL, the mempool is full
  if (pool->head == NULL) {
    if ((pool->msize - pool->usize) > asize) {
      LEAF_internalErrorCallback(pool->leaf, LEAFMempoolFragmentation);
    }
    else {
      LEAF_internalErrorCallback(pool->leaf, LEAFMempoolOverrun);
    }
    return NULL;
  }
    
  // Should we alloc the first block large enough or check all blocks and pick the one closest in size?
  size_t size_to_alloc = mpool_align(asize);
  mpool_node_t *node_to_alloc = pool->head;
    
  // Traverse the free list for a large enough block
  while (node_to_alloc->size < size_to_alloc) {
    node_to_alloc = node_to_alloc->next;
        
    // If we reach the end of the free list, there
    // are no blocks large enough, return NULL
    if (node_to_alloc == NULL) {
      if ((pool->msize - pool->usize) > asize) {
        LEAF_internalErrorCallback(pool->leaf, LEAFMempoolFragmentation);
      }
      else {
        LEAF_internalErrorCallback(pool->leaf, LEAFMempoolOverrun);
      }
      return NULL;
    }
  }
    
  // Create a new node after the node to be allocated if there is enough space
  mpool_node_t* new_node;
  size_t leftover = node_to_alloc->size - size_to_alloc;
  node_to_alloc->size = size_to_alloc;
  if (leftover > pool->leaf->header_size) {
    long offset = (void *) node_to_alloc - (void *) pool->mpool;
    offset += pool->leaf->header_size + node_to_alloc->size;
    new_node = create_node(pool->mpool + offset,
                           node_to_alloc->next,
                           node_to_alloc->prev,
                           leftover - pool->leaf->header_size, pool->leaf->header_size);
  }
  else {
    // Add any leftover space to the allocated node to avoid fragmentation
    node_to_alloc->size += leftover;
        
    new_node = node_to_alloc->next;
  }
    
  // Update the head if we are allocating the first node of the free list
  // The head will be NULL if there is no space left
  if (pool->head == node_to_alloc) {
    pool->head = new_node;
  }
    
  // Remove the allocated node from the free list
  delink_node(node_to_alloc);
    
  pool->usize += pool->leaf->header_size + node_to_alloc->size;
    
  if (pool->leaf->clearOnAllocation > 0) {
    // FIXME: probably better to use memset
    char *new_pool = (char *) node_to_alloc->pool;
    for (int i = 0; i < node_to_alloc->size; i++) new_pool[i] = 0;
  }
    
  // Return the pool of the allocated node;
  return node_to_alloc->pool;
#endif
}

void mpool_free(void *ptr, tMempool *pool) {
  pool->leaf->freeCount++;
#if LEAF_DEBUG
  DBG("free");
#endif
#if LEAF_USE_DYNAMIC_ALLOCATION
  free(ptr);
#else
  //if (ptr < pool->mpool || ptr >= pool->mpool + pool->msize)
  // Get the node at the freed space
  mpool_node_t* freed_node = (mpool_node_t*) (ptr - pool->leaf->header_size);
    
  pool->usize -= pool->leaf->header_size + freed_node->size;
    
  // Check each node in the list against the newly freed one to see if it's adjacent in memory
  mpool_node_t* other_node = pool->head;
  mpool_node_t* next_node;
  while (other_node != NULL) {
    if ((long) other_node < (long) pool->mpool ||
        (long) other_node >= (((long) pool->mpool) + pool->msize)) {
      LEAF_internalErrorCallback(pool->leaf, LEAFInvalidFree);
      return;
    }
    next_node = other_node->next;
    // Check if a node is directly after the freed node
    if (((long) freed_node) + (pool->leaf->header_size + freed_node->size) == (long) other_node) {
      // Increase freed node's size
      freed_node->size += pool->leaf->header_size + other_node->size;
      // If we are merging with the head, move the head forward
      if (other_node == pool->head) pool->head = pool->head->next;
      // Delink the merged node
      delink_node(other_node);
    }
        
      // Check if a node is directly before the freed node
    else if (((long) other_node) + (pool->leaf->header_size + other_node->size) == (long) freed_node) {
      // Increase the merging node's size
      other_node->size += pool->leaf->header_size + freed_node->size;
            
      if (other_node != pool->head) {
        // Delink the merging node
        delink_node(other_node);
        // Attach the merging node to the head
        other_node->next = pool->head;
        // Merge
        freed_node = other_node;
      }
      else {
        // If we are merging with the head, move the head forward
        pool->head = pool->head->next;
        // Merge
        freed_node = other_node;
      }
    }
        
    other_node = next_node;
  }
    
  // Ensure the freed node is attached to the head
  freed_node->next = pool->head;
  if (pool->head != NULL) pool->head->prev = freed_node;
  pool->head = freed_node;
    
  // Format the freed pool
  //    char* freed_pool = (char*)freed_node->pool;
  //    for (int i = 0; i < freed_node->size; i++) freed_pool[i] = 0;
#endif
}

/**
 * create memory pool
 */
void mpool_create (void* memory, size_t size, tMempool* pool) {
  pool->leaf->header_size = mpool_align(sizeof(mpool_node_t));
    
  pool->mpool = (char*)memory;
  pool->usize  = 0;
  if (size < pool->leaf->header_size) {
    size = pool->leaf->header_size;
  }
  pool->msize  = size;
    
  pool->head = create_node(pool->mpool, NULL, NULL, pool->msize - pool->leaf->header_size, pool->leaf->header_size);
}

/**
 * align byte boundary
 */
static inline size_t mpool_align(size_t size) {
  return (size + (MPOOL_ALIGN_SIZE - 1)) & ~(MPOOL_ALIGN_SIZE - 1);
}

static inline mpool_node_t* create_node(void* block_location, mpool_node_t* next, mpool_node_t* prev, size_t size, size_t header_size) {
  mpool_node_t* node = (mpool_node_t*)block_location;
  node->pool = block_location + header_size;
  node->next = next;
  node->prev = prev;
  node->size = size;
  return node;
}

static inline void delink_node(mpool_node_t* node) {
  // If there is a node after the node to remove
  if (node->next != NULL) {
    // Close the link
    node->next->prev = node->prev;
  }
  // If there is a node before the node to remove
  if (node->prev != NULL) {
    // Close the link
    node->prev->next = node->next;
  }
    
  node->next = NULL;
  node->prev = NULL;
}

Lfloat LEAF_clip(Lfloat min, Lfloat val, Lfloat max) {
  if (val < min) {
    return min;
  }
  else if (val > max) {
    return max;
  }
  else {
    return val;
  }
}

#ifdef __cplusplus
}
#endif
