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
    "team_2",
    /* First member's full name */
    "LimDongho",
    /* First member's email address */
    "lopm123456789@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
static void *coalesce(void *ptr); /* 연결시킴 페이로드 포인터 반환하며, 헤더와 푸터 삽입 */
static void *extend_heap(size_t size);/* 힙을 chunksize만큼 확장 확장 후 포인터 반환하는데 헤더에 끝부분까지 사이즈 적어놓음 */
static void *find_fit(size_t size);/* 알맞은 자리 찾을때까지 for문 돌림 없으면 NULL */
static void place(void* ptr,size_t size);/* 헤더와 푸터에 사이즈 정보 전달하고 뒤에 남은 데이터 자름 남은 데이터가16byte 이상일 경우 가용헤더 푸터 생성 */

static char *bp;
static char *heaplist_prologue;
#define ALIGNMENT 8
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE 4096
#define MAX_HEAP (20*(1<<20))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#define PUT(pointer, value) ((*(unsigned int *)(pointer)) = (value))/* 해당포인터에 값을 대입함 */
#define PACK(size, alloc) ((size) + (alloc))/* 할당여부 포함해서 헤더에 들어갈 값(블록의 크기 + 할당여부)을 계산함 */
#define GET(pointer) (*(unsigned int *)(pointer))/* 해당 포인터에 들어가있는 값을 구함(헤더 : 블록의 크기 + 할당여부, 페이로드 : 데이터, 푸터 : 블록의 크기 + 할당여부) */
#define GET_ALLOC(pointer) ((*(unsigned int *)(pointer)) % 2)/* 할당여부 확인 */
#define GET_SIZE(pointer) (*(unsigned int *)(pointer) % 2 ? (*(unsigned int *)(pointer) - 1) : (*(unsigned int *)(pointer)))/* 해당되는 블록의 크기 확인 */
#define HDRP(pointer) ((char *)(pointer) - WSIZE)/* 헤더포인터의 주소 구함 */
#define FTRP(pointer) ((char *)(pointer) + GET_SIZE(HDRP(pointer)) - DSIZE)/* 푸터포인터의 주소 구함 주의할점은 푸터포인터의 시작주소 */
#define NEXT_BLKP(pointer) (FTRP(pointer) + DSIZE)/* 다음 페이로드의 시작점 */
#define PREV_BLKP(pointer) ((char *)(pointer) - GET_SIZE(HDRP(pointer)-WSIZE))/* 그 전 페이로드의 시작점 */


/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)/* 패딩을 추가해서 데이터블록을 8의 배수로 맞춤 & ~0x7: 맨 아래 3비트를 0으로 지움 → +7후 8의 배수로 올림*/

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    char *unused_data = mem_sbrk(WSIZE);
    if (unused_data == (void*)-1)
        return -1;
    char *prologue = mem_sbrk(DSIZE);
    if (prologue == (void*)-1)
        return -1;
    PUT(prologue, PACK(DSIZE, 1));
    PUT(prologue + WSIZE, PACK(DSIZE, 1));
    char *epilogue = mem_sbrk(WSIZE);
    if (epilogue == (void*)-1)
        return -1;
    PUT(epilogue, PACK(0, 1));
    heaplist_prologue = WSIZE + prologue;
    return 0;
}



