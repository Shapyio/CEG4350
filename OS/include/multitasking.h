// The maximum number of user procs
#define MAX_USER_PROCS 5

// The maximum number of kernel procs
#define MAX_KERN_PROCS 1

// The maximum number of total procs
#define MAX_PROCS MAX_USER_PROCS + MAX_KERN_PROCS

// All possible statuses for processes
typedef enum
{
	PROC_READY, 		// The process is ready, but waits for OS to dispatch
  PROC_RUNNING, 	// The process is executing on CPU but can be interrupted
	PROC_TERMINATED // Process was finished or forcefully terminated
} proc_status_t;

// All possible types of processes
typedef enum
{
	PROC_USER,
	PROC_KERNEL
} proc_type_t;

// Process control block
// Contains all registers and info for each process
typedef struct
{
  int pid;							// Process ID
	proc_type_t type;			// Process type (proc_type_t)
	proc_status_t status;	// Process status (proc_status_t)
	uint32 eax;						// Registers A
	uint32 ebx;						//					 B
	uint32 ecx;						// 					 C
	uint32 edx;						// 					 D
	uint32 esi;						
	uint32 edi;						
	void *ebp;
	void *esp;
	uint32 eflags;
	uint32 cr3;
	void *eip;
} proc_t;

int schedule();
int createproc(void *func, char *stack);
int startkernel(void func());
void runproc(proc_t proc);
void yield();
void switchcontext();
void exit();
void banner();