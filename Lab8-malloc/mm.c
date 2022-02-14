/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};
/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12) /* Extend heap by this amount (bytes) */
#define MINBLOCKSIZE 16

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc)) /* Pack a size and allocated bit into a word */

#define GET(p) (*(unsigned int *)(p)) /* read a word at address p */
#define PUT(p, val) (*(unsigned int *)(p) = (val)) /* write a word at address p */

#define GET_SIZE(p) (GET(p) & ~0x7) /* read the size field from address p */
#define GET_ALLOC(p) (GET(p) & 0x1) /* read the alloc field from address p */

#define HP(bp) ((char*)(bp) - WSIZE) /* given block ptr bp, compute address of its header */
#define FP(bp) ((char*)(bp) + GET_SIZE(HP(bp)) - DSIZE) /* given block ptr bp, compute address of its footer */

#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(HP(bp))) /* given block ptr bp, compute address of next blocks */
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE((char*)(bp) - DSIZE)) /* given block ptr bp, compute address of prev blocks */

#define PREV(bp) ((char*)(bp))
#define NEXT(bp) ((char*)(bp) + WSIZE)


static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t size);
static void place(void *bp,size_t asize);
inline void insert_to_list(char *p);
inline void remove_from_list(char *p);
inline char *find_list_head(size_t size);

static char *heap_listp;
static char *block_list_head;

/*
 * mm_init - initialize the malloc package.
 *      The return value should be -1 if there was a problem in performing the initialization, 0 otherwise
 */
int mm_init(void)
{
    if((heap_listp = mem_sbrk(14 * WSIZE))==(void *)-1) 
        return -1;

    //PUT(heap_listp, 0);                   
    PUT(heap_listp, 0);                   //[0, 64)
    PUT(heap_listp + ( 1 * WSIZE), 0);    //[64, 128)
    PUT(heap_listp + ( 2 * WSIZE), 0);    //[128, 256)
    PUT(heap_listp + ( 3 * WSIZE), 0);    //[256, 512)
    PUT(heap_listp + ( 4 * WSIZE), 0);    //[512, 1024)
    PUT(heap_listp + ( 5 * WSIZE), 0);    //[1024, 1560)
    PUT(heap_listp + ( 6 * WSIZE), 0);    //[1560, 2048)
    PUT(heap_listp + ( 7 * WSIZE), 0);     //[2048, 2880)
    PUT(heap_listp + ( 8 * WSIZE), 0);    //[2880, 4096)
    PUT(heap_listp + ( 9 * WSIZE), 0);    //[4096, ...)
    PUT(heap_listp + (10 * WSIZE), 0);
    PUT(heap_listp + (11 * WSIZE),PACK(DSIZE, 1));
    PUT(heap_listp + (12 * WSIZE),PACK(DSIZE, 1));
    PUT(heap_listp + (13 * WSIZE),PACK(0, 1));

    block_list_head = heap_listp;
    heap_listp += (12 * WSIZE);

    if((extend_heap(CHUNKSIZE / DSIZE)) == NULL) 
        return -1;

    return 0;
}

/*
 * find_list_head - find the head of list fit for size
 */
inline char *find_list_head(size_t size)
{
    int i = 0;
    if(size < 64) i = 0;
    else if(size <  128) i = 1;
    else if(size <  256) i = 2;
    else if(size <  512) i = 3;
    else if(size < 1024) i = 4;
    else if(size < 1560) i = 5;
    else if(size < 2048) i = 6;
    else if(size < 2880) i = 7;
    else if(size < 4096) i = 8;
    else i = 9;
    return block_list_head + (i * WSIZE);
}

