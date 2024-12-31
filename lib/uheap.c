#include <inc/lib.h>

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//=============================================
// [1] CHANGE THE BREAK LIMIT OF THE USER HEAP:
//=============================================
/*2023*/
void* sbrk(int increment)
{
	return (void*) sys_sbrk(increment);
}


uint32 user_pages[NUM_OF_UHEAP_PAGES]={0};
uint32 FF_implementation(int num_of_pages) {

	uint32 itr_index = ((myEnv->env_Limit+ PAGE_SIZE) - (uint32)USER_HEAP_START) / (uint32)PAGE_SIZE;
	uint32 end = ((uint32)USER_HEAP_MAX - (uint32)USER_HEAP_START) / PAGE_SIZE;
	bool exist = 0;
	int found_pages = 0;
	uint32 va;
	while(itr_index < end){

		if(user_pages[itr_index] == 0){
			found_pages++;
		}
		else{
			found_pages = 0;
			itr_index += user_pages[itr_index];
			itr_index--;
		}
		if(found_pages == 1)
			{
			va = USER_HEAP_START + (itr_index * PAGE_SIZE);}

		if(found_pages == num_of_pages){
			exist = 1;
			break;
		}
		itr_index ++;
	}

	if(exist == 0)
			return 0;

		uint32 i = ((uint32)va - (uint32)USER_HEAP_START) / (uint32)PAGE_SIZE;
		user_pages[i] = num_of_pages;
		return va;
}

void* malloc(uint32 size)
{
	    //==============================================================
		//DON'T CHANGE THIS CODE========================================
		if (size == 0) return NULL ;
		//==============================================================
		//TODO: [PROJECT'24.MS2 - #12] [3] USER HEAP [USER SIDE] - malloc()
		// Write your code here, remove the panic and write your code

		uint32 new_size = ROUNDUP(size,PAGE_SIZE);
		uint32 num_of_pages = new_size / (uint32)PAGE_SIZE;
		uint32 va;
		if(size <= DYN_ALLOC_MAX_BLOCK_SIZE)
		{
				return alloc_block_FF(size);
		}
			if(sys_isUHeapPlacementStrategyFIRSTFIT())
			{
		           va = FF_implementation(num_of_pages);
		           if(va == 0)
				{
					//cprintf(" mallloc va = 0 here \n");
					return NULL;
				}
		    	    sys_allocate_user_mem(va,(uint32)new_size);
		    	    return (void*)va;
			}
			return NULL;
}

//=================================
// [3] FREE SPACE FROM USER HEAP:
//=================================
void free(void* virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #14] [3] USER HEAP [USER SIDE] - free()
	// Write your code here, remove the panic and write your code

    if (virtual_address == NULL)
    	return;

    //dynamic allocator range
    if ((uint32)virtual_address >= USER_HEAP_START &&
    		(uint32)virtual_address < (uint32)myEnv->env_Limit) {
        free_block(virtual_address);
    }
    //page allocator range

    if ((uint32)virtual_address >= ((uint32)myEnv->env_Limit + PAGE_SIZE) &&
        (uint32)virtual_address < USER_HEAP_MAX)
    {
    	//cprintf(" in free page allocator\n");
        int page_index = ((uint32)virtual_address - USER_HEAP_START) / PAGE_SIZE;

        if (user_pages[page_index] > 0) {
            sys_free_user_mem((uint32)virtual_address, user_pages[page_index] * PAGE_SIZE);

            int remaining_pages = user_pages[page_index];
            int index = 0;
            //Reset page
            while (index < remaining_pages) {

                user_pages[page_index + index] = 0;
                index++;
            }
        }
        return;
    }

}


//=================================
// [4] ALLOCATE SHARED VARIABLE:
//=================================

void* smalloc(char *sharedVarName, uint32 size, uint8 isWritable)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	if (size == 0) return NULL ;
	//==============================================================
	//TODO: [PROJECT'24.MS2 - #18] [4] SHARED MEMORY [USER SIDE] - smalloc()
	// Write your code here, remove the panic and write your code

	    uint32 new_size = ROUNDUP(size, PAGE_SIZE);
	    uint32 num_pages = new_size / PAGE_SIZE;
	    uint32 va ;

	    // Apply the user heap placement strategy
	    if (sys_isUHeapPlacementStrategyFIRSTFIT()) {
	        va = FF_implementation(num_pages);
	        //cprintf("virtual address is %x\n",va);
	    if (va == 0)
	    {
	        //cprintf(" no space for shared allocation\n");
	        return NULL;
	    }
	    int result = sys_createSharedObject(sharedVarName, new_size, isWritable, (void*)va);
	    if (result < 0)
	    {
	        //cprintf("resulttttt\n", result);
	        return NULL;
	    }
	  }
	    return (void*)va;
}


//========================================
// [5] SHARE ON ALLOCATED SHARED VARIABLE:
//========================================


void* sget(int32 ownerEnvID, char *sharedVarName)
{
	//TODO: [PROJECT'24.MS2 - #20] [4] SHARED MEMORY [USER SIDE] - sget()
	// Write your code here, remove the panic and write your code
	//panic("sget() is not implemented yet...!!");

	uint32 size = sys_getSizeOfSharedObject(ownerEnvID, sharedVarName);

    if (size == E_SHARED_MEM_NOT_EXISTS)
    {
    	return NULL;
    }
    uint32 np = ROUNDUP(size, PAGE_SIZE);
	    if (sys_isUHeapPlacementStrategyFIRSTFIT()) {
	         struct BlockElement* virtual_address = malloc(np);
	    if (virtual_address == 0)
	    {
	        //cprintf(" return here1 \n");
	    }

        //  get the shared object
        int result = sys_getSharedObject(ownerEnvID, sharedVarName, virtual_address);

        // Return the virtual address if success, or  NULL
        return (result < 0) ? NULL : virtual_address;
	    }
	return NULL;
}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//=================================
// FREE SHARED VARIABLE:
//=================================
//	This function frees the shared variable at the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND "EMPTY" PAGE TABLES
//	from main memory then switch back to the user again.
//
//	use sys_freeSharedObject(...); which switches to the kernel mode,
//	calls freeSharedObject(...) in "shared_memory_manager.c", then switch back to the user mode here
//	the freeSharedObject() function is empty, make sure to implement it.

void sfree(void* virtual_address)
{
	//TODO: [PROJECT'24.MS2 - BONUS#4] [4] SHARED MEMORY [USER SIDE] - sfree()
	// Write your code here, remove the panic and write your code
	panic("sfree() is not implemented yet...!!");
}


//=================================
// REALLOC USER SPACE:
//=================================
//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to malloc().
//	A call with new_size = zero is equivalent to free().

//  Hint: you may need to use the sys_move_user_mem(...)
//		which switches to the kernel mode, calls move_user_mem(...)
//		in "kern/mem/chunk_operations.c", then switch back to the user mode here
//	the move_user_mem() function is empty, make sure to implement it.
void *realloc(void *virtual_address, uint32 new_size)
{
	//[PROJECT]
	// Write your code here, remove the panic and write your code
	panic("realloc() is not implemented yet...!!");
	return NULL;

}


//==================================================================================//
//========================== MODIFICATION FUNCTIONS ================================//
//==================================================================================//

void expand(uint32 newSize)
{
	panic("Not Implemented");

}
void shrink(uint32 newSize)
{
	panic("Not Implemented");

}
void freeHeap(void* virtual_address)
{
	panic("Not Implemented");

}
