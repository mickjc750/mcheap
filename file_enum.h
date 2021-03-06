/*

Header to enumerate source files in a project.
FILE_ENUM macro will have a unique (uint8_t) value for up to 256 files.

This is formed by taking the address of a single uint8_t variable in each file.
These variables will be grouped together in ram by using the section attribute, and will consume 1 byte of ram per file.
The resulting values are unlikely to start from 0.

If it is desired for these values to start from 0, create a a section in the linker script called file_enum (optional).
This will start numbering from 0, and will also save 1 byte of ram per file (as variables are no longer in the data section).


In the linker script, add this line to the MEMORY command:
MEMORY
{
	file_enum : ORIGIN = 0x00, LENGTH = 0xFF
	...

and add this to the SECTIONS command:
SECTIONS
{
	.file_enum (NOLOAD):
	{
		KEEP(*(.file_enum))
	} > file_enum
	...

To determine which file has been assigned which number, search the .map file for lines containing file_enum
eg.

 .file_enum     0x0000000000000004        0x1 obj/app/src/name_of_file.o

Note that FILE_ENUM values may change from build to build. 
It should exist in a commit along side the build number.

It is unknown if there is a way to make this work with -flto, as -flto produces a confusing .map file output.

*/

//********************************************************************************************************
// Local variables
//********************************************************************************************************

	static volatile uint8_t __attribute__((section (".file_enum"))) file_number;
 
//********************************************************************************************************
// Defines
//********************************************************************************************************

	#define FILE_ENUM	((uint8_t)(intptr_t)(&file_number))

