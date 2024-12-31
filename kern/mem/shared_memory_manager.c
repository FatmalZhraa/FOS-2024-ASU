#include <inc/memlayout.h>
#include "shared_memory_manager.h"

#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/queue.h>
#include <inc/environment_definitions.h>

#include <kern/proc/user_environment.h>
#include <kern/trap/syscall.h>
#include "kheap.h"
#include "memory_manager.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//
struct Share* get_share(int32 ownerID, char* name);

//===========================
// [1] INITIALIZE SHARES:
//===========================
//Initialize the list and the corresponding lock
void sharing_init()
{
#if USE_KHEAP
	LIST_INIT(&AllShares.shares_list) ;
	init_spinlock(&AllShares.shareslock, "shares lock");
#else
	panic("not handled when KERN HEAP is disabled");
#endif
}

//==============================
// [2] Get Size of Share Object:
//==============================
int getSizeOfSharedObject(int32 ownerID, char* shareName)
{
	struct Share* ptr_share = get_share(ownerID, shareName);
	if (ptr_share == NULL)
		return E_SHARED_MEM_NOT_EXISTS;
	else
		return ptr_share->size;
	return 0;
}

//===========================================================



//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
//===========================
// [1] Create frames_storage:
//===========================
// Create the frames_storage and initialize it by 0
inline struct FrameInfo** create_frames_storage(int numOfFrames)
{
	//TODO: [PROJECT'24.MS2 - #16] [4] SHARED MEMORY - create_frames_storage()
	uint32 s=sizeof(struct FrameInfo*) ;
    struct FrameInfo** frames = (struct FrameInfo**)kmalloc(s * numOfFrames);
    if (!frames)
    	{
    	return NULL;
    	//cprintf("Failed to allocate frames.\n");
    	}
    int i = 0;
   memset(frames,0,numOfFrames*s);
    return frames;
}

//=====================================
// [2] Alloc & Initialize Share Object:
//=====================================
//Allocates a new shared object and initialize its member
//It dynamically creates the "framesStorage"
//Return: allocatedObject (pointer to struct Share) passed by reference
struct Share* create_share(int32 ownerID, char* shareName, uint32 size, uint8 isWritable)
{
	// uint32 UP_SIZE=ROUNDUP(size, PAGE_SIZE);
	//TODO: [PROJECT'24.MS2 - #16] [4] SHARED MEMORY - create_share()
	 struct Share* NEW = (struct Share*)kmalloc(sizeof(struct Share));
	    if (!NEW)
	    	{
	    	return NULL;
	    	//cprintf("Failed to allocate memory.\n");
	    	}
	    int numF = (size +  PAGE_SIZE -1)/ PAGE_SIZE;
	    NEW->framesStorage = create_frames_storage(numF);
	    if (!NEW->framesStorage) {
	        kfree(NEW);
	        return NULL;
	    }
	    uint32 s=sizeof(NEW->name);
	    NEW->ownerID = ownerID;
	    NEW->size = size;
	    NEW->name[ s- 1] = '\0';
	    NEW->isWritable = isWritable;
	    strncpy(NEW->name, shareName,s - 1);
	    NEW->ID = (uint32)NEW & 0x7FFFFFFF;
	    NEW->references = 1;
	   // LIST_INSERT_HEAD(&AllShares.shares_list,NEW);
	    return NEW;

}

//=============================
// [3] Search for Share Object:
//=============================
//Search for the given shared object in the "shares_list"
//Return:
//	a) if found: ptr to Share object
//	b) else: NULL
struct Share* get_share(int32 ownerID, char* name)
{
	//TODO: [PROJECT'24.MS2 - #17] [4] SHARED MEMORY - get_share()
    struct Share* Now;
    acquire_spinlock(&AllShares.shareslock);
    Now = AllShares.shares_list.lh_first;
    while (Now != NULL)
    {
    	bool id = (Now->ownerID == ownerID);
    	bool str = (strcmp(Now->name, name) == 0);
        if ( id && str )
        {
            release_spinlock(&AllShares.shareslock);
            return Now;
        }
        Now = Now->prev_next_info.le_next;
    }
    //cprintf(" Share not found for Owner.\n");

    release_spinlock(&AllShares.shareslock);
    return NULL;
}

//=========================
// [4] Create Share Object:
//=========================
int createSharedObject(int32 ownerID, char* shareName, uint32 size, uint8 isWritable, void* virtual_address)
{
	uint32 Up_size = ROUNDUP(size, PAGE_SIZE);
	uint32 N_PAG = Up_size / PAGE_SIZE;
	int num_frams = (size + PAGE_SIZE -1)/PAGE_SIZE;
	struct Env* myenv = get_cpu_proc();

//    }
    struct Share* old = get_share(ownerID, shareName);
    if (old != NULL) {
    	//cprintf("Shared object already exist");
        return E_SHARED_MEM_EXISTS;
    }
    struct Share* new = create_share(ownerID, shareName, size, isWritable);
    if (new == NULL) {
    	//cprintf("Failed to create new Share .\n");
        return E_NO_SHARE;
    }
    uint32 c = 0;
    while (c < num_frams) {
        struct FrameInfo* info;
        int located = allocate_frame(&info);
        if (located == E_NO_MEM) {
        	//cprintf("Failed to allocate frame.\n");

            return E_NO_MEM;
        }
        uint32 framev =(uint32)virtual_address + c * PAGE_SIZE;
        int located2 = map_frame(myenv->env_page_directory, info, framev,
             PERM_USER | PERM_WRITEABLE);
          new->framesStorage[c] = info;

        c++;   }

    acquire_spinlock(&AllShares.shareslock);
    LIST_INSERT_HEAD(&AllShares.shares_list,new);
    release_spinlock(&AllShares.shareslock);
   // new->ID = (int32)((uint32)virtual_address & 0x7FFFFFFF);
    return new->ID;

}

//======================
// [5] Get Share Object:
//======================
int getSharedObject(int32 ownerID, char* shareName, void* virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #21] [4] SHARED MEMORY [KERNEL SIDE] - getSharedObject()

	struct Env* myenv = get_cpu_proc(); //The calling environment
	struct Share* gitshare = get_share(ownerID,shareName);

	int num_frams = (gitshare->size + PAGE_SIZE -1)/PAGE_SIZE;
	uint32 va = (uint32)virtual_address;
    if (!gitshare) {
        //cprintf("Shared object  not found ");
        return E_NO_SHARE;
    }
    int i = 0;
    while (i < num_frams)
    {
        int permission = PERM_USER;

        if (gitshare->isWritable)
        {
            permission |= PERM_WRITEABLE;
        }

        map_frame(myenv->env_page_directory, gitshare->framesStorage[i], va, permission);

        va += PAGE_SIZE;
        i++;
    }
	gitshare->references +=1;
	return gitshare->ID;
}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//==========================
// [B1] Delete Share Object:
//==========================
//delete the given shared object from the "shares_list"
//it should free its framesStorage and the share object itself
void free_share(struct Share* ptrShare)
{
	//TODO: [PROJECT'24.MS2 - BONUS#4] [4] SHARED MEMORY [KERNEL SIDE] - free_share()
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	panic("free_share is not implemented yet");
	//Your Code is Here...

}
//========================
// [B2] Free Share Object:
//========================
int freeSharedObject(int32 sharedObjectID, void *startVA)
{
	//TODO: [PROJECT'24.MS2 - BONUS#4] [4] SHARED MEMORY [KERNEL SIDE] - freeSharedObject()
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	panic("freeSharedObject is not implemented yet");
	//Your Code is Here...

}

