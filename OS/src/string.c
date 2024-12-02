#include "./fat.h"
#include "./io.h"

int stringcompare(char *string0, char *string1, int length)
{
	for(int i = 0; i < length; i++)
	{
		if(string0[i] != string1[i])
		{
			return 0;
		}
	}

	return 1;
}

void printFileName(directory_entry_t *entry)
{
	// Print file name
	for(int i = 0; i < 8; i++)
	{
		putchar((char)entry->filename[i]);
	}

	putchar('.');

	// Print file extension
	for(int i = 0; i < 3; i++)
	{
		putchar((char)entry->extension[i]);
	}

}

void scanfWithPadding(char *string, char paddingChar, int length)
{
	char nextChar = '-';
	for(int i = 0; i < length; i++)
	{
		if(nextChar != '\n')
		{
			nextChar = getchar();
		}
		
		if(nextChar != '\n')
		{
			putchar(nextChar);		
			string[i] = nextChar;
		}
		else
		{
			string[i] = paddingChar;
		}
	}
}

void stringcopy(char *src, char *dest, int length)
{
    for(int i = 0; i < length; i++)
    {
        dest[i] = src[i];
    }
}