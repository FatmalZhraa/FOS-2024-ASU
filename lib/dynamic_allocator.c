/*
 * dynamic_allocator.c
 *
 *  Created on: Sep 21, 2023
 *      Author: HP
 */
#include <inc/assert.h>
#include <inc/string.h>
#include "../inc/dynamic_allocator.h"


//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//=====================================================
// 1) GET BLOCK SIZE (including size of its meta data):
//=====================================================
__inline__ uint32 get_block_size(void* va)
{
	uint32 *curBlkMetaData = ((uint32 *)va - 1) ;
	return (*curBlkMetaData) & ~(0x1);
}

//===========================
// 2) GET BLOCK STATUS:
//===========================
__inline__ int8 is_free_block(void* va)
{
	uint32 *curBlkMetaData = ((uint32 *)va - 1) ;
	return (~(*curBlkMetaData) & 0x1) ;
}

//===========================
// 3) ALLOCATE BLOCK:
//===========================

void *alloc_block(uint32 size, int ALLOC_STRATEGY)
{
	void *va = NULL;
	switch (ALLOC_STRATEGY)
	{
	case DA_FF:
		va = alloc_block_FF(size);
		break;
	case DA_NF:
		va = alloc_block_NF(size);
		break;
	case DA_BF:
		va = alloc_block_BF(size);
		break;
	case DA_WF:
		va = alloc_block_WF(size);
		break;
	default:
		cprintf("Invalid allocation strategy\n");
		break;
	}
	return va;
}

//===========================
// 4) PRINT BLOCKS LIST:
//===========================

void print_blocks_list(struct MemBlock_LIST list)
{
	cprintf("=========================================\n");
	struct BlockElement* blk ;
	cprintf("\nDynAlloc Blocks List:\n");
	LIST_FOREACH(blk, &list)
	{
		cprintf("(size: %d, isFree: %d)\n", get_block_size(blk), is_free_block(blk)) ;
	}
	cprintf("=========================================\n");

}
//
////********************************************************************************//
////********************************************************************************//

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
#define MIN_BLOCK_SIZE 16
#define HEADER_FOOTER (sizeof(uint32) * 2)
bool is_initialized = 0;
//==================================
// [1] INITIALIZE DYNAMIC ALLOCATOR:
//==================================
void initialize_dynamic_allocator(uint32 daStart, uint32 initSizeOfAllocatedSpace)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		if (initSizeOfAllocatedSpace % 2 != 0) initSizeOfAllocatedSpace++; //ensure it's multiple of 2
		if (initSizeOfAllocatedSpace == 0)
			return ;
		is_initialized = 1;
	}
	//==================================================================================
	//==================================================================================

	//TODO: [PROJECT'24.MS1 - #04] [3] DYNAMIC ALLOCATOR - initialize_dynamic_allocator
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("initialize_dynamic_allocator is not implemented yet");
	//Your Code is Here...
    uint32* daBegain = (uint32*) daStart;
    uint32* daEnd = (uint32*) (daStart + initSizeOfAllocatedSpace - sizeof(uint32));
    *daBegain = 1;
    *daEnd = 1;
    uint32* blockH = (uint32*) (daStart + sizeof(uint32));
    uint32* blockF = (uint32*) (daStart + initSizeOfAllocatedSpace - 2 * sizeof(uint32));
    uint32 blockSize = initSizeOfAllocatedSpace - 2 * sizeof(uint32);
    *blockH = blockSize;
    *blockF = blockSize;
    struct BlockElement* firstFreeBlock = (struct BlockElement*) (daStart + 2 * sizeof(uint32));
    LIST_INIT(&freeBlocksList);
    LIST_INSERT_HEAD(&freeBlocksList, firstFreeBlock);
    firstFreeBlock->prev_next_info.le_prev = NULL;
    firstFreeBlock->prev_next_info.le_next = NULL;

}
//==================================
// [2] SET BLOCK HEADER & FOOTER:
//==================================
void set_block_data(void* va, uint32 totalSize, bool isAllocated)
{
	//TODO: [PROJECT'24.MS1 - #05] [3] DYNAMIC ALLOCATOR - set_block_data
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("set_block_data is not implemented yet");
	//Your Code is Here...
	uint32* blockH = (void *) (int) va- sizeof(uint32);
	uint32* blockF = (void *) (int) va + totalSize - DYN_ALLOC_MIN_BLOCK_SIZE ;
    uint32 blockData = totalSize | (isAllocated ? 1 : 0);
	*blockH = blockData;
	*blockF = blockData;


}

