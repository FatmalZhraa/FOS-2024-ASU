/*
 * fault_handler.c
 *
 *  Created on: Oct 12, 2022
 *      Author: HP
 */

#include "trap.h"
#include <kern/proc/user_environment.h>
#include <kern/cpu/sched.h>
#include <kern/cpu/cpu.h>
#include <kern/disk/pagefile_manager.h>
#include <kern/mem/memory_manager.h>

//2014 Test Free(): Set it to bypass the PAGE FAULT on an instruction with this length and continue executing the next one
// 0 means don't bypass the PAGE FAULT
uint8 bypassInstrLength = 0;

//===============================
// REPLACEMENT STRATEGIES
//===============================
//2020
void setPageReplacmentAlgorithmLRU(int LRU_TYPE)
{
	assert(LRU_TYPE == PG_REP_LRU_TIME_APPROX || LRU_TYPE == PG_REP_LRU_LISTS_APPROX);
	_PageRepAlgoType = LRU_TYPE ;
}
void setPageReplacmentAlgorithmCLOCK(){_PageRepAlgoType = PG_REP_CLOCK;}
void setPageReplacmentAlgorithmFIFO(){_PageRepAlgoType = PG_REP_FIFO;}
void setPageReplacmentAlgorithmModifiedCLOCK(){_PageRepAlgoType = PG_REP_MODIFIEDCLOCK;}
/*2018*/ void setPageReplacmentAlgorithmDynamicLocal(){_PageRepAlgoType = PG_REP_DYNAMIC_LOCAL;}
/*2021*/ void setPageReplacmentAlgorithmNchanceCLOCK(int PageWSMaxSweeps){_PageRepAlgoType = PG_REP_NchanceCLOCK;  page_WS_max_sweeps = PageWSMaxSweeps;}

//2020
uint32 isPageReplacmentAlgorithmLRU(int LRU_TYPE){return _PageRepAlgoType == LRU_TYPE ? 1 : 0;}
uint32 isPageReplacmentAlgorithmCLOCK(){if(_PageRepAlgoType == PG_REP_CLOCK) return 1; return 0;}
uint32 isPageReplacmentAlgorithmFIFO(){if(_PageRepAlgoType == PG_REP_FIFO) return 1; return 0;}
uint32 isPageReplacmentAlgorithmModifiedCLOCK(){if(_PageRepAlgoType == PG_REP_MODIFIEDCLOCK) return 1; return 0;}
/*2018*/ uint32 isPageReplacmentAlgorithmDynamicLocal(){if(_PageRepAlgoType == PG_REP_DYNAMIC_LOCAL) return 1; return 0;}
/*2021*/ uint32 isPageReplacmentAlgorithmNchanceCLOCK(){if(_PageRepAlgoType == PG_REP_NchanceCLOCK) return 1; return 0;}

//===============================
// PAGE BUFFERING
//===============================
void enableModifiedBuffer(uint32 enableIt){_EnableModifiedBuffer = enableIt;}
uint8 isModifiedBufferEnabled(){  return _EnableModifiedBuffer ; }

void enableBuffering(uint32 enableIt){_EnableBuffering = enableIt;}
uint8 isBufferingEnabled(){  return _EnableBuffering ; }

void setModifiedBufferLength(uint32 length) { _ModifiedBufferLength = length;}
uint32 getModifiedBufferLength() { return _ModifiedBufferLength;}

//===============================
// FAULT HANDLERS
//===============================

//==================
// [1] MAIN HANDLER:
//==================
/*2022*/
uint32 last_eip = 0;
uint32 before_last_eip = 0;
uint32 last_fault_va = 0;
uint32 before_last_fault_va = 0;
int8 num_repeated_fault  = 0;