/*
 * extend_heap - extend heap by words * word(4 bytes)
 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words & 1) ? (words + 1) * DSIZE : words * DSIZE;

    if((long)(bp = mem_sbrk(size)) == (void *)-1)
        return NULL;

    PUT(HP(bp), PACK(size, 0));
    PUT(FP(bp), PACK(size, 0));
    PUT(NEXT(bp), NULL);
    PUT(PREV(bp), NULL);

    PUT(HP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;
    if(size == 0) return NULL;

    if(size <= DSIZE)
        asize = 2 * (DSIZE);
    else
        asize = (DSIZE) * ((size + (DSIZE) + (DSIZE - 1)) / (DSIZE));

    if((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    if((bp = extend_heap(MAX(asize,CHUNKSIZE) / DSIZE)) == NULL)
    {
        return NULL;
    }
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{

    if(bp == 0)
        return;
    size_t size = GET_SIZE(HP(bp));

    PUT(HP(bp), PACK(size, 0));
    PUT(FP(bp), PACK(size, 0));

    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if(ptr == NULL)
        return mm_malloc(size);
    if(size == 0){
        mm_free(ptr);
        return NULL;
    }

    size_t prev_alloc = GET_ALLOC(HP(PREV_BLKP(ptr)));
    size_t next_alloc = GET_ALLOC(HP(NEXT_BLKP(ptr)));
    size_t old_size = GET_SIZE(HP(ptr));
    size_t asize;
    
    if(size <= DSIZE)
        asize = 2 * (DSIZE);
    else
        asize = (DSIZE) * ((size + (DSIZE) + (DSIZE - 1)) / (DSIZE));
    
    if(old_size == asize)
        return ptr;
    else if(old_size >= asize + 2 * DSIZE){
        PUT(HP(ptr), PACK(asize, 1));
        PUT(FP(ptr), PACK(asize, 1));
        PUT(HP(NEXT_BLKP(ptr)), PACK(old_size - asize, 0));
        PUT(FP(NEXT_BLKP(ptr)), PACK(old_size - asize, 0));
        PUT(NEXT(NEXT_BLKP(ptr)), NULL);
        PUT(PREV(NEXT_BLKP(ptr)), NULL);
        coalesce(NEXT_BLKP(ptr));     
        return ptr;
    }


    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    size = GET_SIZE(HP(oldptr));
    copySize = GET_SIZE(HP(newptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize-WSIZE);
    mm_free(oldptr);
    return newptr;
}

/*
 * coalesce - coalesce the empty block
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HP(bp));

    if(prev_alloc && next_alloc){
        /*
         * do nothing
         */
    }   
    else if(prev_alloc && !next_alloc){
        size += GET_SIZE(HP(NEXT_BLKP(bp)));
        remove_from_list(NEXT_BLKP(bp));
        PUT(HP(bp), PACK(size,0));
        PUT(FP(bp), PACK(size,0));
    }
    else if(!prev_alloc && next_alloc){
        size += GET_SIZE(HP(PREV_BLKP(bp)));
        remove_from_list(PREV_BLKP(bp));
        PUT(FP(bp),PACK(size,0));
        PUT(HP(PREV_BLKP(bp)),PACK(size,0));
        bp = PREV_BLKP(bp);
    }
    else{
        size += GET_SIZE(FP(NEXT_BLKP(bp))) + GET_SIZE(HP(PREV_BLKP(bp)));
        remove_from_list(PREV_BLKP(bp));
        remove_from_list(NEXT_BLKP(bp));
        PUT(FP(NEXT_BLKP(bp)),PACK(size, 0));
        PUT(HP(PREV_BLKP(bp)),PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    insert_to_list(bp);
    return bp;
}

/*
 * insert_to_list - insert bp to the block list
 */
inline void insert_to_list(char *bp)
{
    char* head = find_list_head(GET_SIZE(HP(bp)));
    char* prev = head;
    char* cur = GET(head);

    
    while(cur != NULL){
        if(GET_SIZE(HP(cur)) >= GET_SIZE(HP(bp))) 
            break;
        prev = cur;
        cur = GET(NEXT(cur));
    }
    if(prev == head){
        PUT(head, bp);
        PUT(NEXT(bp), cur);
        PUT(PREV(bp), NULL);
        if(cur != NULL) 
            PUT(PREV(cur), bp);
    }
    else{
        PUT(NEXT(prev), bp);
        PUT(PREV(bp), prev);
        PUT(NEXT(bp), cur);
        if(cur != NULL) 
            PUT(PREV(cur), bp);
    }
}

/*
 * remove_from_list - remove bp from block list
 */
inline void remove_from_list(char *bp){
    char* head = find_list_head(GET_SIZE(HP(bp)));
    char* prev = GET(PREV(bp));
    char *next = GET(NEXT(bp));

    if(prev == NULL){
        if(next != NULL)
            PUT(PREV(next), NULL);
        PUT(head, next);
    }
    else{
        if(next != NULL)
            PUT(PREV(next), prev);
        PUT(NEXT(prev), next);
    }
}

/*
 * find_fit - use first fit strategy to find an empty block.
 */
static void *find_fit(size_t size)
{
    char* head = find_list_head(size);
    char* bp = GET(head);
    for(; head != (heap_listp - (2 * WSIZE)); head += WSIZE){
        char *bp = GET(head);
        while(bp != NULL){
            if(GET_SIZE(HP(bp)) >= size) 
                return bp;
            bp = GET(NEXT(bp));
        }
    }
    return NULL;
}

/*
 * place - remove the block bp from free list,
 *         and only split if the remaining part is equal to or larger than the size of the smallest block
 */
static void place(void *bp,size_t asize)
{
    size_t size = GET_SIZE(HP(bp));
    remove_from_list(bp);   
    if(size - asize >= 2 * DSIZE){
        PUT(HP(bp), PACK(asize, 1));
        PUT(FP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HP(bp),PACK(size - asize, 0));
        PUT(FP(bp),PACK(size - asize, 0));
        coalesce(bp);
    }
    else{
        PUT(HP(bp),PACK(size, 1));
        PUT(FP(bp),PACK(size, 1));
    }
}

