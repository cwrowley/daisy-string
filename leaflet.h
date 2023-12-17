/* tiny subset of the LEAF library, for experimenting with interface */
#include <stdlib.h>

#ifndef LEAFLET_INCLUDED
#define LEAFLET_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef float Lfloat;

#define PI              (3.14159265358979f)
#define TWO_PI          (6.28318530717958f)
#define TWO_TO_32        4294967296.0f
#define MPOOL_ALIGN_SIZE (8)

//! Include wave table required to use tCycle.
#define LEAF_INCLUDE_SINE_TABLE 1
#define SINE_TABLE_SIZE 2048
extern const Lfloat __leaf_table_sinewave[SINE_TABLE_SIZE];

// leaf-global.h

typedef struct mpool_node_t mpool_node_t;
typedef struct tMempool tMempool;
typedef struct LEAF LEAF;

// node of free list
struct mpool_node_t {
  void                *pool;     // memory pool field
  mpool_node_t *next;     // next node pointer
  mpool_node_t *prev;     // prev node pointer
  size_t size;
};

struct tMempool {
  tMempool      *mempool;
  LEAF*         leaf;
  void*         mpool;       // start of the mpool
  size_t        usize;       // used size of the pool
  size_t        msize;       // max size of the pool
  mpool_node_t* head;        // first node of memory pool free list
};

typedef enum LEAFErrorType {
  LEAFMempoolOverrun = 0,
  LEAFMempoolFragmentation,
  LEAFInvalidFree,
  LEAFErrorNil
} LEAFErrorType;

struct LEAF {
  ///@{
  Lfloat   sampleRate; //!< The current audio sample rate. Set with LEAF_setSampleRate().
  Lfloat   invSampleRate; //!< The inverse of the current sample rate.
  int     blockSize; //!< The audio block size.
  Lfloat   twoPiTimesInvSampleRate; //!<  Two-pi times the inverse of the current sample rate.
  Lfloat   (*random)(void); //!< A pointer to the random() function provided on initialization.
  int     clearOnAllocation; //!< A flag that determines whether memory allocated from the LEAF memory pool will be cleared.
  tMempool *mempool; //!< Pointer to the default LEAF mempool object.
  tMempool _internal_mempool;
  size_t header_size; //!< The size in bytes of memory region headers within mempools.
  void (*errorCallback)(LEAF* const, LEAFErrorType); //!< A pointer to the callback function for LEAF errors. Can be set by the user.
  int     errorState[LEAFErrorNil]; //!< An array of flags that indicate which errors have occurred.
  unsigned int allocCount; //!< A count of LEAF memory allocations.
  unsigned int freeCount; //!< A count of LEAF memory frees.
  ///@}
};

Lfloat LEAF_clip(Lfloat min, Lfloat val, Lfloat max);

// leaf-mempool.h


LEAF *LEAF_init(Lfloat sampleRate, void *memory, size_t memorySize, Lfloat(*random)(void));
void LEAF_defaultErrorCallback(LEAF* const leaf, LEAFErrorType errorType);
void LEAF_internalErrorCallback(LEAF* const leaf, LEAFErrorType whichone);

    
//==============================================================================


void mpool_create(void *memory, size_t size, tMempool *pool);
    
void *mpool_alloc(size_t size, tMempool *pool);
void *mpool_calloc(size_t asize, tMempool *pool);
    
void mpool_free(void *ptr, tMempool *pool);
    
size_t mpool_get_size(tMempool *pool);
size_t mpool_get_used(tMempool *pool);
    
void leaf_pool_init(LEAF *leaf, void *memory, size_t size);
    
void *leaf_alloc(LEAF *leaf, size_t size);
void *leaf_calloc(LEAF *leaf, size_t size);
    
void leaf_free(LEAF *leaf, void *ptr);
    
size_t leaf_pool_get_size(LEAF *leaf);
size_t leaf_pool_get_used(LEAF *leaf);
    
void *leaf_pool_get_pool(LEAF *leaf);

// leaf-oscillators.h

typedef struct tCycle tCycle;

tCycle *tCycle_new(LEAF *leaf);
tCycle *tCycle_newFromPool(LEAF *leaf, tMempool *pool);
void tCycle_free(tCycle *obj);

Lfloat tCycle_tick(tCycle *obj);
void tCycle_setFreq(tCycle *obj, Lfloat freq);
void tCycle_setPhase(tCycle *obj, Lfloat phase);
void tCycle_setSampleRate(tCycle *obj, Lfloat sr);

#ifdef __cplusplus
}
#endif

#endif

