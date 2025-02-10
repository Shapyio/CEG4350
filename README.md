# CEG4350
CEG4350 - OS Internals &amp; Design (FA24)

The projects build off one another.
## Projects
- Project 1: Initial OS framework provided. Focus on displaying text on screen. Modified bootloader to disable cursor. Modified `io.c`/`io.h` to be able to process text onscreen. OS displays "Hello World!".
- Project 2: Reading from the keyboard. Completed `initkeymap()` to initialize the key mapping. `hal.c`/`hal.h` modified to implement `getchar()` and `scanf(char string[])`. Now OS can take keyboard input and display it.
- Project 3: Primitive multitasking implemented. `multitasking.c`/`multitasking.h` provided. Implemented `schedule()`, `yield()`, `exit()`, and `createproc(func, stack)`. Now running new provided `kernel.c` multitasking takes place.
- Project 4: Implement custom scheduling, cooperative round robin scheduling.
- Project 5: Implement file system. 

## How to Launch:
Bochs is needed to compile and boot the OS. Run `wsl make` in the OS folder to compile a bochs boot file. From there, load the boot file in bochs and start. Currently, the boot file should load the file system. You can `git pull` older project commits to see how the different parts work.