//=========================================
// [3] ALLOCATE BLOCK BY FIRST FIT:
//=========================================
void *alloc_block_FF(uint32 size) {
	    //==================================================================================
		//DON'T CHANGE THESE LINES==========================================================
		//==================================================================================
		{
			if (size % 2 != 0) size++;	//ensure that the size is even (to use LSB as allocation flag)
			if (size < DYN_ALLOC_MIN_BLOCK_SIZE)
				size = DYN_ALLOC_MIN_BLOCK_SIZE ;
			if (!is_initialized)
			{
				uint32 required_size = size + 2*sizeof(int) /*header & footer*/ + 2*sizeof(int) /*da begin & end*/ ;
				uint32 da_start = (uint32)sbrk(ROUNDUP(required_size, PAGE_SIZE)/PAGE_SIZE);
				uint32 da_break = (uint32)sbrk(0);
				initialize_dynamic_allocator(da_start, da_break - da_start);
			}
		}
		//==================================================================================
		//==================================================================================
		//TODO: [PROJECT'24.MS1 - #06] [3] DYNAMIC ALLOCATOR - alloc_block_FF

			if (size == 0) {
				return NULL;
			}

			uint32 required = size + 2 * sizeof(uint32); // Include header and footer
			struct BlockElement* currentBlock = NULL;
			struct BlockElement* try = NULL;

			LIST_FOREACH(currentBlock, &freeBlocksList)
			{
				try = currentBlock;

				if (is_free_block(currentBlock))
				{
					uint32 blockSize = get_block_size(currentBlock);

					if (blockSize >= required)
					{
						uint32 remainingSpace = blockSize - required;

						if (remainingSpace >= (2*HEADER_FOOTER))
						{
							set_block_data(currentBlock, required, 1);
							struct BlockElement* newFreeBlock =
									(struct BlockElement*)((uint32*)currentBlock + required/sizeof(uint32) );
							set_block_data(newFreeBlock, remainingSpace, 0);
							LIST_INSERT_AFTER(&freeBlocksList, currentBlock, newFreeBlock);
							set_block_data(currentBlock, (size + HEADER_FOOTER), 1);
							LIST_REMOVE(&freeBlocksList, currentBlock);
							//cprintf("currentBlock111: %x\n ",currentBlock);

							return try;
						}
						else{
							set_block_data(currentBlock,required + remainingSpace, 1);
							LIST_REMOVE(&freeBlocksList, currentBlock);
							//cprintf("currentBlock222: %x\n ",currentBlock);

						}		return try;
					}
					//return NULL;
				}
	      }
    uint32 sizeAlloc = ROUNDUP(required, PAGE_SIZE);
    struct BlockElement *myBlock = (struct BlockElement *)sbrk(sizeAlloc / PAGE_SIZE);

    if (myBlock == (void *)-1) {
        return NULL; // sbrk failed
    }

    // if last block is free
    struct BlockElement *lastBlock = LIST_LAST(&freeBlocksList);

    if (lastBlock!= NULL)
    {

    	if (is_free_block(lastBlock)) {
			//cprintf("hereeeeeeeee call sbrk at last block :\n");

        uint32 endlastBlock= (uint32)lastBlock + get_block_size(lastBlock);

        if (endlastBlock == (uint32)myBlock) {
            // Merge new with last free
            uint32 newBlockSize = sizeAlloc + get_block_size(lastBlock);
            set_block_data(lastBlock, newBlockSize, 0);

            uint32 *endMark_at = (uint32 *)((uint32)lastBlock + newBlockSize - sizeof(uint32));
            *endMark_at = 0x1;
            myBlock = lastBlock;
        }
        else {
            // no merge
        	set_block_data(myBlock, sizeAlloc, 0);
			uint32* endMark_at = (uint32*)((uint32)myBlock + sizeAlloc - sizeof(uint32));
			*endMark_at = 0x1;
			LIST_INSERT_TAIL(&freeBlocksList, myBlock);
        }
      }
    }

    else {
		//cprintf("here call sbrk last block IS NULLLL:\n");

    	set_block_data(myBlock, sizeAlloc, 0);
		uint32* endMark_at = (uint32*)((uint32)myBlock + sizeAlloc - sizeof(uint32));
		*endMark_at = 0x1;
		LIST_INSERT_TAIL(&freeBlocksList, myBlock);
    }

    uint32 myBloc_Size = get_block_size(myBlock);

     if (myBloc_Size >= required) {
        // Split
        uint32 remaining = myBloc_Size - required;
        if (remaining >= (2*HEADER_FOOTER)) {

            set_block_data(myBlock, required, 1);

            struct BlockElement *newFreeBlock =
            		(struct BlockElement *)((uint32 *)myBlock + required / sizeof(uint32));
            set_block_data(newFreeBlock, remaining, 0);
            LIST_INSERT_AFTER(&freeBlocksList, myBlock, newFreeBlock);
            LIST_REMOVE(&freeBlocksList, myBlock);
        }
        else
        {
        	set_block_data(myBlock, myBloc_Size, 1);
            LIST_REMOVE(&freeBlocksList, myBlock);
        }
        return myBlock;
    }
    return NULL;
}

