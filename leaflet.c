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

// Cycle
struct tCycle {
  tMempool *mempool;
  // Underlying phasor
  uint32_t phase;
  int32_t inc;
  Lfloat freq;
  Lfloat invSampleRateTimesTwoTo32;
  uint32_t mask;
};

tCycle *tCycle_new(LEAF *leaf) {
  return tCycle_newFromPool(leaf, leaf->mempool);
}

tCycle *tCycle_newFromPool(LEAF *leaf, tMempool *pool) {
  tCycle *c = (tCycle *) mpool_alloc(sizeof(tCycle), pool);
  c->mempool = pool;
  c->inc      =  0;
  c->phase    =  0;
  c->invSampleRateTimesTwoTo32 = (leaf->invSampleRate * TWO_TO_32);
  c->mask = SINE_TABLE_SIZE - 1;
  return c;
}

void    tCycle_free (tCycle *c) {
  if (c) {
    mpool_free(c, c->mempool);
  }
}

//need to check bounds and wrap table properly to allow through-zero FM
Lfloat tCycle_tick(tCycle *c) {
  uint32_t tempFrac;
  uint32_t idx;
  Lfloat samp0;
  Lfloat samp1;
    
  // Phasor increment
  c->phase += c->inc;
  // Wavetable synthesis
  idx = c->phase >> 21; //11 bit table 
  tempFrac = (c->phase & 2097151); //(2^21 - 1) all the lower bits i.e. the remainder of a division by 2^21  (2097151 is the 21 bits after the 11 bits that represent the main index) 
    
  samp0 = __leaf_table_sinewave[idx];
  idx = (idx + 1) & c->mask;
  samp1 = __leaf_table_sinewave[idx];
    
  return (samp0 + (samp1 - samp0) * ((Lfloat)tempFrac * 0.000000476837386f)); // 1/2097151 
}

void tCycle_setFreq(tCycle* const c, Lfloat freq) {
  c->freq  = freq;
  c->inc = freq * c->invSampleRateTimesTwoTo32;
}