struct Env* last_faulted_env = NULL;
void fault_handler(struct Trapframe *tf)
{
	/******************************************************/
	// Read processor's CR2 register to find the faulting address
	uint32 fault_va = rcr2();
	//	cprintf("\n************Faulted VA = %x************\n", fault_va);
	//	print_trapframe(tf);
	/******************************************************/

	//If same fault va for 3 times, then panic
	//UPDATE: 3 FAULTS MUST come from the same environment (or the kernel)
	struct Env* cur_env = get_cpu_proc();
	if (last_fault_va == fault_va && last_faulted_env == cur_env)
	{
		num_repeated_fault++ ;
		if (num_repeated_fault == 3)
		{
			print_trapframe(tf);
			panic("Failed to handle fault! fault @ at va = %x from eip = %x causes va (%x) to be faulted for 3 successive times\n", before_last_fault_va, before_last_eip, fault_va);
		}
	}
	else
	{
		before_last_fault_va = last_fault_va;
		before_last_eip = last_eip;
		num_repeated_fault = 0;
	}
	last_eip = (uint32)tf->tf_eip;
	last_fault_va = fault_va ;
	last_faulted_env = cur_env;
	/******************************************************/
	//2017: Check stack overflow for Kernel
	int userTrap = 0;
	if ((tf->tf_cs & 3) == 3) {
		userTrap = 1;
	}
	if (!userTrap)
	{
		struct cpu* c = mycpu();
		//cprintf("trap from KERNEL\n");
		if (cur_env && fault_va >= (uint32)cur_env->kstack && fault_va < (uint32)cur_env->kstack + PAGE_SIZE)
			panic("User Kernel Stack: overflow exception!");
		else if (fault_va >= (uint32)c->stack && fault_va < (uint32)c->stack + PAGE_SIZE)
			panic("Sched Kernel Stack of CPU #%d: overflow exception!", c - CPUS);
#if USE_KHEAP
		if (fault_va >= KERNEL_HEAP_MAX)
			panic("Kernel: heap overflow exception!");
#endif
	}
	//2017: Check stack underflow for User
	else
	{
		//cprintf("trap from USER\n");
		if (fault_va >= USTACKTOP && fault_va < USER_TOP)
			panic("User: stack underflow exception!");
	}

	//get a pointer to the environment that caused the fault at runtime
	//cprintf("curenv = %x\n", curenv);
	struct Env* faulted_env = cur_env;
	if (faulted_env == NULL)
	{
		print_trapframe(tf);
		panic("faulted env == NULL!");
	}
	//check the faulted address, is it a table or not ?
	//If the directory entry of the faulted address is NOT PRESENT then
	if ( (faulted_env->env_page_directory[PDX(fault_va)] & PERM_PRESENT) != PERM_PRESENT)
	{
		// we have a table fault =============================================================
		//		cprintf("[%s] user TABLE fault va %08x\n", curenv->prog_name, fault_va);
		//		print_trapframe(tf);

		faulted_env->tableFaultsCounter ++ ;

		table_fault_handler(faulted_env, fault_va);
	}
	else
	{
		if (userTrap)
		{
			/*============================================================================================*/
			//TODO: [PROJECT'24.MS2 - #08] [2] FAULT HANDLER I - Check for invalid pointers
			//(e.g. pointing to unmarked user heap page, kernel or wrong access rights),
			//your code is here


			/*============================================================================================*/
	          //validate the faulted va

				uint32 perm = pt_get_page_permissions(faulted_env->env_page_directory,fault_va);

				//check if this faulted va is in user heap
				if ((fault_va >= USER_HEAP_START) && (fault_va < USER_HEAP_MAX))
				{
					//check if faulted va is pointing to unmarked page or not
					if ((perm & PERM_TO_MARK) != PERM_TO_MARK)
					{
						//cprintf("here_1 invalid pointers");
						env_exit();
					}
				}
				//check if this faulted va is pointing to kernel
				if (fault_va >= USER_LIMIT) {

					//cprintf("here_2 invalid pointers");
					env_exit();
				}
				//check if this faulted va exists but with read-only permessions
				if (perm & PERM_PRESENT) {

					if ((perm & PERM_WRITEABLE) == 0) {
						//cprintf("here_3 invalid pointers");     //correct
						env_exit();

					}
				}
		/*2022: Check if fault due to Access Rights */
		int perms = pt_get_page_permissions(faulted_env->env_page_directory, fault_va);
		if (perms & PERM_PRESENT)
			panic("Page @va=%x is exist! page fault due to violation of ACCESS RIGHTS\n", fault_va) ;
		/*============================================================================================*/


		// we have normal page fault =============================================================
		faulted_env->pageFaultsCounter ++ ;

		//		cprintf("[%08s] user PAGE fault va %08x\n", curenv->prog_name, fault_va);
		//		cprintf("\nPage working set BEFORE fault handler...\n");
		//		env_page_ws_print(curenv);

		if(isBufferingEnabled())
		{
			__page_fault_handler_with_buffering(faulted_env, fault_va);
		}
		else
		{
			//page_fault_handler(faulted_env, fault_va);
			page_fault_handler(faulted_env, fault_va);
		}
		//		cprintf("\nPage working set AFTER fault handler...\n");
		//		env_page_ws_print(curenv);
		}//usertrap

	}

	/*************************************************************/
	//Refresh the TLB cache
	tlbflush();
	/*************************************************************/
}