void *coalesce(void *ptr)
{
    if (((GET_ALLOC(HDRP(NEXT_BLKP(ptr)))) == 0) && (GET_ALLOC(FTRP(PREV_BLKP(ptr)))) == 0)
    {
        PUT(HDRP(PREV_BLKP(ptr)), (GET_SIZE(HDRP((ptr))) + GET_SIZE(FTRP(PREV_BLKP(ptr))) + GET_SIZE(HDRP(NEXT_BLKP((ptr))))));
        PUT(FTRP(NEXT_BLKP(ptr)), GET_SIZE(HDRP(PREV_BLKP(ptr))));
        ptr = PREV_BLKP(ptr);
        return ptr;
    }
    else if (((GET_ALLOC(HDRP(NEXT_BLKP(ptr)))) == 1) && (GET_ALLOC(FTRP(PREV_BLKP(ptr)))) == 1)
    {
        return ptr;
    }
    else if (((GET_ALLOC(HDRP(NEXT_BLKP(ptr)))) == 1) && (GET_ALLOC(FTRP(PREV_BLKP(ptr)))) == 0)
    {
        PUT(HDRP(PREV_BLKP(ptr)), (GET_SIZE(HDRP((ptr))) + GET_SIZE(FTRP(PREV_BLKP(ptr)))));
        PUT(FTRP(ptr), GET_SIZE(HDRP(PREV_BLKP(ptr))));
        ptr = PREV_BLKP(ptr);
        return ptr;
    }
    else
    {
        PUT(HDRP(ptr), (GET_SIZE(HDRP((ptr))) + GET_SIZE(HDRP(NEXT_BLKP((ptr))))));
        PUT(FTRP(ptr), GET_SIZE(HDRP(ptr)));
        return ptr;
    }
}
    void *extend_heap(size_t size)
{
    size_t remained_size = MAX_HEAP-mem_heapsize();
    void *ptr = mem_sbrk(MAX(CHUNKSIZE, size)); /* 나중에 chunksize 줄이는것도 생각해보기 */
    if(ptr == (void*)-1)
    {
        if(remained_size > 2 * DSIZE && remained_size >= size)
        {
            ptr = mem_sbrk(remained_size);
            PUT(HDRP(ptr),remained_size);
            PUT(FTRP(ptr),remained_size);
            PUT(HDRP(NEXT_BLKP(ptr)),PACK(0,1));
            return coalesce(ptr);
        }
        else
        {
        return NULL;
        }
    }
    PUT(HDRP(ptr),MAX(CHUNKSIZE, size));
    PUT(FTRP(ptr),MAX(CHUNKSIZE, size));
    PUT(HDRP(NEXT_BLKP(ptr)),PACK(0,1));
    return coalesce(ptr);/* 이게 최적화에 더좋은지는 나중에 생각해보기 */
}
/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void place(void* ptr,size_t size)
{
    size_t split_size = GET_SIZE(HDRP(ptr)) - size;
    if(split_size < 2 * DSIZE)
    {
    PUT(HDRP((ptr)),PACK(GET_SIZE(HDRP(ptr)),1));
    PUT(FTRP((ptr)),PACK(GET_SIZE(HDRP(ptr)),1));
    }
    else
    {
    PUT(HDRP(ptr),PACK(size,1));
    PUT(FTRP(ptr),PACK(size,1));
    PUT(HDRP(NEXT_BLKP(ptr)),PACK(split_size,0));
    PUT(FTRP(NEXT_BLKP(ptr)),PACK(split_size,0));
    ptr = NEXT_BLKP(ptr);
    }
}
void *find_fit(size_t size)
{
    for(void* i = heaplist_prologue; GET_SIZE(HDRP(i)) != 0; i = NEXT_BLKP(i))
    {
        if(GET_ALLOC(HDRP(i)) == 0 && size <= GET_SIZE(HDRP(i)))
        {
            return i;
        }
    }
    return NULL;
}

void *mm_malloc(size_t size)
{
    void *ptr;
    size_t asize;
    if(size == 0)
        return NULL;
    if(size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE + ALIGN(size);
    ptr = find_fit(asize);
    if(ptr == NULL)
    {
        ptr = extend_heap(asize);
        if(ptr == NULL)
            return NULL;
    }
    place(ptr, asize);
    return ptr;
}
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size = ALIGN(size);
    if (ptr == NULL)
    {
        ptr = mm_malloc(size);
        return ptr;
    }
    if (size == 0)
    {
        mm_free(ptr);
        return NULL;
    }
    size_t asize = size + DSIZE;
    size_t copySize = (GET_SIZE(HDRP(ptr)) - DSIZE);
    size_t total = GET_SIZE(HDRP(ptr));
    if (GET_SIZE(HDRP(ptr)) < asize)
    {
        if (GET_ALLOC(HDRP(NEXT_BLKP(ptr))) == 0)
            total += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        if (GET_ALLOC(FTRP(PREV_BLKP(ptr))) == 0)
            total += GET_SIZE(FTRP(PREV_BLKP(ptr)));
        if (total >= asize)
        {
            ptr = coalesce(ptr);
            memmove(ptr, oldptr, copySize);
            place(ptr, asize);
            return ptr;
        }
        newptr = mm_malloc(size);
        if (newptr == NULL)
            return NULL;
        memmove(newptr, ptr, copySize);
        mm_free(ptr);
        return newptr; 
    }
    else
    {
        place(ptr, asize);
        if (GET_ALLOC(HDRP(NEXT_BLKP(ptr))) == 0)
            coalesce(NEXT_BLKP(ptr));
        return ptr;
    }
}