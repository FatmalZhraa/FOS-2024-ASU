#include "kheap.h"

#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include "memory_manager.h"

struct spinlock protec_lock ;

//Initialize the dynamic allocator of kernel heap with the given start address, size & limit
//All pages in the given range should be allocated
//Remember: call the initialize_dynamic_allocator(..) to complete the initialization
//Return:
//	On success: 0
//	Otherwise (if no memory OR initial size exceed the given limit): PANIC

int initialize_kheap_dynamic_allocator(uint32 daStart, uint32 initSizeToAllocate, uint32 daLimit)
{

	init_spinlock(&protec_lock,"kernal protection");
	//TODO: [PROJECT'24.MS2 - #01] [1] KERNEL HEAP - initialize_kheap_dynamic_allocator
	// Write your code here, remove the panic and write your code
	//panic("initialize_kheap_dynamic_allocator() is not implemented yet...!!");

	K_start = ROUNDDOWN(daStart,PAGE_SIZE) ;
	K_hard_limit = daLimit ;
	K_break = (daStart +initSizeToAllocate);

	if (daStart >= K_hard_limit || K_break > K_hard_limit)
	{
		return E_NO_MEM ;
	}

	        uint32 track_ptr = daStart;
			while (track_ptr < K_break)
			{

			struct FrameInfo *ptr_frame_info;
			allocate_frame(& ptr_frame_info);

			ptr_frame_info-> bufferedVA = track_ptr;
			uint32 Assign = map_frame(ptr_page_directory , ptr_frame_info , track_ptr, PERM_WRITEABLE);
			if( Assign == E_NO_MEM )
			{
				  free_frame (ptr_frame_info);
				  return E_NO_MEM;
			}

			track_ptr += PAGE_SIZE;
		}

	initialize_dynamic_allocator(daStart ,initSizeToAllocate);

	return 0;
}



void* sbrk(int numOfPages)
{
	     /*numOfPages > 0: move the segment break of the kernel to increase the size of its heap by the given numOfPages,
		 * 				you should allocate pages and map them into the kernel virtual address space,
		 * 				and returns the address of the previous break (i.e. the beginning of newly mapped memory).
		 * numOfPages = 0: just return the current position of the segment break
		 *
		 * NOTES:
		 * 	1) Allocating additional pages for a kernel dynamic allocator will fail if the free frames are exhausted
		 * 		or the break exceed the limit of the dynamic allocator. If sbrk fails, return -1*/


		//MS2: COMMENT THIS LINE BEFORE START CODING==========
		//return (void*)-1 ;
		//====================================================

		//TODO: [PROJECT'24.MS2 - #02] [1] KERNEL HEAP - sbrk
		// Write your code here, remove the panic and write your code
		//panic("sbrk() is not implemented yet...!!");
		// Return current break if numOfPages is 0

				     if (numOfPages == 0)
				     {
				         return (void*)K_break;
				     }
				     if (numOfPages < 0)
				     {
				         return (void*)-1;
				     }

				     uint32 oldBreak = K_break;
				     uint32 newBreak = ROUNDUP(K_break + (numOfPages * PAGE_SIZE), PAGE_SIZE);

				     // Check if new break exceeds hard limit
				     if (newBreak > K_hard_limit)
				     {
				         return (void*)-1;
				     }
				     uint32 curVA = oldBreak;

				     // Allocate each page from old break to new break
				     while (curVA < newBreak)
				     {
				         struct FrameInfo* ptr_frame_info;
				         int ret = allocate_frame(&ptr_frame_info);

				         if (ret != 0)
				         {
				             // Clean up allocated frames if allocation fails
				             uint32 cleanVa = oldBreak;
				             while (cleanVa < curVA)
				             {
				                 unmap_frame(ptr_page_directory, cleanVa);
				                 cleanVa += PAGE_SIZE;
				             }

				             return (void*)-1;
				         }
				         // Map the frame
				         ptr_frame_info->bufferedVA = curVA;
				         ret = map_frame(ptr_page_directory, ptr_frame_info, curVA, PERM_WRITEABLE);

				         if (ret != 0)
				         {
				             // Clean up if mapping fails
				            // free_frame(ptr_frame_info);

				             uint32 cleanup_va = oldBreak;
				             while (cleanup_va < curVA)
				             {
				            	 ptr_frame_info->bufferedVA = 0;
				                 unmap_frame(ptr_page_directory, cleanup_va);
				                 cleanup_va += PAGE_SIZE;
				             }
				             return (void*)-1;
				         }
				         curVA += PAGE_SIZE;
				     }

				     // Update break only after successful allocation
				     K_break = newBreak;

				     // Set end block at the new break
				     uint32* endMark_at = (uint32 *)(K_break - sizeof(uint32));
				     *endMark_at = 0x1;

				     return (void*)oldBreak;
}

uint32 num = 0 ;
struct PAGES
{
	uint32  numOfPages;
	void * Va;
};