//=========================
// [2] TABLE FAULT HANDLER:
//=========================
void table_fault_handler(struct Env * curenv, uint32 fault_va)
{
	//panic("table_fault_handler() is not implemented yet...!!");
	//Check if it's a stack page
	uint32* ptr_table;
#if USE_KHEAP
	{
		ptr_table = create_page_table(curenv->env_page_directory, (uint32)fault_va);
	}
#else
	{
		__static_cpt(curenv->env_page_directory, (uint32)fault_va, &ptr_table);
	}
#endif
}

//=========================
// [3] PAGE FAULT HANDLER:
//=========================
/***MS3 '24 REPLACEMENT HANDELING***/

void Nclock_replacement (struct Env *faulted_env, struct WorkingSetElement *My_element, uint32 vic_Adress, uint32 fault_va)
{
    uint32 *ptr = NULL;
    get_page_table(faulted_env->env_page_directory, vic_Adress, &ptr);

    if (ptr!= NULL) {
        struct FrameInfo *ptrvic_frame =
        		get_frame_info(faulted_env->env_page_directory, vic_Adress, &ptr);
        // get check modified perm victim
        uint32 vic_permissions = pt_get_page_permissions(faulted_env->env_page_directory, vic_Adress);
		if(vic_permissions ==-1||(vic_permissions & PERM_MODIFIED) == PERM_MODIFIED )
		{
			// update env
            pf_update_env_page(faulted_env, vic_Adress, ptrvic_frame);
        }

        // Unmap the victim page
        unmap_frame(faulted_env->env_page_directory, vic_Adress);
        //allocate space for the faulted page
			struct FrameInfo *Replacement_ptr = NULL;
			int result = allocate_frame(&Replacement_ptr);
			//check if there is a space or not
				if(result !=E_NO_MEM)
				{
				  //if there is space, then allocate and map
				  map_frame(faulted_env->env_page_directory,Replacement_ptr,fault_va,PERM_USER|PERM_WRITEABLE);
				}
				else{
					cprintf("kill Replacement 1 here \n");
					env_exit();
					}
	                int ret = pf_read_env_page(faulted_env,(void *)fault_va);
					//if the page does not exist in page file, then:
					if(ret ==E_PAGE_NOT_EXIST_IN_PF)
					{
						//check if it is stack or heap page
					if((fault_va >= USTACKBOTTOM && fault_va<USTACKTOP) || (fault_va>=USER_HEAP_START && fault_va<USER_HEAP_MAX))
					{
						//it is ok and does not do anything
					}
					else
					{
						//if not, then kill the process
						cprintf("kill Replacement 2 here \n");

						 env_exit();
					}
					}

			// Update the working set
			My_element->virtual_address = fault_va;
			//reset counter
			My_element->sweeps_counter = 0;
			//for updating the last one
			faulted_env->page_last_WS_element = LIST_NEXT(My_element);
			if (faulted_env->page_last_WS_element == NULL)
			{
                   faulted_env->page_last_WS_element = LIST_FIRST(&faulted_env->page_WS_list);
               }
           } else {
               //cprintf("replacement22222222\n");
               env_exit();
           }
}
// [1] NORMAL ALGORITHM (+VE)
void Normal_replacement(struct Env* faulted_env, uint32 fault_va )
{
	                 bool Always = 1;
					// Check page_WS_max_sweeps
					if (page_WS_max_sweeps > 0) {
						int Exist = 0;
						struct WorkingSetElement *My_element = faulted_env->page_last_WS_element;
						uint32 vic_Adress;
						do {
							 vic_Adress = My_element->virtual_address;
							 // get , check vic permissions at normal case
							uint32 vic_permissions = pt_get_page_permissions(faulted_env->env_page_directory, vic_Adress);
							//check (USED ONLY) if used is 0
							if ((vic_permissions & PERM_USED) != PERM_USED) {
								// Increment count
								My_element->sweeps_counter++;
								// if get max sweeps
								if (My_element->sweeps_counter == page_WS_max_sweeps) {
									Exist = 1;
									break;
								}
							}
							//if used not 0
							else if((vic_permissions & PERM_USED) == PERM_USED||vic_permissions == -1 ){
								// Reset count
								My_element->sweeps_counter = 0;
								// Reset bit
								pt_set_page_permissions(faulted_env->env_page_directory, vic_Adress, 0, PERM_USED);
							}
							//for updating the last one
							My_element = My_element->prev_next_info.le_next;
											    if (!My_element) {
											    	My_element = LIST_FIRST(&faulted_env->page_WS_list);
											    }
						} while (Always);

						if (Exist)
						{
							// Replace
							Nclock_replacement(faulted_env, My_element, vic_Adress, fault_va);
						} else {
							cprintf("No victim EXIST\n");
							env_exit();
						}
					}
	}