const Lfloat __leaf_table_sinewave[SINE_TABLE_SIZE] = {0.0f, 0.00305f, 0.00613f, 0.00919f, 0.01227f, 0.01532f, 0.0184f, 0.02145f, 0.02454f, 0.02759f, 0.03067f, 0.03372f, 0.0368f, 0.03986f, 0.04291f, 0.04599f, 0.04904f, 0.05212f, 0.05518f, 0.05823f,
0.06131f, 0.06436f, 0.06741f, 0.0705f, 0.07355f, 0.0766f, 0.07965f, 0.08273f, 0.08578f, 0.08884f, 0.09189f, 0.09494f, 0.09799f, 0.10104f, 0.1041f, 0.10715f, 0.1102f, 0.11325f, 0.1163f, 0.11935f,
0.12241f, 0.12543f, 0.12848f, 0.13153f, 0.13455f, 0.1376f, 0.14066f, 0.14368f, 0.1467f, 0.14975f, 0.15277f, 0.15582f, 0.15884f, 0.16187f, 0.16489f, 0.16791f, 0.17093f, 0.17398f, 0.17697f, 0.17999f,
0.18301f, 0.18604f, 0.18906f, 0.19205f, 0.19507f, 0.19809f, 0.20108f, 0.2041f, 0.20709f, 0.21008f, 0.2131f, 0.21609f, 0.21909f, 0.22208f, 0.22507f, 0.22806f, 0.23105f, 0.23401f, 0.237f, 0.23999f,
0.24295f, 0.24594f, 0.2489f, 0.25189f, 0.25485f, 0.25781f, 0.26077f, 0.26373f, 0.26669f, 0.26965f, 0.27261f, 0.27554f, 0.2785f, 0.28143f, 0.28439f, 0.28732f, 0.29025f, 0.29321f, 0.29614f, 0.29907f,
0.30197f, 0.3049f, 0.30783f, 0.31076f, 0.31366f, 0.31656f, 0.31949f, 0.32239f, 0.32529f, 0.32819f, 0.33109f, 0.33398f, 0.33688f, 0.33975f, 0.34265f, 0.34552f, 0.34839f, 0.35126f, 0.35416f, 0.35703f,
0.35986f, 0.36273f, 0.3656f, 0.36844f, 0.37131f, 0.37415f, 0.37698f, 0.37982f, 0.38266f, 0.3855f, 0.38834f, 0.39114f, 0.39398f, 0.39679f, 0.3996f, 0.4024f, 0.40521f, 0.40802f, 0.41083f, 0.4136f,
0.41641f, 0.41919f, 0.42197f, 0.42474f, 0.42752f, 0.4303f, 0.43307f, 0.43582f, 0.4386f, 0.44135f, 0.44409f, 0.44684f, 0.44958f, 0.45233f, 0.45505f, 0.45779f, 0.46051f, 0.46323f, 0.46594f, 0.46866f,
0.47137f, 0.47409f, 0.47678f, 0.47946f, 0.48215f, 0.48483f, 0.48752f, 0.4902f, 0.49289f, 0.49554f, 0.4982f, 0.50085f, 0.50351f, 0.50616f, 0.50882f, 0.51144f, 0.51407f, 0.51672f, 0.51932f, 0.52194f,
0.52457f, 0.52716f, 0.52979f, 0.53238f, 0.53497f, 0.53757f, 0.54016f, 0.54272f, 0.54529f, 0.54788f, 0.55045f, 0.55298f, 0.55554f, 0.55811f, 0.56064f, 0.56317f, 0.5657f, 0.56824f, 0.57077f, 0.57327f,
0.57578f, 0.57828f, 0.58078f, 0.58328f, 0.58578f, 0.58826f, 0.59073f, 0.5932f, 0.59567f, 0.59814f, 0.60059f, 0.60303f, 0.6055f, 0.60791f, 0.61035f, 0.61279f, 0.6152f, 0.61761f, 0.62003f, 0.62244f,
0.62485f, 0.62723f, 0.62961f, 0.63199f, 0.63437f, 0.63675f, 0.6391f, 0.64145f, 0.6438f, 0.64615f, 0.6485f, 0.65082f, 0.65314f, 0.65546f, 0.65778f, 0.6601f, 0.66238f, 0.66467f, 0.66696f, 0.66925f,
0.67154f, 0.6738f, 0.67606f, 0.67831f, 0.68057f, 0.68283f, 0.68506f, 0.68729f, 0.68951f, 0.69174f, 0.69394f, 0.69614f, 0.69836f, 0.70053f, 0.70273f, 0.7049f, 0.70709f, 0.70926f, 0.7114f, 0.71356f,
0.7157f, 0.71783f, 0.71997f, 0.72211f, 0.72421f, 0.72632f, 0.72842f, 0.73053f, 0.73264f, 0.73471f, 0.73679f, 0.73886f, 0.74094f, 0.74298f, 0.74503f, 0.74707f, 0.74911f, 0.75113f, 0.75317f, 0.75519f,
0.75717f, 0.75919f, 0.76117f, 0.76315f, 0.76514f, 0.76712f, 0.76907f, 0.77103f, 0.77298f, 0.77493f, 0.77686f, 0.77878f, 0.7807f, 0.78262f, 0.78452f, 0.78644f, 0.78833f, 0.79019f, 0.79208f, 0.79395f,
0.79581f, 0.79767f, 0.7995f, 0.80136f, 0.80319f, 0.80499f, 0.80682f, 0.80862f, 0.81042f, 0.81223f, 0.814f, 0.8158f, 0.81757f, 0.81931f, 0.82108f, 0.82281f, 0.82455f, 0.82629f, 0.828f, 0.82974f,
0.83145f, 0.83313f, 0.83484f, 0.83652f, 0.8382f, 0.83987f, 0.84152f, 0.84317f, 0.84482f, 0.84647f, 0.84808f, 0.8497f, 0.85132f, 0.85294f, 0.85452f, 0.85611f, 0.8577f, 0.85928f, 0.86084f, 0.8624f,
0.86395f, 0.86548f, 0.867f, 0.86853f, 0.87006f, 0.87155f, 0.87308f, 0.87457f, 0.87604f, 0.8775f, 0.879f, 0.88043f, 0.8819f, 0.88333f, 0.88477f, 0.8862f, 0.8876f, 0.88901f, 0.89041f, 0.89182f,
0.89319f, 0.89456f, 0.89594f, 0.89731f, 0.89865f, 0.89999f, 0.90131f, 0.90265f, 0.90396f, 0.90527f, 0.90656f, 0.90787f, 0.90915f, 0.9104f, 0.91168f, 0.91293f, 0.91418f, 0.91541f, 0.91666f, 0.91788f,
0.9191f, 0.92029f, 0.92148f, 0.92267f, 0.92386f, 0.92502f, 0.92618f, 0.92734f, 0.92847f, 0.92963f, 0.93073f, 0.93185f, 0.93295f, 0.93405f, 0.93515f, 0.93625f, 0.93732f, 0.93839f, 0.93942f, 0.94049f,
0.94153f, 0.94254f, 0.94357f, 0.94458f, 0.94559f, 0.94656f, 0.94757f, 0.94852f, 0.94949f, 0.95047f, 0.95142f, 0.95233f, 0.95328f, 0.95419f, 0.95511f, 0.95602f, 0.95691f, 0.95779f, 0.95868f, 0.95953f,
0.96039f, 0.96124f, 0.9621f, 0.96292f, 0.96375f, 0.96457f, 0.96536f, 0.96616f, 0.96695f, 0.96771f, 0.96851f, 0.96924f, 0.97f, 0.97073f, 0.97147f, 0.9722f, 0.9729f, 0.9736f, 0.9743f, 0.97501f,
0.97568f, 0.97635f, 0.97699f, 0.97766f, 0.9783f, 0.97891f, 0.97955f, 0.98016f, 0.98074f, 0.98135f, 0.98193f, 0.98251f, 0.98306f, 0.98364f, 0.98419f, 0.98471f, 0.98526f, 0.98578f, 0.98627f, 0.98679f,
0.98727f, 0.98776f, 0.98822f, 0.98868f, 0.98914f, 0.98959f, 0.99002f, 0.99045f, 0.99088f, 0.99127f, 0.99167f, 0.99207f, 0.99246f, 0.99283f, 0.99319f, 0.99353f, 0.99387f, 0.9942f, 0.99454f, 0.99484f,
0.99515f, 0.99545f, 0.99573f, 0.996f, 0.99628f, 0.99655f, 0.9968f, 0.99704f, 0.99725f, 0.99747f, 0.99768f, 0.99789f, 0.99808f, 0.99826f, 0.99844f, 0.9986f, 0.99878f, 0.9989f, 0.99905f, 0.99918f,
0.9993f, 0.99939f, 0.99951f, 0.9996f, 0.99966f, 0.99973f, 0.99979f, 0.99985f, 0.99991f, 0.99994f, 0.99994f, 0.99997f, 0.99997f, 0.99997f, 0.99994f, 0.99994f, 0.99991f, 0.99985f, 0.99979f, 0.99973f,
0.99966f, 0.9996f, 0.99951f, 0.99939f, 0.9993f, 0.99918f, 0.99905f, 0.9989f, 0.99878f, 0.9986f, 0.99844f, 0.99826f, 0.99808f, 0.99789f, 0.99768f, 0.99747f, 0.99725f, 0.99704f, 0.9968f, 0.99655f,
0.99628f, 0.996f, 0.99573f, 0.99545f, 0.99515f, 0.99484f, 0.99454f, 0.9942f, 0.99387f, 0.99353f, 0.99319f, 0.99283f, 0.99246f, 0.99207f, 0.99167f, 0.99127f, 0.99088f, 0.99045f, 0.99002f, 0.98959f,
0.98914f, 0.98868f, 0.98822f, 0.98776f, 0.98727f, 0.98679f, 0.98627f, 0.98578f, 0.98526f, 0.98471f, 0.98419f, 0.98364f, 0.98306f, 0.98251f, 0.98193f, 0.98135f, 0.98074f, 0.98016f, 0.97955f, 0.97891f,
0.9783f, 0.97766f, 0.97699f, 0.97635f, 0.97568f, 0.97501f, 0.9743f, 0.9736f, 0.9729f, 0.9722f, 0.97147f, 0.97073f, 0.97f, 0.96924f, 0.96851f, 0.96771f, 0.96695f, 0.96616f, 0.96536f, 0.96457f,
0.96375f, 0.96292f, 0.9621f, 0.96124f, 0.96039f, 0.95953f, 0.95868f, 0.95779f, 0.95691f, 0.95602f, 0.95511f, 0.95419f, 0.95328f, 0.95233f, 0.95142f, 0.95047f, 0.94949f, 0.94852f, 0.94757f, 0.94656f,
0.94559f, 0.94458f, 0.94357f, 0.94254f, 0.94153f, 0.94049f, 0.93942f, 0.93839f, 0.93732f, 0.93625f, 0.93515f, 0.93405f, 0.93295f, 0.93185f, 0.93073f, 0.92963f, 0.92847f, 0.92734f, 0.92618f, 0.92502f,
0.92386f, 0.92267f, 0.92148f, 0.92029f, 0.9191f, 0.91788f, 0.91666f, 0.91541f, 0.91418f, 0.91293f, 0.91168f, 0.9104f, 0.90915f, 0.90787f, 0.90656f, 0.90527f, 0.90396f, 0.90265f, 0.90131f, 0.89999f,
0.89865f, 0.89731f, 0.89594f, 0.89456f, 0.89319f, 0.89182f, 0.89041f, 0.88901f, 0.8876f, 0.8862f, 0.88477f, 0.88333f, 0.8819f, 0.88043f, 0.879f, 0.8775f, 0.87604f, 0.87457f, 0.87308f, 0.87155f,
0.87006f, 0.86853f, 0.867f, 0.86548f, 0.86395f, 0.8624f, 0.86084f, 0.85928f, 0.8577f, 0.85611f, 0.85452f, 0.85294f, 0.85132f, 0.8497f, 0.84808f, 0.84647f, 0.84482f, 0.84317f, 0.84152f, 0.83987f,
0.8382f, 0.83652f, 0.83484f, 0.83313f, 0.83145f, 0.82974f, 0.828f, 0.82629f, 0.82455f, 0.82281f, 0.82108f, 0.81931f, 0.81757f, 0.8158f, 0.814f, 0.81223f, 0.81042f, 0.80862f, 0.80682f, 0.80499f,
0.80319f, 0.80136f, 0.7995f, 0.79767f, 0.79581f, 0.79395f, 0.79208f, 0.79019f, 0.78833f, 0.78644f, 0.78452f, 0.78262f, 0.7807f, 0.77878f, 0.77686f, 0.77493f, 0.77298f, 0.77103f, 0.76907f, 0.76712f,
0.76514f, 0.76315f, 0.76117f, 0.75919f, 0.75717f, 0.75519f, 0.75317f, 0.75113f, 0.74911f, 0.74707f, 0.74503f, 0.74298f, 0.74094f, 0.73886f, 0.73679f, 0.73471f, 0.73264f, 0.73053f, 0.72842f, 0.72632f,
0.72421f, 0.72211f, 0.71997f, 0.71783f, 0.7157f, 0.71356f, 0.7114f, 0.70926f, 0.70709f, 0.7049f, 0.70273f, 0.70053f, 0.69836f, 0.69614f, 0.69394f, 0.69174f, 0.68951f, 0.68729f, 0.68506f, 0.68283f,
0.68057f, 0.67831f, 0.67606f, 0.6738f, 0.67154f, 0.66925f, 0.66696f, 0.66467f, 0.66238f, 0.6601f, 0.65778f, 0.65546f, 0.65314f, 0.65082f, 0.6485f, 0.64615f, 0.6438f, 0.64145f, 0.6391f, 0.63675f,
0.63437f, 0.63199f, 0.62961f, 0.62723f, 0.62485f, 0.62244f, 0.62003f, 0.61761f, 0.6152f, 0.61279f, 0.61035f, 0.60791f, 0.6055f, 0.60303f, 0.60059f, 0.59814f, 0.59567f, 0.5932f, 0.59073f, 0.58826f,
0.58578f, 0.58328f, 0.58078f, 0.57828f, 0.57578f, 0.57327f, 0.57077f, 0.56824f, 0.5657f, 0.56317f, 0.56064f, 0.55811f, 0.55554f, 0.55298f, 0.55045f, 0.54788f, 0.54529f, 0.54272f, 0.54016f, 0.53757f,
0.53497f, 0.53238f, 0.52979f, 0.52716f, 0.52457f, 0.52194f, 0.51932f, 0.51672f, 0.51407f, 0.51144f, 0.50882f, 0.50616f, 0.50351f, 0.50085f, 0.4982f, 0.49554f, 0.49289f, 0.4902f, 0.48752f, 0.48483f,
0.48215f, 0.47946f, 0.47678f, 0.47409f, 0.47137f, 0.46866f, 0.46594f, 0.46323f, 0.46051f, 0.45779f, 0.45505f, 0.45233f, 0.44958f, 0.44684f, 0.44409f, 0.44135f, 0.4386f, 0.43582f, 0.43307f, 0.4303f,
0.42752f, 0.42474f, 0.42197f, 0.41919f, 0.41641f, 0.4136f, 0.41083f, 0.40802f, 0.40521f, 0.4024f, 0.3996f, 0.39679f, 0.39398f, 0.39114f, 0.38834f, 0.3855f, 0.38266f, 0.37982f, 0.37698f, 0.37415f,
0.37131f, 0.36844f, 0.3656f, 0.36273f, 0.35986f, 0.35703f, 0.35416f, 0.35126f, 0.34839f, 0.34552f, 0.34265f, 0.33975f, 0.33688f, 0.33398f, 0.33109f, 0.32819f, 0.32529f, 0.32239f, 0.31949f, 0.31656f,
0.31366f, 0.31076f, 0.30783f, 0.3049f, 0.30197f, 0.29907f, 0.29614f, 0.29321f, 0.29025f, 0.28732f, 0.28439f, 0.28143f, 0.2785f, 0.27554f, 0.27261f, 0.26965f, 0.26669f, 0.26373f, 0.26077f, 0.25781f,
0.25485f, 0.25189f, 0.2489f, 0.24594f, 0.24295f, 0.23999f, 0.237f, 0.23401f, 0.23105f, 0.22806f, 0.22507f, 0.22208f, 0.21909f, 0.21609f, 0.2131f, 0.21008f, 0.20709f, 0.2041f, 0.20108f, 0.19809f,
0.19507f, 0.19205f, 0.18906f, 0.18604f, 0.18301f, 0.17999f, 0.17697f, 0.17398f, 0.17093f, 0.16791f, 0.16489f, 0.16187f, 0.15884f, 0.15582f, 0.15277f, 0.14975f, 0.1467f, 0.14368f, 0.14066f, 0.1376f,
0.13455f, 0.13153f, 0.12848f, 0.12543f, 0.12241f, 0.11935f, 0.1163f, 0.11325f, 0.1102f, 0.10715f, 0.1041f, 0.10104f, 0.09799f, 0.09494f, 0.09189f, 0.08884f, 0.08578f, 0.08273f, 0.07965f, 0.0766f,
0.07355f, 0.0705f, 0.06741f, 0.06436f, 0.06131f, 0.05823f, 0.05518f, 0.05212f, 0.04904f, 0.04599f, 0.04291f, 0.03986f, 0.0368f, 0.03372f, 0.03067f, 0.02759f, 0.02454f, 0.02145f, 0.0184f, 0.01532f,
0.01227f, 0.00919f, 0.00613f, 0.00305f, 0.0f, -0.00308f, -0.00616f, -0.00922f, -0.0123f, -0.01535f, -0.01843f, -0.02148f, -0.02457f, -0.02762f, -0.0307f, -0.03375f, -0.03683f, -0.03989f, -0.04294f, -0.04602f,
-0.04907f, -0.05215f, -0.05521f, -0.05826f, -0.06134f, -0.06439f, -0.06744f, -0.07053f, -0.07358f, -0.07663f, -0.07968f, -0.08276f, -0.08582f, -0.08887f, -0.09192f, -0.09497f, -0.09802f, -0.10107f, -0.10413f, -0.10718f,
-0.11023f, -0.11328f, -0.11633f, -0.11938f, -0.12244f, -0.12546f, -0.12851f, -0.13156f, -0.13458f, -0.13763f, -0.14069f, -0.14371f, -0.14673f, -0.14978f, -0.1528f, -0.15585f, -0.15887f, -0.1619f, -0.16492f, -0.16794f,
-0.17096f, -0.17401f, -0.177f, -0.18002f, -0.18304f, -0.18607f, -0.18909f, -0.19208f, -0.1951f, -0.19812f, -0.20111f, -0.20413f, -0.20712f, -0.21011f, -0.21313f, -0.21613f, -0.21912f, -0.22211f, -0.2251f, -0.22809f,
-0.23108f, -0.23404f, -0.23703f, -0.24002f, -0.24298f, -0.24597f, -0.24893f, -0.25192f, -0.25488f, -0.25784f, -0.2608f, -0.26376f, -0.26672f, -0.26968f, -0.27264f, -0.27557f, -0.27853f, -0.28146f, -0.28442f, -0.28735f,
-0.29028f, -0.29324f, -0.29617f, -0.2991f, -0.302f, -0.30493f, -0.30786f, -0.31079f, -0.31369f, -0.31659f, -0.31952f, -0.32242f, -0.32532f, -0.32822f, -0.33112f, -0.33401f, -0.33691f, -0.33978f, -0.34268f, -0.34555f,
-0.34842f, -0.35129f, -0.35419f, -0.35706f, -0.35989f, -0.36276f, -0.36563f, -0.36847f, -0.37134f, -0.37418f, -0.37701f, -0.37985f, -0.38269f, -0.38553f, -0.38837f, -0.39117f, -0.39401f, -0.39682f, -0.39963f, -0.40244f,
-0.40524f, -0.40805f, -0.41086f, -0.41364f, -0.41644f, -0.41922f, -0.422f, -0.42477f, -0.42755f, -0.43033f, -0.43311f, -0.43585f, -0.43863f, -0.44138f, -0.44412f, -0.44687f, -0.44962f, -0.45236f, -0.45508f, -0.45782f,
-0.46054f, -0.46326f, -0.46597f, -0.46869f, -0.47141f, -0.47412f, -0.47681f, -0.47949f, -0.48218f, -0.48486f, -0.48755f, -0.49023f, -0.49292f, -0.49557f, -0.49823f, -0.50089f, -0.50354f, -0.5062f, -0.50885f, -0.51147f,
-0.5141f, -0.51675f, -0.51935f, -0.52197f, -0.5246f, -0.52719f, -0.52982f, -0.53241f, -0.535f, -0.5376f, -0.54019f, -0.54276f, -0.54532f, -0.54791f, -0.55048f, -0.55301f, -0.55557f, -0.55814f, -0.56067f, -0.5632f,
-0.56573f, -0.56827f, -0.5708f, -0.5733f, -0.57581f, -0.57831f, -0.58081f, -0.58331f, -0.58582f, -0.58829f, -0.59076f, -0.59323f, -0.5957f, -0.59818f, -0.60062f, -0.60306f, -0.60553f, -0.60794f, -0.61038f, -0.61282f,
-0.61523f, -0.61765f, -0.62006f, -0.62247f, -0.62488f, -0.62726f, -0.62964f, -0.63202f, -0.6344f, -0.63678f, -0.63913f, -0.64148f, -0.64383f, -0.64618f, -0.64853f, -0.65085f, -0.65317f, -0.65549f, -0.65781f, -0.66013f,
-0.66241f, -0.6647f, -0.66699f, -0.66928f, -0.67157f, -0.67383f, -0.67609f, -0.67834f, -0.6806f, -0.68286f, -0.68509f, -0.68732f, -0.68954f, -0.69177f, -0.69397f, -0.69617f, -0.69839f, -0.70056f, -0.70276f, -0.70493f,
-0.70712f, -0.70929f, -0.71143f, -0.71359f, -0.71573f, -0.71786f, -0.72f, -0.72214f, -0.72424f, -0.72635f, -0.72845f, -0.73056f, -0.73267f, -0.73474f, -0.73682f, -0.73889f, -0.74097f, -0.74301f, -0.74506f, -0.7471f,
-0.74915f, -0.75116f, -0.7532f, -0.75522f, -0.7572f, -0.75922f, -0.7612f, -0.76318f, -0.76517f, -0.76715f, -0.7691f, -0.77106f, -0.77301f, -0.77496f, -0.77689f, -0.77881f, -0.78073f, -0.78265f, -0.78455f, -0.78647f,
-0.78836f, -0.79022f, -0.79211f, -0.79398f, -0.79584f, -0.7977f, -0.79953f, -0.80139f, -0.80322f, -0.80502f, -0.80685f, -0.80865f, -0.81046f, -0.81226f, -0.81403f, -0.81583f, -0.8176f, -0.81934f, -0.82111f, -0.82285f,
-0.82458f, -0.82632f, -0.82803f, -0.82977f, -0.83148f, -0.83316f, -0.83487f, -0.83655f, -0.83823f, -0.8399f, -0.84155f, -0.8432f, -0.84485f, -0.8465f, -0.84811f, -0.84973f, -0.85135f, -0.85297f, -0.85455f, -0.85614f,
-0.85773f, -0.85931f, -0.86087f, -0.86243f, -0.86398f, -0.86551f, -0.86703f, -0.86856f, -0.87009f, -0.87158f, -0.87311f, -0.8746f, -0.87607f, -0.87753f, -0.87903f, -0.88046f, -0.88193f, -0.88336f, -0.8848f, -0.88623f,
-0.88763f, -0.88904f, -0.89044f, -0.89185f, -0.89322f, -0.89459f, -0.89597f, -0.89734f, -0.89868f, -0.90002f, -0.90134f, -0.90268f, -0.90399f, -0.9053f, -0.90659f, -0.9079f, -0.90918f, -0.91043f, -0.91171f, -0.91296f,
-0.91422f, -0.91544f, -0.91669f, -0.91791f, -0.91913f, -0.92032f, -0.92151f, -0.9227f, -0.92389f, -0.92505f, -0.92621f, -0.92737f, -0.9285f, -0.92966f, -0.93076f, -0.93188f, -0.93298f, -0.93408f, -0.93518f, -0.93628f,
-0.93735f, -0.93842f, -0.93945f, -0.94052f, -0.94156f, -0.94257f, -0.9436f, -0.94461f, -0.94562f, -0.94659f, -0.9476f, -0.94855f, -0.94952f, -0.9505f, -0.95145f, -0.95236f, -0.95331f, -0.95422f, -0.95514f, -0.95605f,
-0.95694f, -0.95782f, -0.95871f, -0.95956f, -0.96042f, -0.96127f, -0.96213f, -0.96295f, -0.96378f, -0.9646f, -0.96539f, -0.96619f, -0.96698f, -0.96774f, -0.96854f, -0.96927f, -0.97003f, -0.97076f, -0.9715f, -0.97223f,
-0.97293f, -0.97363f, -0.97433f, -0.97504f, -0.97571f, -0.97638f, -0.97702f, -0.97769f, -0.97833f, -0.97894f, -0.97958f, -0.98019f, -0.98077f, -0.98138f, -0.98196f, -0.98254f, -0.98309f, -0.98367f, -0.98422f, -0.98474f,
-0.98529f, -0.98581f, -0.9863f, -0.98682f, -0.9873f, -0.98779f, -0.98825f, -0.98871f, -0.98917f, -0.98962f, -0.99005f, -0.99048f, -0.99091f, -0.9913f, -0.9917f, -0.9921f, -0.99249f, -0.99286f, -0.99323f, -0.99356f,
-0.9939f, -0.99423f, -0.99457f, -0.99487f, -0.99518f, -0.99548f, -0.99576f, -0.99603f, -0.99631f, -0.99658f, -0.99683f, -0.99707f, -0.99728f, -0.9975f, -0.99771f, -0.99792f, -0.99811f, -0.99829f, -0.99847f, -0.99863f,
-0.99881f, -0.99893f, -0.99908f, -0.99921f, -0.99933f, -0.99942f, -0.99954f, -0.99963f, -0.99969f, -0.99976f, -0.99982f, -0.99988f, -0.99994f, -0.99997f, -0.99997f, -1.0f, -1.0f, -1.0f, -0.99997f, -0.99997f,
-0.99994f, -0.99988f, -0.99982f, -0.99976f, -0.99969f, -0.99963f, -0.99954f, -0.99942f, -0.99933f, -0.99921f, -0.99908f, -0.99893f, -0.99881f, -0.99863f, -0.99847f, -0.99829f, -0.99811f, -0.99792f, -0.99771f, -0.9975f,
-0.99728f, -0.99707f, -0.99683f, -0.99658f, -0.99631f, -0.99603f, -0.99576f, -0.99548f, -0.99518f, -0.99487f, -0.99457f, -0.99423f, -0.9939f, -0.99356f, -0.99323f, -0.99286f, -0.99249f, -0.9921f, -0.9917f, -0.9913f,
-0.99091f, -0.99048f, -0.99005f, -0.98962f, -0.98917f, -0.98871f, -0.98825f, -0.98779f, -0.9873f, -0.98682f, -0.9863f, -0.98581f, -0.98529f, -0.98474f, -0.98422f, -0.98367f, -0.98309f, -0.98254f, -0.98196f, -0.98138f,
-0.98077f, -0.98019f, -0.97958f, -0.97894f, -0.97833f, -0.97769f, -0.97702f, -0.97638f, -0.97571f, -0.97504f, -0.97433f, -0.97363f, -0.97293f, -0.97223f, -0.9715f, -0.97076f, -0.97003f, -0.96927f, -0.96854f, -0.96774f,
-0.96698f, -0.96619f, -0.96539f, -0.9646f, -0.96378f, -0.96295f, -0.96213f, -0.96127f, -0.96042f, -0.95956f, -0.95871f, -0.95782f, -0.95694f, -0.95605f, -0.95514f, -0.95422f, -0.95331f, -0.95236f, -0.95145f, -0.9505f,
-0.94952f, -0.94855f, -0.9476f, -0.94659f, -0.94562f, -0.94461f, -0.9436f, -0.94257f, -0.94156f, -0.94052f, -0.93945f, -0.93842f, -0.93735f, -0.93628f, -0.93518f, -0.93408f, -0.93298f, -0.93188f, -0.93076f, -0.92966f,
-0.9285f, -0.92737f, -0.92621f, -0.92505f, -0.92389f, -0.9227f, -0.92151f, -0.92032f, -0.91913f, -0.91791f, -0.91669f, -0.91544f, -0.91422f, -0.91296f, -0.91171f, -0.91043f, -0.90918f, -0.9079f, -0.90659f, -0.9053f,
-0.90399f, -0.90268f, -0.90134f, -0.90002f, -0.89868f, -0.89734f, -0.89597f, -0.89459f, -0.89322f, -0.89185f, -0.89044f, -0.88904f, -0.88763f, -0.88623f, -0.8848f, -0.88336f, -0.88193f, -0.88046f, -0.87903f, -0.87753f,
-0.87607f, -0.8746f, -0.87311f, -0.87158f, -0.87009f, -0.86856f, -0.86703f, -0.86551f, -0.86398f, -0.86243f, -0.86087f, -0.85931f, -0.85773f, -0.85614f, -0.85455f, -0.85297f, -0.85135f, -0.84973f, -0.84811f, -0.8465f,
-0.84485f, -0.8432f, -0.84155f, -0.8399f, -0.83823f, -0.83655f, -0.83487f, -0.83316f, -0.83148f, -0.82977f, -0.82803f, -0.82632f, -0.82458f, -0.82285f, -0.82111f, -0.81934f, -0.8176f, -0.81583f, -0.81403f, -0.81226f,
-0.81046f, -0.80865f, -0.80685f, -0.80502f, -0.80322f, -0.80139f, -0.79953f, -0.7977f, -0.79584f, -0.79398f, -0.79211f, -0.79022f, -0.78836f, -0.78647f, -0.78455f, -0.78265f, -0.78073f, -0.77881f, -0.77689f, -0.77496f,
-0.77301f, -0.77106f, -0.7691f, -0.76715f, -0.76517f, -0.76318f, -0.7612f, -0.75922f, -0.7572f, -0.75522f, -0.7532f, -0.75116f, -0.74915f, -0.7471f, -0.74506f, -0.74301f, -0.74097f, -0.73889f, -0.73682f, -0.73474f,
-0.73267f, -0.73056f, -0.72845f, -0.72635f, -0.72424f, -0.72214f, -0.72f, -0.71786f, -0.71573f, -0.71359f, -0.71143f, -0.70929f, -0.70712f, -0.70493f, -0.70276f, -0.70056f, -0.69839f, -0.69617f, -0.69397f, -0.69177f,
-0.68954f, -0.68732f, -0.68509f, -0.68286f, -0.6806f, -0.67834f, -0.67609f, -0.67383f, -0.67157f, -0.66928f, -0.66699f, -0.6647f, -0.66241f, -0.66013f, -0.65781f, -0.65549f, -0.65317f, -0.65085f, -0.64853f, -0.64618f,
-0.64383f, -0.64148f, -0.63913f, -0.63678f, -0.6344f, -0.63202f, -0.62964f, -0.62726f, -0.62488f, -0.62247f, -0.62006f, -0.61765f, -0.61523f, -0.61282f, -0.61038f, -0.60794f, -0.60553f, -0.60306f, -0.60062f, -0.59818f,
-0.5957f, -0.59323f, -0.59076f, -0.58829f, -0.58582f, -0.58331f, -0.58081f, -0.57831f, -0.57581f, -0.5733f, -0.5708f, -0.56827f, -0.56573f, -0.5632f, -0.56067f, -0.55814f, -0.55557f, -0.55301f, -0.55048f, -0.54791f,
-0.54532f, -0.54276f, -0.54019f, -0.5376f, -0.535f, -0.53241f, -0.52982f, -0.52719f, -0.5246f, -0.52197f, -0.51935f, -0.51675f, -0.5141f, -0.51147f, -0.50885f, -0.5062f, -0.50354f, -0.50089f, -0.49823f, -0.49557f,
-0.49292f, -0.49023f, -0.48755f, -0.48486f, -0.48218f, -0.47949f, -0.47681f, -0.47412f, -0.47141f, -0.46869f, -0.46597f, -0.46326f, -0.46054f, -0.45782f, -0.45508f, -0.45236f, -0.44962f, -0.44687f, -0.44412f, -0.44138f,
-0.43863f, -0.43585f, -0.43311f, -0.43033f, -0.42755f, -0.42477f, -0.422f, -0.41922f, -0.41644f, -0.41364f, -0.41086f, -0.40805f, -0.40524f, -0.40244f, -0.39963f, -0.39682f, -0.39401f, -0.39117f, -0.38837f, -0.38553f,
-0.38269f, -0.37985f, -0.37701f, -0.37418f, -0.37134f, -0.36847f, -0.36563f, -0.36276f, -0.35989f, -0.35706f, -0.35419f, -0.35129f, -0.34842f, -0.34555f, -0.34268f, -0.33978f, -0.33691f, -0.33401f, -0.33112f, -0.32822f,
-0.32532f, -0.32242f, -0.31952f, -0.31659f, -0.31369f, -0.31079f, -0.30786f, -0.30493f, -0.302f, -0.2991f, -0.29617f, -0.29324f, -0.29028f, -0.28735f, -0.28442f, -0.28146f, -0.27853f, -0.27557f, -0.27264f, -0.26968f,
-0.26672f, -0.26376f, -0.2608f, -0.25784f, -0.25488f, -0.25192f, -0.24893f, -0.24597f, -0.24298f, -0.24002f, -0.23703f, -0.23404f, -0.23108f, -0.22809f, -0.2251f, -0.22211f, -0.21912f, -0.21613f, -0.21313f, -0.21011f,
-0.20712f, -0.20413f, -0.20111f, -0.19812f, -0.1951f, -0.19208f, -0.18909f, -0.18607f, -0.18304f, -0.18002f, -0.177f, -0.17401f, -0.17096f, -0.16794f, -0.16492f, -0.1619f, -0.15887f, -0.15585f, -0.1528f, -0.14978f,
-0.14673f, -0.14371f, -0.14069f, -0.13763f, -0.13458f, -0.13156f, -0.12851f, -0.12546f, -0.12244f, -0.11938f, -0.11633f, -0.11328f, -0.11023f, -0.10718f, -0.10413f, -0.10107f, -0.09802f, -0.09497f, -0.09192f, -0.08887f,
-0.08582f, -0.08276f, -0.07968f, -0.07663f, -0.07358f, -0.07053f, -0.06744f, -0.06439f, -0.06134f, -0.05826f, -0.05521f, -0.05215f, -0.04907f, -0.04602f, -0.04294f, -0.03989f, -0.03683f, -0.03375f, -0.0307f, -0.02762f,
-0.02457f, -0.02148f, -0.01843f, -0.01535f, -0.0123f, -0.00922f, -0.00616f, -0.00308f
};

#ifdef __cplusplus
}
#endif
