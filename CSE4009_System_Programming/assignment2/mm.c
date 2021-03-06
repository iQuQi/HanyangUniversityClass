/*
 * mm-implicit.c -  Simple allocator based on implicit free lists,
 *                  first fit placement, and boundary tag coalescing.
 *
 * Each block has header and footer of the form:
 *
 *      31                     3  2  1  0
 *      -----------------------------------
 *     | s  s  s  s  ... s  s  s  0  0  a/f
 *      -----------------------------------
 *
 * where s are the meaningful size bits and a/f is set
 * iff the block is allocated. The list has the following form:
 *
 * begin                                                          end
 * heap                                                           heap
 *  -----------------------------------------------------------------
 * |  pad   | hdr(8:a) | ftr(8:a) | zero or more usr blks | hdr(8:a) |
 *  -----------------------------------------------------------------
 *          |       prologue      |                       | epilogue |
 *          |         block       |                       | block    |
 *
 * The allocated prologue and epilogue blocks are overhead that
 * eliminate edge conditions during coalescing.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

#define WSIZE 4
#define DSIZE 8 
#define CHUNKSIZE (1<<12)

#define MAX(x, y) ((x) > (y)? (x): (y))
#define MIN(x, y) ((x) > (y)? (y): (x))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *) (p))
#define PUT(p, val) (*(unsigned int *) (p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define p_to_char(bp) ((char *) (bp))
#define pAtC(bp) ((char *) (bp + WSIZE))
#define p_char_GET(bp) ((char *) GET(p_to_char(bp)))
#define pAtC_GET(bp) ((char *) GET(pAtC(bp)))


#define HDRP(bp) ((char *) (bp) - WSIZE)
#define FTRP(bp) ((char *) (bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define HDRP(oldbp) ((char *) (oldbp) - WSIZE)
#define FTRP(oldbp) ((char *) (oldbp) + GET_SIZE(HDRP(oldbp)) - DSIZE)

#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(oldbp) ((char *)(oldbp) - GET_SIZE(((char *)(oldbp) - DSIZE)))
#define NEXT_BLKP(oldbp) ((char *)(oldbp) + GET_SIZE(HDRP(oldbp)))

 /* private helper function definitions */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);


#define ClassSize 17 
#define BLOCKSIZE (4*WSIZE) 
#define eight 8
#define sideby(size) (((size) + (eight-1)) & ~0x7)
#define SIZE_T_SIZE (sideby(sizeof(size_t)))

static char *HeapBlocks = 0;
static char **FreeBlocks = 0;
static void free_list_insert(void* bp);
static void free_list_remove(void* bp);


static void put_head_foot(void *bp, size_t size, int t);
static void put(void *bp, unsigned int t);



/*
 * mm_init - Initialize the memory manager
 */
 /* $begin mminit */
int mm_init(void)
{
	/* create the initial empty heap */
	if ((HeapBlocks = mem_sbrk(WSIZE*(ClassSize + 2 + 1))) == (void *)-1)
		return -1;

	memset(HeapBlocks, 0, ClassSize*WSIZE);
	FreeBlocks = (char **)HeapBlocks;


	HeapBlocks += ClassSize * WSIZE;
	PUT(HeapBlocks, PACK(DSIZE, 1));
	PUT(HeapBlocks + (1 * WSIZE), PACK(DSIZE, 1));
	PUT(HeapBlocks + (2 * WSIZE), PACK(0, 1));
	HeapBlocks += (1 * WSIZE);
	/* Extend the empty heap with a free block of CHUNKSIZE bytes */
	if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
		return -1;

	return 0;
}
/* $end mminit */

/*
 * mm_malloc - Allocate a block with at least size bytes of payload
 */
 /* $begin mmmalloc */