//=========================================
// [4] ALLOCATE BLOCK BY BEST FIT:
//=========================================
void *alloc_block_BF(uint32 size)
{
	//TODO: [PROJECT'24.MS1 - BONUS] [3] DYNAMIC ALLOCATOR - alloc_block_BF
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	panic("alloc_block_BF is not implemented yet");
	//Your Code is Here...

}

//===================================================
// [5] FREE BLOCK WITH COALESCING:
//===================================================`
void free_block(void *va)
{
	//TODO: [PROJECT'24.MS1 - #07] [3] DYNAMIC ALLOCATOR - free_block
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("free_block is not implemented yet");
	//Your Code is Here...
	if (va == NULL)
	{
		return;
	}
	    uint32* header = (uint32*)((uint32)va - sizeof(uint32));
	    uint32 current_size = *header & ~1;

	    current_size = ROUNDUP(current_size, 2);
	    uint32* footer = (uint32*)((uint32)va + current_size - 2 * sizeof(uint32));

	    *header = current_size;
	    *footer = current_size;

	    uint32* next_header = (uint32*)((uint32)footer + sizeof(uint32));
	    uint32* prev_footer = (uint32*)((uint32)header - sizeof(uint32));

	    void* merged_block_start = va;
	    uint32 total_size = current_size;
	    if ( !(*prev_footer & 1)) //the edit at condition #fatma
	    {
	        uint32 prev_size = *prev_footer & ~1;
	        prev_size = ROUNDUP(prev_size, 2);
	        void* prev_va = (void*)((uint32)va - prev_size);
	        struct BlockElement* prev_block = (struct BlockElement*)prev_va;
	        LIST_REMOVE(&freeBlocksList, prev_block);
	        merged_block_start = prev_va;
	        total_size += prev_size;
	    }

	    if ( !(*next_header & 1)) //edited
	    {
	        uint32 next_size = *next_header & ~1;
	        next_size = ROUNDUP(next_size, 2);
	        void* next_va = (void*)((uint32)next_header + sizeof(uint32));
	        struct BlockElement* next_block = (struct BlockElement*)next_va;
	        LIST_REMOVE(&freeBlocksList, next_block);

	        total_size += next_size;
	    }
	    if(total_size%2!=0)
	    {
	    	total_size++;
	    }

	    if (total_size < MIN_BLOCK_SIZE) {
	        total_size = MIN_BLOCK_SIZE;
	    }

        set_block_data(merged_block_start, total_size, 0);

	    struct BlockElement* block_to_insert = (struct BlockElement*)merged_block_start;

	    struct BlockElement* currentBlock;
	        struct BlockElement* prev = NULL;
	        // search the siutable
	        LIST_FOREACH(currentBlock, &freeBlocksList) {
	            // If we find a block with a higher address, insert before it
	            if (block_to_insert < currentBlock) {
	                // If  have previous block, insert after it
	                if (prev)
	                {
	                    LIST_INSERT_AFTER(&freeBlocksList, prev, block_to_insert);
	                }
	                else {
	                    // If no previous , insert at the head
	                    LIST_INSERT_HEAD(&freeBlocksList, block_to_insert);
	                }
	                return;
	            }
	            prev = currentBlock;
	        }
	        LIST_INSERT_TAIL(&freeBlocksList, block_to_insert);
}

