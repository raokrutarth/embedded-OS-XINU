/* kill.c - kill */

#include <xinu.h>

/*------------------------------------------------------------------------
 *  kill  -  Kill a process and remove it from the system
 *------------------------------------------------------------------------
 */
syscall	kill(
	  pid32		pid		/* ID of process to kill	*/
	)
{
	intmask	mask;			/* Saved interrupt mask		*/
	struct	procent *prptr;		/* Ptr to process' table entry	*/
	int32	i;			/* Index into descriptors	*/

	mask = disable();
	if (isbadpid(pid) || (pid == NULLPROC)
	    || ((prptr = &proctab[pid])->prstate) == PR_FREE) {
		restore(mask);
		return SYSERR;
	}

	/* Garbage Collection */
	struct allocated_block b;
	b = (struct allocated_block) {0, 0};
	pid32 temp = currpid;
	currpid = pid;
	b = popMemRecord( &(prptr->dmem) );
	while( b.blkAddr != (char*)EMPTY )
	{ 
		// kprintf("[kill][%d] Freeing unfreed block. blkaddr: %u\n", currpid, (char*)b.blkAddr );
		freemem(b.blkAddr, b.size);
		b = popMemRecord( &(prptr->dmem) );
	}
	currpid = temp;

	if (--prcount <= 1) {		/* Last user process completes	*/
		xdone();
	}

	send(prptr->prparent, pid);
	pid32 parent = prptr->prparent;
	struct	procent *parent_ptr;
	if(parent > 0)
	{
		parent_ptr = &proctab[parent];
		parent_ptr->child_pr_killed = pid;
		if(parent_ptr->prstate == WAITFORCHLD)
			ready(parent); // parent was waiting for child to exit		
	}

	for (i=0; i<3; i++) {
		close(prptr->prdesc[i]);
	}
	freestk(prptr->prstkbase, prptr->prstklen);

	switch (prptr->prstate) {
	case PR_CURR:
		prptr->prstate = PR_FREE;	/* Suicide */
		resched();

	case PR_SLEEP:
	case PR_RECTIM:
		unsleep(pid);
		prptr->prstate = PR_FREE;
		break;

	case PR_WAIT:
		semtab[prptr->prsem].scount++;
		/* Fall through */

	case PR_READY:
		getitem(pid);		/* Remove from queue */
		/* Fall through */

	default:
		prptr->prstate = PR_FREE;
	}

	restore(mask);
	return OK;
}