void *mm_malloc(size_t size)
{

	char *bp;
	size_t size2;
	size_t newSize;

	//???? ???????? 0?????? ?????????? ??????
	if (size <= 0)
		return NULL;

	//???????? ?????????????? ???? ?????? 
	if (size > DSIZE) size2 = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);
	//?????? ?????? size2?? ???????????? 2??
	else size2 = 2 * DSIZE;

	//?????????????? ?????? ?????? ?????? ??????.
	if ((bp = find_fit(size2)) != NULL) {
		place(bp, size2);
		return bp;
	}

	//?????? ?????? ?????? ?????? ????
	newSize = MAX(size2, CHUNKSIZE);
	//?????? ?????????? null????
	if ((bp = extend_heap(newSize / WSIZE)) == NULL) return NULL;
	//???????? ???????? size2???? alloc
	place(bp, size2);

	return bp;
}
/* $end mmmalloc */

/*
 * mm_free - Free a block
 */
 /* $begin mmfree */
void mm_free(void *bp)
{
	//???????????? ????????
	size_t size = GET_SIZE(HDRP(bp));
	//?????? ?????? ??????????.
	put_head_foot(bp, size, 0);
	//?????? ???? ????
	put(bp, 0);

	//???? ?????? ???????? ??????.
	free_list_insert(coalesce(bp));

}

/* $end mmfree */

/*
 * mm_realloc - naive implementation of mm_realloc
 */
void *mm_realloc(void *oldbp, size_t size)
{
	//???? ??????
	size_t old_size = GET_SIZE(HDRP(oldbp));
	size_t size2, next_block, copy_size;
	//?? ?????? ?????? ??????
	void *newbp;

	//???? ?????????? ???????? ???? ?? ?????? ???? malloc
	if (oldbp == NULL) {
		return mm_malloc(size);
	}

	//???????? 0?????? free?? ????
	if (size == 0) {
		mm_free(oldbp);
		return NULL;
	}

	//?????????? ???? ?????? ?????? 
	if (size <= DSIZE) {
		size2 = 2 * DSIZE;
	}
	//?????????????? ?????? 
	else {
		size2 = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);
	}

	//???? ???????? ???????? ???????? ???? ???????? x ?????? ????
	if (size2 == old_size) {
		return oldbp;
	}

	//?? ?? ???????? ?? ???? ???? ????
	else if (size2 > old_size) {
		//?????? ?????? ?????? ???????????? ????????
		next_block = GET_SIZE(HDRP(NEXT_BLKP(oldbp)));
		//???? ?????? ?????? ???? free ???????? ???????? ?????? ?????? ????
		if (!GET_ALLOC(HDRP(NEXT_BLKP(oldbp)))) {
			if (next_block + old_size >= size2) {
				//?????????? ???? ?????? ?????????????? ???? ?????? ???? ???? ???? ????
				free_list_remove(NEXT_BLKP(oldbp));
				put_head_foot(oldbp, old_size + next_block, 1);
				return oldbp;
			}
		}

		//?????????? free?? ???????? ???????? ???? ?????? ??????????????
		//?????? ?????? ???? ???? ?????? ?????????? ???????? ???????? ?????? ????????.

		//?????? ?????? ?? ????
		newbp = mm_malloc(size);
		//???? ???? : ?????? ????= ???????????? ????????????????
		copy_size = old_size - DSIZE;
		memcpy(newbp, oldbp, copy_size);
		//???? ???? free
		mm_free(oldbp);
		return newbp;
	}

	//?? ???? ???????? ?????? ???? ????
	else {
		//?????????? ?? ?????? ?????????????? ?????? ????????
		if (old_size - size2 >= BLOCKSIZE) {
			//?????? ???????? ???? ????
			put_head_foot(oldbp, size2, 1);
			//next ???? ?????? ???????? ???????? ???? ???? ?? free ???????? ????????.
			newbp = NEXT_BLKP(oldbp);
			put_head_foot(newbp, old_size - size2, 0);
			put(newbp, 0);
			free_list_insert(newbp);
		}

		//?????? ?????? ?????????????? ???? ?????? ???? ????
		return oldbp;
	}
}

