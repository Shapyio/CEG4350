#include "./fat.h"
#include "./fdc.h"
#include <stddef.h>
#include "./string.h"
#include "./io.h"

// FAT Copies
// First copy is fat0 stored at 
// Second copy is fat1
// There were issues declaring the FATs as non-pointers
// When they would get read from floppy, it would overwrite wrong areas of memory
fat_t *fat0;
fat_t *fat1;
void *startAddress = (void *) 0x20000;

// Initialize the file system
// Loads the FATs and root directory
void init_fs(directory_t *directory)
{
    // The FATs and directory are loaded into 0x20000, 0x21200, and 0x22400
    // These addresses were chosen because they are far enough away from the kernel (0x01000 - 0x07000)

    // Read the first copy of the FAT (Drive 0, Cluster 1, 512 bytes * 9 clusters)
    fat0 = (fat_t *) startAddress; // Put FAT at 0x20000
    floppy_read(0, 1,  (void *)fat0, sizeof(fat_t));

    // Read the second copy of the FAT (Drive 0, Cluster 10, 512 bytes * 9 clusters)
    fat1 = (fat_t *) (startAddress+sizeof(fat_t)); // Put FAT at 0x21200
    floppy_read(0, 10, (void *)fat1, sizeof(fat_t));

    // Read the root directory (Drive 0, Cluster 19, 512 bytes * 14 clusters)
    directory->startingAddress = (uint8 *) (startAddress+(sizeof(fat_t)*2)); // Put ROOT at 0x22400
    floppy_read(0, 19, (void *)directory->startingAddress, 512 * 14);

    directory->entry.filename[0] = 'R';
    directory->entry.filename[1] = 'O';
    directory->entry.filename[2] = 'O';
    directory->entry.filename[3] = 'T';
}

void closeFile(file_t *file)
{
    if (file != NULL || file->isOpened == 1)
    {
        uint16 current = file->entry.startingCluster;
        uint32 index = 0;
        uint32 remainingSize = file->entry.fileSize;

        while (remainingSize > 0)
        {
            // Write file contents to storage
            uint32 sectorSize = remainingSize > 512 ? 512 : remainingSize;
            floppy_write(0, 33 + (current - 2), (void *)(file->startingAddress + index), 1);
            index += sectorSize; // Offset by a sector of data
            remainingSize -= sectorSize; // Remove a sector of bytes

            // If no more data to write and at EOF
            if (remainingSize > 0 && fat0->entries[current] == 0xFFFF)
            {
                // Go through all clusters
                for (uint16 i=2; i<2304; i++)
                {
                    // If empty cluster
                    if (fat0->entries[i] == 0x0000)
                    {
                        fat0->entries[current] = i;      // Save cluster addr to current cluster
                        current = i;                     // Set free cluster as current cluster
                        fat0->entries[current] = 0xFFFF; // Mark EOF (the free cluster now EOF)
                        break;
                    }
                }
            }
            else {
                current = fat0->entries[current]; // Set current as new cluster
            }
        }
        file->isOpened = 0; // Make file open false (closed)

    } else {
        return; // Cannot close file, just return
    }
}

int openFile(file_t *file)
{
    if (file != NULL || file->entry.startingCluster >= 33) // Sector 33 is start of kernel executable 
    {
        uint16 current = file->entry.startingCluster;
        uint32 index = 0;
        file->startingAddress = (uint8 *)0x30000; 

        // If not EOF, load more data
        while (current != 0xFFFF)
        {
            floppy_read(0, 33 + (current - 2), (void *)(file->startingAddress + index), 512);
            index += 512; // Offset for next sector of data
            current = fat0->entries[current]; // Get next cluster
        }

        file->isOpened = 1; // Make file open true

    } else {
        return -1;
    }
    return 0;
}

// Creates an empty file and writes it to the floppy disk
// You can see the effects of calling this function by viewing your OS image in a hex editor
// Your OS image will be updated to contain the following:
// FAT Tables changes:
// 0x0266 - 0x0267: 0xFFFF (FAT entry) (First FAT table)
// 0x1466 - 0x1467: 0xFFFF (FAT entry) (Second FAT table)
// Root Directory changes:
// 0x2620 - 0x263F: Directory entry for our new file (contains filename, extension, startingCluster, filesize, etc.)
// File changes:
// 0xA400 - 0xA5FF: Our file (1 sector) contains either 0's or "Hello World!\n"
int createFile(file_t *file, directory_t *parent)
{
    // IFF file and parent exists
    if (file != NULL && parent != NULL && parent->entry.startingCluster != 0)
    {
        directory_entry_t *entry = (directory_entry_t *)parent->startingAddress;
       
        stringcopy((char *)file->entry.filename, (char *)entry->filename, 8);   // Filenames are 8 characters
        stringcopy((char *)file->entry.extension, (char *)entry->extension, 3); // Extensions are 3 chars

        // Set up metadata for new file
        entry->attributes = 0x00;   // Normal file code
        entry->fileSize = 0;        // 0 because no data stored

        for (uint16 i=2; i<2304; i++)
        {
            if (fat0->entries[i] == 0x0000) // If cluster is free/empty
            {
                entry->startingCluster = i;     // Set starting cluster to i (free cluster)
                fat0->entries[i] = 0xFFFF;   // Mark cluster as EOF (because no data stored yet)
                break;
            }
        }
        file->entry = *entry; // Copy entry metadata to file data

        closeFile(file);                // Close file (not in use, just created)
    } else {

        return -1; // No file or parent directory found
    }

    return 0;
}

