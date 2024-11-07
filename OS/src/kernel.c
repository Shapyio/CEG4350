#include "./io.h"
#include "./multitasking.h"
#include "./irq.h"
#include "./isr.h"
//#include "./fat.h" Not existant... yet

void prockernel();
void proca();
void procb();
void procc();
void procd();
void proce();

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
	createproc(proca, (void *) 0x3000);
	createproc(procb, (void *) 0x3100);
	createproc(procc, (void *) 0x3200);
	createproc(procd, (void *) 0x3300);
	createproc(proce, (void *) 0x3400);

	// Schedule the next process
	int userprocs = schedule();

	while(userprocs > 0)
	{
		yield();
		userprocs = schedule();
	}

	printf("\nExiting Kernel Process...\n");
}

// The user processes

void proca()
{
	printf("A");
	exit();
}

void procb()
{
	printf("B");
	yield();
	printf("B");
	exit();
}

void procc()
{
	printf("C");
	yield();
	printf("C");
	yield();
	printf("C");
	yield();
	printf("C");
	exit();
}

void procd()
{
	printf("D");
	yield();
	printf("D");
	yield();
	printf("D");
	exit();
}

void proce()
{
	printf("E");
	yield();
	printf("E");
	exit();
}