/*
 * mm_checkheap - Check the heap for consistency
 *//*
void mm_checkheap(int verbose)
{
	char *bp = heap_listp;

	if (verbose)
	printf("Heap (%p):\n", heap_listp);

	if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp)))
	printf("Bad prologue header\n");
	checkblock(heap_listp);

	for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
	if (verbose)
		printblock(bp);
	checkblock(bp);
	}

	if (verbose)
	printblock(bp);
	if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
	printf("Bad epilogue header\n");
}*/

/* The remaining routines are internal helper routines */

/*
 * extend_heap - Extend heap with free block and return its block pointer
 */
 /* $begin mmextendheap */
static void *extend_heap(size_t words) {
	char *bp;
	size_t size;

	/* Allocate an even number of words to maintain alignment */
	size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
	if ((long)(bp = mem_sbrk(size)) == -1)
		return NULL;

	/* Initialize free block header/footer and the epilogue header */
	PUT(HDRP(bp), PACK(size, 0)); 			/* free block header */
	PUT(FTRP(bp), PACK(size, 0)); 			/* free block footer */
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); 	/* new epilogue header */

/* Coalesce if the previous block was free */
	bp = coalesce(bp);

	free_list_insert(bp);

	return bp;
}
/* $end mmextendheap */

/*
 * place - Place block of asize bytes at start of free block bp
 *         and split if remainder would be at least minimum block size
 */
 /* $begin mmplace */
 /* $begin mmplace-proto */
static void place(void *bp, size_t asize) {
	/* $end mmplace-proto */
	size_t csize = GET_SIZE(HDRP(bp));

	if ((csize - asize) >= BLOCKSIZE) {

		free_list_remove(bp);

		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));

		bp = NEXT_BLKP(bp);
		PUT(HDRP(bp), PACK(csize - asize, 0));
		PUT(FTRP(bp), PACK(csize - asize, 0));

		PUT(p_to_char(bp), 0);
		PUT( pAtC(bp), 0);
		free_list_insert(bp);
	}
	else {
		free_list_remove(bp);
		PUT(HDRP(bp), PACK(csize, 1));
		PUT(FTRP(bp), PACK(csize, 1));
	}
}
/* $end mmplace */

/*
 * find_fit - Find a fit for a block with asize bytes
 */
static void *find_fit(size_t asize) {

	size_t tmp_s = asize;
	int size_class = 0;
	int sumr = 0;
	while (asize > BLOCKSIZE && size_class < ClassSize - 1) {
		size_class++;
		sumr += asize % 2;
		asize /= 2;
	}
	if (size_class < ClassSize - 1 && sumr > 0 && asize == BLOCKSIZE) {
		size_class++;
	}
	
	void *class_p, *bp;
	size_t sizeB;

	/* search from the rover to the end of list */
	while (size_class < ClassSize) {
		class_p = FreeBlocks + size_class;

		/* search from start of list to old rover */
		if (GET(class_p) != 0) {
			bp = (void *)GET(class_p);
			while (bp != ((void *)0)) {
				sizeB = GET_SIZE(HDRP(bp));
				if (tmp_s <= sizeB) {
					return bp;
				}
				bp = pAtC_GET(bp);
			}
		}
		size_class++;
	}

	return NULL; // no fit found
}