void deleteFile(file_t *file, directory_t *parent)
{
    if (file != NULL && parent != NULL && parent->entry.startingCluster != 0)
    {
        uint16 current = file->entry.startingCluster;   // get cluster of file

        // Loop thru clusters until EOF (Last cluster)
        while (current != 0xFFFF)
        {
            uint16 next = fat0->entries[current];       // entries[cluster] has next cluster #
            fat0->entries[current] = 0x0000;            // Make cluster empty
            current = next;
        }

        // Sync FAT1 with FAT0
        for (uint16 i=0; i<2304; i++)
            fat1->entries[i] = fat0->entries[i];

        // Updating parent directory by removing directory entry as per instruction
        directory_entry_t *entry = (directory_entry_t *)parent->startingAddress;
        while (entry->filename[0] != 0)
        {
            // Check if filename and extension match
            // Extension is needed because test.py != test.txt, they're different files
            if (stringcompare((char *)entry->filename, (char *)file->entry.filename, 8) && stringcompare((char *)entry->extension, (char *)file->entry.extension, 3))
            {
                entry->filename[0] = 0; // Setting name as 0
                break;
            }
            entry++;
        }

    } else {
        return; // File or parent directory not found
    }
}

// Returns a byte from a file that is currently loaded into memory
// This does NOT modify the floppy disk
// This function requires the file to have been loaded into memory with floppy_read()
uint8 readByte(file_t *file, uint32 index)
{
    if (file != NULL && file->isOpened)
        return *((uint8 *)(file->startingAddress + index));
    return 0;
}

// Writes a byte to a file that is currently loaded into memory
// This does NOT modify the floppy disk
// To write this to the floppy disk, we have to call floppy_write()
int writeByte(file_t *file, uint8 byte, uint32 index)
{
    if (!file->isOpened)
    {
        return -1; //File not opened
    }

    *((uint8 *)(file->startingAddress + index)) = byte; // Add bye to index
    file->entry.fileSize++;                              // Update file size

    return 0;
}

int findFile(char *filename, char* ext, directory_t directory, directory_entry_t *foundEntry)
{
	directory_entry_t *entry = (directory_entry_t *)directory.startingAddress;

	while(entry->filename[0] != 0)
	{
		int fileExists = stringcompare((char *)entry->filename, filename, 8) && stringcompare((char *)entry->extension, ext, 3);
		
		if(fileExists)
		{
			*foundEntry = *entry;
			return 1;
		}

		entry++;
	}

	return 0;
}

// Renames the file in the parent directory entry with the new filename and extension.
// Once the parent directory entry is modified, the changes must be written to the floppy disk using floppy_write()
void renameFile(file_t *file, directory_t *parent, char *newFilename, char *newExtension)
{
    // Pre check
    if (file == NULL || parent == NULL || parent->entry.startingCluster == 0)
    {
        return; // Invalid file or parent directory
    }

    directory_entry_t *entry = (directory_entry_t *)parent->startingAddress;

    // Search for the file in the parent directory
    while (entry->filename[0] != 0)
    {
        if (stringcompare((char *)entry->filename, (char *)file->entry.filename, 8) &&
            stringcompare((char *)entry->extension, (char *)file->entry.extension, 3))
        {
            // Update filename and extension
            stringcopy((char *)entry->filename, newFilename, 8);
            stringcopy((char *)entry->extension, newExtension, 3);

            // Dynamically determine where to write the directory back to floppy disk
            uint16 numSectors = (uint16)((parent->entry.fileSize + 511) / 512); // Total sectors (fileSize rounded up)

            // Write the updated directory back to disk
            floppy_write(0, 33 + (parent->entry.startingCluster - 2), parent->startingAddress, numSectors);

            // Update the file's metadata in memory
            stringcopy((char *)file->entry.filename, newFilename, 8);
            stringcopy((char *)file->entry.extension, newExtension, 3);

            return; // File successfully renamed
        }
        entry++;
    }
}