//=========================================
// [6] REALLOCATE BLOCK BY FIRST FIT:
//=========================================
void *realloc_block_FF(void* va, uint32 new_size)
{
	//TODO: [PROJECT'24.MS1 - #08] [3] DYNAMIC ALLOCATOR - realloc_block_FF
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	//panic("realloc_block_FF is not implemented yet");
	//Your Code is Here...
    if (new_size == 0)
    {
        free_block(va);
        return NULL;
    }
    if (va == NULL)
    {
        return alloc_block_FF(new_size);
    }

    uint32 requiredSize = new_size + 2 * sizeof(uint32);
    uint32 current_size = get_block_size(va);

    if (requiredSize == current_size) {
        set_block_data(va, current_size, 1);
        return va;
    }

    if (requiredSize <= current_size) {
        uint32 remaining_size = current_size - requiredSize;

        if (remaining_size >= MIN_BLOCK_SIZE) {
            set_block_data(va, requiredSize, 1);
            uint32* new_free_block = (uint32*)((uint32)va + requiredSize);
            set_block_data(new_free_block, remaining_size, 0);
            LIST_INSERT_HEAD(&freeBlocksList, (struct BlockElement*)new_free_block);
        } else {
            set_block_data(va, current_size, 1);
        }
        return va;
    }

    uint32* next_header = (uint32*)((uint32)va + current_size);

    if ((uint32)next_header < KERNEL_HEAP_MAX && is_free_block(next_header)) {
        uint32 next_size = get_block_size(next_header);

        if (current_size + next_size >= requiredSize) {
            LIST_REMOVE(&freeBlocksList, (struct BlockElement*)next_header);

            set_block_data(va, current_size + next_size, 1);

            uint32 merged_size = current_size + next_size;
            uint32 remaining_size = merged_size - requiredSize;

            if (remaining_size >= MIN_BLOCK_SIZE) {
                uint32* split_block = (uint32*)((uint32)va + requiredSize);
                set_block_data(split_block, remaining_size, 0);
                LIST_INSERT_HEAD(&freeBlocksList, (struct BlockElement*)split_block);
                set_block_data(va, requiredSize, 1);
            }

            return va;
        }
    }
    void* new = alloc_block_FF(new_size);

    if (new != NULL)
    {
    	set_block_data(new ,get_block_size(va),0);

        free_block(va);
        return new;
    }
    return NULL;
}

/*********************************************************************************************/
/*********************************************************************************************/
/*********************************************************************************************/
//=========================================
// [7] ALLOCATE BLOCK BY WORST FIT:
//=========================================
void *alloc_block_WF(uint32 size)
{
	panic("alloc_block_WF is not implemented yet");
	return NULL;
}

//=========================================
// [8] ALLOCATE BLOCK BY NEXT FIT:
//=========================================
void *alloc_block_NF(uint32 size)
{
	panic("alloc_block_NF is not implemented yet");
	return NULL;
}