/*
 * coalesce - boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce(void *bp) {
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));

	if (prev_alloc && next_alloc) {				/* Case 1 */
		return bp;
	}

	else if (prev_alloc && !next_alloc) {		/* Case 2 */

		free_list_remove(NEXT_BLKP(bp));

		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
	}

	else if (!prev_alloc && next_alloc) {		/* Case 3 */

		free_list_remove(PREV_BLKP(bp));

		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		PUT(FTRP(bp), PACK(size, 0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}

	else {										/* Case 4 */

		free_list_remove(PREV_BLKP(bp));
		free_list_remove(NEXT_BLKP(bp));

		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}

	return bp;
}
/*
static void printblock(void *bp)
{
	size_t hsize, halloc, fsize, falloc;

	hsize = GET_SIZE(HDRP(bp));
	halloc = GET_ALLOC(HDRP(bp));
	fsize = GET_SIZE(FTRP(bp));
	falloc = GET_ALLOC(FTRP(bp));

	if (hsize == 0) {
	printf("%p: EOL\n", bp);
	return;
	}

	printf("%p: header: [%d:%c] footer: [%d:%c]\n", bp,
	   hsize, (halloc ? 'a' : 'f'),
	   fsize, (falloc ? 'a' : 'f'));
}

static void checkblock(void *bp)
{
	if ((size_t)bp % 8)
	printf("Error: %p is not doubleword aligned\n", bp);
	if (GET(HDRP(bp)) != GET(FTRP(bp)))
	printf("Error: header does not match footer\n");
}*/

static void free_list_insert(void *bp) {
	size_t size = GET_SIZE(HDRP(bp));
	char **size_class_ptr;
	unsigned int bp_val = (unsigned int)bp;

	int size_class = 0;
	int sumr = 0;
	while (size > BLOCKSIZE && size_class < ClassSize - 1) {
		size_class++;
		sumr += size % 2;
		size /= 2;
	}
	if (size_class < ClassSize - 1 && sumr > 0 && size == BLOCKSIZE) {
		size_class++;
	}
	

	size_class_ptr = FreeBlocks + size_class;
	if (GET(size_class_ptr) == 0) {
		PUT(size_class_ptr, bp_val);
		PUT(p_to_char(bp), (unsigned int)size_class_ptr);
		PUT( pAtC(bp), 0);
	}

	else {
		PUT(p_to_char(bp), (unsigned int)size_class_ptr);
		PUT( pAtC(bp), GET(size_class_ptr));
		PUT(p_to_char(GET(size_class_ptr)), bp_val);
		PUT(size_class_ptr, bp_val);
	}

}


static void free_list_remove(void *bp) {
	int pre ;

	unsigned int valP = (unsigned int)p_char_GET(bp);
	unsigned int p = (unsigned int)FreeBlocks;
	unsigned int f = p + WSIZE * (ClassSize - 1);

	if (valP > f || valP < p)
		pre= 1;
	else if ((f - valP) % WSIZE)
		pre= 1;
	else pre=0;
	int suc = (pAtC_GET(bp) != (void *)0);

	if (GET_ALLOC(HDRP(bp))) {
		printf("calling free_list_remove on an allocated block\n");
		return;
	}

	if (!pre && suc) {
		PUT(p_char_GET(bp), (unsigned int)pAtC_GET(bp));
		PUT(p_to_char(pAtC_GET(bp)), (unsigned int)p_char_GET(bp));
	}

	else if (!pre && !suc) {
		PUT(p_char_GET(bp), (unsigned int)pAtC_GET(bp));
	}

	else if (pre && suc) {
		PUT(pAtC(p_char_GET(bp)), (unsigned int)pAtC_GET(bp));
		PUT(p_to_char(pAtC_GET(bp)), (unsigned int)p_char_GET(bp));
	}

	else {
		PUT(pAtC(p_char_GET(bp)), 0);
	}

	PUT(p_to_char(bp), 0);
	PUT( pAtC(bp), 0);

}




//?????? ?????? ????????
static void put_head_foot(void *bp, size_t size, int t) {
	PUT(HDRP(bp), PACK(size, t));
	PUT(FTRP(bp), PACK(size, t));

}

//t?? ???? ?????? write
static void put(void *bp, unsigned int t) {
	PUT(p_to_char(bp), t);
	PUT(pAtC(bp), t);

}
