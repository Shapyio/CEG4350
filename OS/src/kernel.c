#include "./io.h"
#include "./multitasking.h"
#include "./irq.h"
#include "./isr.h"
//#include "./fat.h" Not existant... yet

void prockernel();
void proca();

int main() 
{
	// Clear the screen
	clearscreen();

	// Initialize our keyboard
	initkeymap();

	startkernel(prockernel);
	
	return 0;
}

void prockernel()
{
	printf("Starting Kernel Process...\n");
	
	// Create the user processes
	createproc(proca, (void *) 0x10000);

	// Schedule the next process
	int userprocs = schedule();

	while(userprocs > 0)
	{
		yield();
		printf("Resuming Kernel Process\n");
		userprocs = schedule();
	}

	printf("Exiting Kernel Process...\n");
}

// The user processes

void proca()
{
	printf("Starting User Process A\n");
	yield();
	printf("Resuming User Process A\n");
	yield();
	printf("Exiting User Process A\n");
	exit();
}