// [2] MODIFIED ALGORITHM (-VE)
void Modified_replacement(struct Env* faulted_env, uint32 fault_va )

{                 uint32 vis_Adress;
				int MAX_S = -1 * page_WS_max_sweeps;
				int Exist = 0;
				bool Always = 1;
				struct WorkingSetElement *My_element = faulted_env->page_last_WS_element;

				do {
					vis_Adress = My_element->virtual_address;
					// get check used and modified
					uint32 vic_permissions = pt_get_page_permissions(faulted_env->env_page_directory, vis_Adress);
					// used is 0
					if ((vic_permissions & PERM_USED) != PERM_USED) {
						My_element->sweeps_counter++;
						// modified is 0
						if ((vic_permissions & PERM_MODIFIED) != PERM_MODIFIED) {
							if (My_element->sweeps_counter == MAX_S)
							{
								Exist = 1;
								break;
							}
						} // modified id not 0
						else if(vic_permissions ==-1||(vic_permissions & PERM_MODIFIED) == PERM_MODIFIED ){
						   // give chance to make extra cycle at modified version
							if (My_element->sweeps_counter == (MAX_S + 1))
							{
								Exist = 1;
								break;
							}
						}
					} else {
						// Reset count
						My_element->sweeps_counter = 0;
						// Reset bit
						pt_set_page_permissions(faulted_env->env_page_directory, vis_Adress, 0, PERM_USED);
					}
					//for updating the last one
					My_element = My_element->prev_next_info.le_next;
									    if (!My_element) {
									    	My_element = LIST_FIRST(&faulted_env->page_WS_list);
									    }
				} while (Always);

				if (Exist) {
					// Replace the victim
					Nclock_replacement(faulted_env, My_element, vis_Adress, fault_va);
				} else {
					cprintf("No victim EXIST\n");
					env_exit();
				}
}