// Verifies both copies of FAT
// Replaces any inconsistencies with 0x0001, and if any inconsistencies, write both copies of FAT to the disk
// Example:
// if fat0->entries[5] = 6 and fat1->entries[5] = 0, rewrite both to fat0->entries[5] = fat1->entries[5] = 1.
// If no inconsistencies, return 0, if any inconsistencies, return count of how many
int verifyFAT()
{
    int inconsistencies = 0;

    // Compare each entry of fat0 and fat1
    for (uint16 i = 0; i < 2304; i++) // 2304 entries in each FAT
    {
        if (fat0->entries[i] != fat1->entries[i])
        {
            // Correct the inconsistency by setting both to 0x0001
            fat0->entries[i] = 0x0001;
            fat1->entries[i] = 0x0001;

            inconsistencies++;
        }
    }

    // If inconsistencies were found, write both FATs back to disk
    if (inconsistencies > 0)
    {
        // Similar to floppy_read command in init_fs
        floppy_write(0, 1, (void *)fat0, sizeof(fat_t));  // Write first FAT back to disk
        floppy_write(0, 10, (void *)fat1, sizeof(fat_t)); // Write second FAT back to disk
    }

    return inconsistencies; // Return the number of inconsistencies
}

// Load entire boot sector from disk 0 into memory from addresses 0x40000 to 0x401FF
// 1FF = 512
// floppy_read(0, 0, 0x40000, 512);

// The and return the number of clusters that the file_t *file occupies.
uint16 clusterCount(file_t *file)
{
    uint16 current = file->entry.startingCluster;
    uint16 count = 1;   // Counting first cluster

    // If not EOF, load more data
    while (current != 0xFFFF)
    {
        count++;
        current = fat0->entries[current]; // Get next cluster
    }

    return count;
}

/*
* Example of clusterCount:
* Index Entry
* x0002 xFFFF
* x0003 x0007
* x0004 x0003
* x0005 x0004
* x0006 x0002
* x0007 xFFFF
* x0008 xFFFF
*/

// Move clusterA and its data to clusterB in the FAT
int moveCluster(uint16 clusterA, uint16 clusterB)
{
    // Check if clusterB is currently occupied, if so return -1
    if (fat->entries[clusterB] != 0x0000)
    {
        return -1;
    }

    uint16 buffer[512]; // Buffer to save data from cluster

    // Grab data inside sector at Cluster A, 
    floppy_read(0, 33 + (clusterA - 2), (void *)buffer, 512);
    // Move it to Cluster B
    floppy_write(0, 33 + (clusterB - 2), (void *)buffer, 512);

    // Link Cluster B
    fat->entries[clusterB] = fat->entries[clusterA];
    // Now ClusterB points to clusterA's next cluster

    // Clear Cluster A
    fat0->entries[clusterA] = 0x0000; // Mark clusterA as free

    // Save/Update FATs
    // Sync FAT1 with FAT0
    for (uint16 i=0; i<2304; i++)
        fat1->entries[i] = fat0->entries[i];
    return 0;
}

int moveCluster(uint16 clusterA, uint16 clusterB)
{
    // Check if clusterB is currently occupied, if so return -1
    if (fat0->entries[clusterB] != 0x0000)
    {
        return -1; // ClusterB is not free
    }

    // Initialize variables for reading and writing
    uint8 buffer[512]; // Buffer to hold data for one sector
    uint16 current = clusterA; // Start from clusterA
    uint32 index = 0; // Index for the buffer

    // Read data from clusterA
    while (current != 0xFFFF)
    {
        // Read the data from the current cluster into the buffer
        floppy_read(0, 33 + (current - 2), buffer, 512);

        // Write the data to clusterB
        floppy_write(0, 33 + (clusterB - 2), buffer, 512);

        // Move to the next cluster in the chain
        current = fat0->entries[current];

        // If we have more clusters to move, we need to find the next free cluster
        if (current != 0xFFFF)
        {
            // Find the next free cluster for clusterB
            uint16 nextFreeCluster = 0xFFFF; // Initialize to indicate no free cluster found
            for (uint16 i = 2; i < 2304; i++)
            {
                if (fat0->entries[i] == 0x0000) // If cluster is free
                {
                    nextFreeCluster = i; // Found a free cluster
                    break;
                }
            }

            if (nextFreeCluster == 0xFFFF)
            {
                return -1; // No free cluster available to continue moving
            }

            // Update the FAT to link the current cluster to the next free cluster
            fat0->entries[clusterB] = nextFreeCluster; // Link clusterB to the next free cluster
            clusterB = nextFreeCluster; // Move to the next free cluster for the next iteration
        }
    }

    // Clear the original clusterA in the FAT
    fat0->entries[clusterA] = 0x0000; // Mark clusterA as free

    // Sync FAT1 with FAT0
    for (uint16 i = 0; i < 2304; i++)
    {
        fat1->entries[i] = fat0->entries[i];
    }

    return 0; // Success
}