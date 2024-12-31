// User-level Semaphore

#include "inc/lib.h"

struct semaphore create_semaphore(char *semaphoreName, uint32 value)
{
	//TODO: [PROJECT'24.MS3 - #02] [2] USER-LEVEL SEMAPHORE - create_semaphore
	struct semaphore sem;
	    sem.semdata = (struct __semdata *)smalloc(semaphoreName, sizeof(struct __semdata), 1);
	    if (sem.semdata == NULL) {
	    	//cprintf("NULL SEM HEREEEEEEE\n");

	    }
	   //initialize semaphore members
	    sem.semdata->count = value;
	    sem.semdata->lock = 0;
	    //initialize the Queue >== system call to use queue_init
	    sys_init_queue(&(sem.semdata->queue));
	    uint32 ssn=sizeof(sem.semdata->name);

	    strncpy(sem.semdata->name, semaphoreName, ssn - 1);
	    sem.semdata->name[ssn - 1] = '\0';
        //return the wrapper for protection

	    return sem;

}
struct semaphore get_semaphore(int32 ownerEnvID, char* semaphoreName)
{
	//TODO: [PROJECT'24.MS3 - #03] [2] USER-LEVEL SEMAPHORE - get_semaphore
	struct semaphore sem;
	    sem.semdata = (struct __semdata*)sget(ownerEnvID, semaphoreName);
	    if (sem.semdata == NULL) {
	    	//cprintf("nnnnnnnnnnnnnnnnnnnn\n");
	    }
	    return sem;
}

void wait_semaphore(struct semaphore sem)
{
	//TODO: [PROJECT'24.MS3 - #04] [2] USER-LEVEL SEMAPHORE - wait_semaphore
	if (sem.semdata == NULL) {
	        //cprintf("Invalid semaphore\n");

	    }
	    while (xchg(&sem.semdata->lock, 1) != 0)
	    {

	    }
	    sem.semdata->count--;
	    if (sem.semdata->count < 0) {
	    	//cprintf("SEM WAIT 1\n");
	        sys_enqueue(&(sem.semdata->queue),&sem);
	        //cprintf("SEM WAIT 2\n");
	    }
	    else
	    {
	    	sem.semdata->lock = 0;
	    }
}

void signal_semaphore(struct semaphore sem)
{
	//TODO: [PROJECT'24.MS3 - #05] [2] USER-LEVEL SEMAPHORE - signal_semaphore
	if (sem.semdata == NULL) {
	        //cprintf("Invalid semaphore \n");
	        //return;
	    }
	    while (xchg(&sem.semdata->lock, 1) != 0)
	    {

	    }
	    sem.semdata->count++;
	    if (sem.semdata->count < 0 || sem.semdata->count == 0) {
	    	//cprintf("SEM SIGNAL 1\n");
	         sys_dequeue(&(sem.semdata->queue));
	         //cprintf("SEM SIGNAL 2\n");
	    }
	    	 sem.semdata->lock = 0;

}

int semaphore_count(struct semaphore sem)
{
	return sem.semdata->count;
}