void page_fault_handler(struct Env * faulted_env, uint32 fault_va)
{
#if USE_KHEAP
		struct WorkingSetElement *victimWSElement = NULL;
		uint32 wsSize = LIST_SIZE(&(faulted_env->page_WS_list));
#else
		int iWS =faulted_env->page_last_WS_index;
		uint32 wsSize = env_page_ws_get_size(faulted_env);
#endif

		/*if(isPageReplacmentAlgorithmNchanceCLOCK())
		{*/
		if(wsSize < (faulted_env->page_WS_max_size))
			{
				//cprintf("PLACEMENT=========================WS Size = %d\n", wsSize );
				//TODO: [PROJECT'24.MS2 - #09] [2] FAULT HANDLER I - Placement

			//allocate space for the faulted page
			struct FrameInfo *placement_ptr=NULL;
			int result=allocate_frame(&placement_ptr);
			//check if there is a space or not
				if(result !=E_NO_MEM)
				{
				  //if there is space, then allocate and map
				  map_frame(faulted_env->env_page_directory,placement_ptr,fault_va,PERM_USER|PERM_WRITEABLE);
				}
				else{
					//if not, then kill the process
					env_exit();
					}
				//read the faulted page from page file to memory
				int ret = pf_read_env_page(faulted_env,(void *)fault_va);
				//if the page does not exist in page file, then:
				if(ret ==E_PAGE_NOT_EXIST_IN_PF)
				{
					//check if it is stack or heap page
				if((fault_va >= USTACKBOTTOM && fault_va<USTACKTOP) || (fault_va>=USER_HEAP_START && fault_va<USER_HEAP_MAX))
				{
					//it is ok and does not do anything
				}
				else
				{
					//if not, then kill the process
					 env_exit();
				}
				}
				//reflect the changes in page ws list

				struct WorkingSetElement *new_element=env_page_ws_list_create_element(faulted_env,fault_va);

				if (new_element==NULL){
						env_exit();
				}
                //for updating the last one
				LIST_INSERT_TAIL(&faulted_env->page_WS_list,new_element);
				if(LIST_SIZE(&faulted_env->page_WS_list)<faulted_env->page_WS_max_size)

				faulted_env->page_last_WS_element=NULL;
				else{
					faulted_env->page_last_WS_element=LIST_FIRST(&faulted_env->page_WS_list);
					}
	}
	else
	{
		//TODO: [PROJECT'24.MS3] [2] FAULT HANDLER II - Replacement
		       	// -ve
			    if (page_WS_max_sweeps < 0)
			    {
			        Modified_replacement(faulted_env,  fault_va);
			    }
			    else if(page_WS_max_sweeps >0)
			    { //+ve
			        Normal_replacement(faulted_env, fault_va);
			    }
		else
		{
			struct WorkingSetElement *My_element = faulted_env->page_last_WS_element;
            bool Always = 1;
			uint32 vic_Adress;
			do {
				vic_Adress = My_element->virtual_address;
				uint32 vic_permissions = pt_get_page_permissions(faulted_env->env_page_directory, vic_Adress);

				if ((vic_permissions & PERM_USED) != PERM_USED) {
					// replace now
					Nclock_replacement(faulted_env, My_element, vic_Adress, fault_va);
					return;
				} else
				{
					// Reset bit
					pt_set_page_permissions(faulted_env->env_page_directory, vic_Adress, 0, PERM_USED);
				}
				//for updating the last one

				My_element = My_element->prev_next_info.le_next;
				    if (!My_element)
				    {
				    	My_element = LIST_FIRST(&faulted_env->page_WS_list);
				    }
			} while (Always);
		}
	}


}

void __page_fault_handler_with_buffering(struct Env * curenv, uint32 fault_va)
{
	//[PROJECT] PAGE FAULT HANDLER WITH BUFFERING
	// your code is here, remove the panic and write your code
	panic("__page_fault_handler_with_buffering() is not implemented yet...!!");
}