struct PAGES  pages_info[NUM_OF_KHEAP_PAGES] ={0};
void* kmalloc(unsigned int size)
{
	//TODO: [PROJECT'24.MS2 - #03] [1] KERNEL HEAP - kmalloc
	// Write your code here, remove the panic and write your code
	//kpanic_into_prompt("kmalloc() is not implemented yet...!!");

	     acquire_spinlock(&protec_lock);
	     if (size > DYN_ALLOC_MAX_SIZE) {
		        release_spinlock(&protec_lock);

	    	 return NULL;
	    }

	    if (size <= DYN_ALLOC_MAX_BLOCK_SIZE) {
	        void* result = alloc_block_FF(size);
	        release_spinlock(&protec_lock);
	        return result;
	    }
	    size = ROUNDUP(size, PAGE_SIZE);
	    uint32 num_of_pages = size / PAGE_SIZE;
	    uint32 search_ptr = K_hard_limit + PAGE_SIZE;
	    uint32 found_count = 0;

	    while (search_ptr < KERNEL_HEAP_MAX) {
	        uint32* Page_Table;
	        struct FrameInfo* ptr_frame_info = get_frame_info(ptr_page_directory, search_ptr, &Page_Table);

	        if (ptr_frame_info == NULL) {
	            found_count++;

	            if (found_count == num_of_pages) {
	                search_ptr -= (num_of_pages - 1) * PAGE_SIZE;
	                uint32 var = search_ptr;

	                for (uint32 i = 0; i < num_of_pages; i++) {
	                    struct FrameInfo* ptr_frame_info;
	                    if (allocate_frame(&ptr_frame_info) != 0 ||
	                        map_frame(ptr_page_directory, ptr_frame_info, var, PERM_WRITEABLE) == E_NO_MEM) {
	            	        release_spinlock(&protec_lock);
	                    	return NULL;
	                    }
	                    ptr_frame_info->bufferedVA = var;
	                    var += PAGE_SIZE;
	                }
	                pages_info[num].Va = (void*)search_ptr;
	                pages_info[num].numOfPages = num_of_pages;
	                num++;
	                release_spinlock(&protec_lock);
	                return (void*)search_ptr;
	            }
	        } else {
	            found_count = 0;
	        }
	        search_ptr += PAGE_SIZE;
	    }
        release_spinlock(&protec_lock);
	    return NULL;
}

void kfree(void* virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #04] [1] KERNEL HEAP - kfree
	// Write your code here, remove the panic and write your code

	    acquire_spinlock(&protec_lock);
	    uint32 v_address = (uint32)virtual_address;
	    uint32 unmapped_pages = 0;
	    int index = -1;

	    if ((uint32)virtual_address >= K_start && (uint32)virtual_address < K_break) {
	        free_block(virtual_address);
	        release_spinlock(&protec_lock);
	        return;
	    }

	    for (int i = 0; i < NUM_OF_KHEAP_PAGES; i++) {
	        if (pages_info[i].Va == virtual_address) {
	            index = i;
	            unmapped_pages = pages_info[i].numOfPages;
	            break;
	        }
	    }

	    if (index == -1) {
	        release_spinlock(&protec_lock);
	    	return;
	    }

	    uint32 count = 0;
	    while (count < unmapped_pages) {
	        uint32* ptr_Page_table;
	        get_frame_info(ptr_page_directory, v_address, &ptr_Page_table);
	        unmap_frame(ptr_page_directory, v_address);
	        v_address += PAGE_SIZE;
	        count++;
	    }
	    pages_info[index].Va = 0;
	    pages_info[index].numOfPages = 0;
	    release_spinlock(&protec_lock);
}

unsigned int kheap_physical_address(unsigned int virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #05] [1] KERNEL HEAP - kheap_physical_address
	// Write your code here, remove the panic and write your code
	//panic("kheap_physical_address() is not implemented yet...!!");

	//return the physical address corresponding to given virtual_address
	//refer to the project presentation and documentation for details
	//EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED ==================

	uint32 *ppt = NULL;

   	uint32 offset= virtual_address % PAGE_SIZE;
	get_page_table(ptr_page_directory,(uint32)virtual_address,&ppt);
	struct FrameInfo *pfi = get_frame_info(ptr_page_directory,(uint32)virtual_address,&ppt);
	if(pfi==NULL)
	{
		return 0;
	}
	else
	{
	    uint32 frame_number = to_frame_number(pfi);
	   unsigned int physical_address=(frame_number*PAGE_SIZE)+offset;
	   return physical_address;

	}

}

unsigned int kheap_virtual_address(unsigned int physical_address)
{
	//TODO: [PROJECT'24.MS2 - #06] [1] KERNEL HEAP - kheap_virtual_address
	// Write your code here, remove the panic and write your code
	//panic("kheap_virtual_address() is not implemented yet...!!");

	//return the virtual address corresponding to given physical_address
	//refer to the project presentation and documentation for details

	uint32 offset;
   	struct FrameInfo *ptr = to_frame_info(physical_address);

   	if (ptr == NULL || ptr->references == 0) {
   		//cprintf("null kheap_virtual_address here \n");
   		return 0;
   	}

    offset = PGOFF(physical_address);

   	return (ptr->bufferedVA + offset);


}
//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//
// krealloc():

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, if moved to another loc: the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to kmalloc().
//	A call with new_size = zero is equivalent to kfree().

void *krealloc(void *virtual_address, uint32 new_size)
{
	//TODO: [PROJECT'24.MS2 - BONUS#1] [1] KERNEL HEAP - krealloc
	// Write your code here, remove the panic and write your code
	return NULL;
	panic("krealloc() is not implemented yet...!!");
}
