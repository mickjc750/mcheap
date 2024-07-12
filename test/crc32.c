/*
*/
	
	#include <stdbool.h>
	#include "crc32.h"

//********************************************************************************************************
// Configurable defines
//********************************************************************************************************
	
	#define POLYNOMIAL	0x04C11DB7ul

//********************************************************************************************************
// Local defines
//********************************************************************************************************

	#define UPPER_BIT		0x80000000
	#define BITS_IN_BYTE	8

//********************************************************************************************************
// Public variables
//********************************************************************************************************

//********************************************************************************************************
// Private variables
//********************************************************************************************************

//********************************************************************************************************
// Private prototypes
//********************************************************************************************************

//********************************************************************************************************
// Public functions
//********************************************************************************************************

uint32_t crc32_byte(uint32_t crc, uint8_t x)
{
	uint8_t bitcount = BITS_IN_BYTE;
	bool carry = false;
	while(bitcount--)
	{
		carry = !!(crc & UPPER_BIT);
		crc <<= 1;
		crc |= !!(x & 0x80);
		x <<= 1;
		if(carry)
			crc ^= POLYNOMIAL;
	};

	return crc;
}

uint32_t crc32_add(uint32_t crc, const void* source_ptr, size_t size)
{
	const uint8_t *tempu8_ptr = source_ptr;

	if(source_ptr == NULL)
	{
		crc = 0;
		size = 0;
	};

	while(size--)
	{
		crc = crc32_byte(crc, *tempu8_ptr);
		tempu8_ptr++;
	};

	return crc;
}

//********************************************************************************************************
// Private functions
//********************************************************************************************************

