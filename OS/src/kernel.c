#include "./io.h"
#include "./multitasking.h"
#include "./irq.h"
#include "./isr.h"
#include "./fat.h"
#include "./string.h"

void prockernel();
void fileproc();

int main() 
{
	// Clear the screen
	clearscreen();

	// Initialize our keyboard
	initkeymap();

	idt_install();
    isrs_install();
    irq_install();

	startkernel(prockernel);
	
	return 0;
}

void prockernel()
{
	// Create the user processes

	createproc(fileproc, (void *) 0x10000);

	// Schedule the next process

	int userprocs = schedule();

	printf("OS Starting...\n");

	while(userprocs > 0)
	{
		yield();
		userprocs = schedule();
	}

	printf("OS Shutting Down...\n");
}

// The user processes

void fileproc()
{
	// Initialize our file system
	directory_t root;
	init_fs(&root);
	char input = ' ';

	while(input != 'q')
	{
		char filename[8];
		char ext[3];

		// A pointer to the directory entry we will hopefully find
		directory_entry_t *foundEntry = 0;

		// Ask the user to make a selection
		printf("Make a selection (c, d, r, w, q): ");
		char input = getchar();
		putchar(input);
		putchar('\n');

		// If the user typed quit, simply break out of the loop
		if(input == 'q')
		{
			break;
		}
		// If the input was invalid, just restart loop
		else if(input != 'c' && input != 'd' && input != 'r' && input != 'w')
		{
			printf("Error: Invalid input!\n");
			continue;
		}

		// Let the user type in a file name and extension and pad them using spaces
		printf("Enter filename: ");
		scanfWithPadding(filename, ' ', 8);
		putchar('\n');

		printf("Enter extension: ");
		scanfWithPadding(ext, ' ', 3);
		putchar('\n');

		// Search the directory to see if there exists an entry that contains the file name and extension
		int fileExists = findFile(filename, ext, root, foundEntry);

		// If we actually found a file...
		if(fileExists)
		{
			// Create a structure for our file (contains our file's data and meta data)
			file_t file;

			// Point our structure's entry to the entry that we found inside our file system
			file.entry = *foundEntry;

			// Open the file (to retrieve the bytes inside the file from the disk)
			openFile(&file);

			// We cannot create a new file with the same name! (Do nothing)
			if(input == 'c')
			{
				printf("Error: Tried to create a file that already exists!\n");
			}
			// Delete the file from the file system
			else if(input == 'd')
			{
				printf("Deleting File...\n");

				// Close the file before deleting it
				putchar('\n');
				closeFile(&file);

				deleteFile(&file, &root);
			}
			// Read the file and print the contents to the display
			else if(input == 'r')
			{
				clearscreen();
				printf("Reading File...\n");

				// Print the contents of the file to the string
				for(uint32 i = 0; i < file.entry.fileSize; i++)
				{
					// Read one byte from the file
					uint8 byte = readByte(&file, i);

					// Print it to the screen
					putchar((char)byte);
				}

				// Close the file
				putchar('\n');
				closeFile(&file);
			}
			// Allow the user to type in characters and write those to the file
			else if(input == 'w')
			{
				clearscreen();
				printf("Writing File...\n");

				// This will keep track of how many bytes are written to the file
				// For now, we will only allow 512 bytes (1 sector) to be written to the file
				uint32 i = 0;
				uint8 byte = 0;

				// Let the user type in bytes into the file (until they hit ENTER)
				while(byte != '\n' && i < 512)
				{
					byte = (uint8)getchar();

					if(byte != 'n')
					{
						putchar((char)byte);
						writeByte(&file, byte, i);
						i++;
					}

				}
				
				// If we have not overwritten the entire sector, do so now
				// This prevents nasty leftovers in the sector from old writes
				while(i % 512 != 0)
				{
					writeByte(&file, 0, i);
					i++;
				}

				// Close the file (save the results to the disk)
				putchar('\n');
				closeFile(&file);
			}
		}
		// If we didn't find the file...
		else
		{
			// If we are creating a new file
			if(input == 'c')
			{
				printf("Creating File...\n");

				// Create a new file structure
				file_t file;

				// Copy over the file name and extension to the file struture's entry
				stringcopy(filename, (char*)file.entry.filename, 8);
				stringcopy(ext, (char*)file.entry.extension, 3);

				// Create the file on our file system (adds the empty file to our floppy disk)
				createFile(&file, &root);
			}
			// None of the following should run, if we couldn't find a file, we cannot delete, read, or write to it!
			else if(input == 'd')
			{
				printf("Error: Tried deleting a file that doesn't exist!\n");
			}
			else if(input == 'r')
			{
				printf("Error: Tried reading a file that doesn't exist!\n");
			}
			else if(input == 'w')
			{
				printf("Error: Tried writing to a file that doesn't exist!\n");
			}
		}	
	}

	exit();
}

