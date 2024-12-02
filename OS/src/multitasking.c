#include "./types.h"
#include "./multitasking.h"
#include "./io.h"

// An array to hold all of the processes we create
proc_t processes[MAX_PROCS];

// Keep track of the next index to place a newly created process in the process array
uint8 process_index = 0;

proc_t *prev;       // The previously ran user process
proc_t *running;    // The currently running process, can be either kernel or user process
proc_t *next;       // The next process to run
proc_t *kernel;     // The kernel process

// Select the next user process (proc_t *next) to run
// Selection must be made from the processes array (proc_t processes[])
// Count is the number of user processes are ready and available
int schedule()
{
    int count = 0;

    // Check how many processes there are left (return accurate count)
    for (int i=0; i < MAX_PROCS; i++)
    {
        if(processes[i].type == PROC_USER && processes[i].status == PROC_READY)
        {
            count++;
        }
    }

    // While processes available, schedule
    if (count > 0)
    {
        // Calculate start position for next loop
        // If prev exists, make start prev + 1, bounded by processes
        // Else, make it 0
        int start = prev ? (prev->pid + 1) % process_index : 0;

        for (int i=0; i < MAX_PROCS; i++) // Loop through processes
        {
            int pid = (start + i) % process_index;

            // DEBUGGING TEXT
            // printf("\nSTART: ");
            // printint(start);
            // printf(", PID: ");
            // printint(pid);
            // printf(", COUNT: ");
            // printint(count);
            // printf("\n");

            if(processes[pid].type == PROC_USER && processes[pid].status == PROC_READY) // Select first waiting user process
            {
                next = &processes[pid];
                return count;
            }
        }
    }
    return count;
}

// Create a new user process
// When the process is eventually ran, start executing from the function provided (void *func)
// Initialize the stack top and base at location (void *stack)
// If we have hit the limit for maximum processes, return -1
// Store the newly created process inside the processes array (proc_t processes[])
int createproc(void *func, char *stack)
{
    // If process limit at least equal to MAX_PROCS, return -1; can't start new process
    if(process_index >= MAX_PROCS)
    {
        return -1;
    }

    // Create the new process
    proc_t process;
    process.status = PROC_READY;
    process.type = PROC_USER;

    // Set the instruction pointer to the function
    process.eip = func;             // func is where execution starts

    // Initialize stack pointers
    process.esp = stack;            // esp points to the top of the stack
    process.ebp = stack;            // ebp is typically initialized to esp

    // Assign PID and add process to process array
    process.pid = process_index;
    processes[process_index] = process;

    // Assign the process as next process
    next = &processes[process_index];

    process_index++; // Increment index

    return 0;
}

// Create a new kernel process
// The kernel process is ran immediately, executing from the function provided (void *func)
// Stack does not to be initialized because it was already initialized when main() was called
// If we have hit the limit for maximum processes, return -1
// Store the newly created process inside the processes array (proc_t processes[])
int startkernel(void func())
{
    // If we have filled our process array, return -1
    if(process_index >= MAX_PROCS)
    {
        return -1;
    }

    // Create the new kernel process
    proc_t kernproc;
    kernproc.status = PROC_RUNNING; // Processes start ready to run
    kernproc.type = PROC_KERNEL;    // Process is a kernel process

    // Assign a process ID and add process to process array
    kernproc.pid = process_index;
    processes[process_index] = kernproc;
    kernel = &processes[process_index]; // Use a proc_t pointer to keep track of the kernel process so we don't have to loop through the entire process array to find it
    process_index++;

    // Assign the kernel to the running process and execute
    running = kernel;
    func();

    return 0;
}

// Terminate the process that is currently running (proc_t current)
// Assign the kernel as the next process to run
// Context switch to the kernel process
void exit()
{
    // Terminate current process
    running->status = PROC_TERMINATED;
    if(running->type == PROC_USER)
    {
        next = kernel;
        switchcontext();
    }

    return;
}

// Yield the current process
// This will give another process a chance to run
// If we yielded a user process, context switch to the kernel process
// If we yielded a kernel process, context switch to the next process
// The next process should have already been selected via scheduling
void yield()
{
    // If user process running, just assign kernel as next
    if (running->type == PROC_USER)
    {
        running->status = PROC_READY;   // Yielded user process becomes ready
        next = kernel;                  // Switch to the kernel process
    }
    else if (!schedule())               // Kernel is running, and next == 0
    {
        clearscreen();
        printf("Error: No next process assigned!\n");
        while (1);                      // Infinite loop to prevent crashing
    }

    switchcontext();

    return;
}

// Context switching function
// This function will save the context of the running process (proc_t running)
// and switch to the context of the next process we want to run (proc_t next)
// The running and next processes must both be valid for this function to work
// if they are not, our OS will certainly crash
void __attribute__((naked)) switchcontext()
{
    // Capture all the register values so we can reload them if we ever run this process again
    register uint32 eax asm ("eax");    // General purpose registers
    register uint32 ebx asm ("ebx");
    register uint32 ecx asm ("ecx");
    register uint32 edx asm ("edx");
    register uint32 esi asm ("esi");    // Indexing registers
    register uint32 edi asm ("edi");
    register void *ebp asm ("ebp");     // Stack base pointer
    register void *esp asm ("esp");     // Stack top pointer
    
    asm volatile("pushfl");
    asm volatile("pop %eax");
    register uint32 eflags asm ("eax"); // Flags and conditions

    asm volatile("mov %cr3, %eax");
    register uint32 cr3  asm ("eax");   // CR3 for virtual addressing

    // Store all the current register values inside the process that is running
    running->eax    = eax;
    running->ebx    = ebx;
    running->ecx    = ecx;
    running->edx    = edx;

    running->esi    = esi;
    running->edi    = edi;

    running->ebp    = ebp;
    running->esp    = esp;

    running->eflags = eflags;
    running->cr3    = cr3;

    // Set the next instruction for this process to be the resume after the context switch
    running->eip    = &&resume;

    // Set prev to the current running process
    prev = running;
    // Start running the next process
    running = next;
    running->status = PROC_RUNNING;

    // Reload all the registers previously saved from the process we want to run
    asm volatile("mov %0, %%eax" : : "r"(running->eflags));
    asm volatile("push %eax");
    asm volatile("popfl");

    asm volatile("mov %0, %%eax" : :    "r"(running->eax));
    asm volatile("mov %0, %%ebx" : :    "r"(running->ebx));
    asm volatile("mov %0, %%ecx" : :    "r"(running->ecx));
    asm volatile("mov %0, %%edx" : :    "r"(running->edx));

    asm volatile("mov %0, %%esi" : :    "r"(running->esi));
    asm volatile("mov %0, %%edi" : :    "r"(running->edi));

    asm volatile("mov %0, %%ebp" : :    "r"(running->ebp));
    asm volatile("mov %0, %%esp" : :    "r"(running->esp));

    asm volatile("mov %0, %%cr3" : :    "r"(running->cr3));

    // Jump to the last instruction we saved from the running process
    // If this is a new process this will be the beginning of the process's function
    asm volatile("jmp *%0" : : "r" (running->eip));

    // This resume address will eventually get executed when the previous process gets executed again
    // This will allow us to resume the previous process after our yield
    resume:
    asm volatile("ret");
}