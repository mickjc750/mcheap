/*
*/

	#include <stdlib.h>
	#include <stdbool.h>
	#include <stdio.h>
	#include <assert.h>
	#include <limits.h>
	#include <stdint.h>
	#include <math.h>
	#include "../mcheap.h"
	#include "crc32.h"
	#include "greatest.h"


//********************************************************************************************************
// Configurable defines
//********************************************************************************************************

	#define ALLOCATION_COUNT 8
	#define RANDOM_OP_COUNT 100000

//********************************************************************************************************
// Local defines
//********************************************************************************************************

	#define DBG(_fmtarg, ...) printf("%s:%.4i - "_fmtarg"\n" , __FILE__, __LINE__ ,##__VA_ARGS__)

	GREATEST_MAIN_DEFS();

//********************************************************************************************************
// Public variables
//********************************************************************************************************


//********************************************************************************************************
// Private variables
//********************************************************************************************************

//********************************************************************************************************
// Private prototypes
//********************************************************************************************************

	SUITE(suite_realloc);
	TEST test_realloc_lower(void);
	TEST test_realloc_shrink_in_place(void);
	TEST test_realloc_ext_down(void);
	TEST test_realloc_ext_up(void);
	TEST test_realloc_higher(void);

	SUITE(suite_other);
	TEST test_alloc_fail(void);
	TEST test_max_free(void);
	TEST test_intact(void);
	TEST test_random(void);

	static void clutter(char* dst, size_t sz);
	int choose_allocation_size(void);

//********************************************************************************************************
// Public functions
//********************************************************************************************************

int main(int argc, const char* argv[])
{
	GREATEST_MAIN_BEGIN();
	RUN_SUITE(suite_realloc);
	RUN_SUITE(suite_other);
	GREATEST_MAIN_END();

	return 0;
}

//********************************************************************************************************
// Private functions
//********************************************************************************************************

SUITE(suite_realloc)
{
	RUN_TEST(test_realloc_lower);
	RUN_TEST(test_realloc_shrink_in_place);
	RUN_TEST(test_realloc_ext_down);
	RUN_TEST(test_realloc_ext_up);
	RUN_TEST(test_realloc_higher);
}

SUITE(suite_other)
{
	RUN_TEST(test_alloc_fail);
	RUN_TEST(test_max_free);
	RUN_TEST(test_intact);
	RUN_TEST(test_random);
}

TEST test_realloc_lower(void)
{
	uint32_t crc;
	mcheap_reinit();
	char *a = mcheap_allocate(100);
			  mcheap_allocate(20);
	char *c = mcheap_allocate(20);
	char *d = mcheap_allocate(100);
	clutter(d, 100);
	crc = crc32_add(0, d, 100);
	mcheap_free(a);
	mcheap_free(c);
	d = mcheap_reallocate(d, 100);	// should not extend down into c, should relocate to a 
	ASSERT_EQ(a, d);
	ASSERT_EQ(crc, crc32_add(0, d, 100));
	PASS();
}

TEST test_realloc_shrink_in_place(void)
{
	uint32_t crc;
	mcheap_reinit();
	char *a = mcheap_allocate(50);
			  mcheap_allocate(20);
	char *c = mcheap_allocate(100);
	char *d;
	clutter(c, 80);
	crc = crc32_add(0, c, 80);
	mcheap_free(a);
	d = mcheap_reallocate(c, 80);	// should not move, should shrink in place 
	ASSERT_EQ(d, c);
	ASSERT_EQ(crc, crc32_add(0, d, 80));
	PASS();
}

TEST test_realloc_ext_down(void)
{
	uint32_t crc;
	mcheap_reinit();
			  mcheap_allocate(100);
	char *c = mcheap_allocate(20);
	char *d = mcheap_allocate(100);
	clutter(d, 100);
	crc = crc32_add(0, d, 100);
	mcheap_free(c);
	d = mcheap_reallocate(d, 100);	// should not extend down into c, should relocate to a 
	ASSERT_EQ(d, c);
	ASSERT_EQ(crc, crc32_add(0, d, 100));
	PASS();
}

TEST test_realloc_ext_up(void)
{
	uint32_t crc;
	mcheap_reinit();
	char *a = mcheap_allocate(100);
	char *b;
	clutter(a, 100);
	crc = crc32_add(0, a, 100);
	b = mcheap_reallocate(a, 200);	// should extend up
	ASSERT_EQ(b, a);
	ASSERT_EQ(crc, crc32_add(0, b, 100));
	PASS();
}

TEST test_realloc_higher(void)
{
	uint32_t crc;
	mcheap_reinit();
			  mcheap_allocate(100);
	char *c = mcheap_allocate(20);
			  mcheap_allocate(100);
	char *d = mcheap_allocate(100);
	mcheap_free(d);
	clutter(c, 20);
	crc = crc32_add(0, c, 20);
	c = mcheap_reallocate(c, 50);	// should move to where d was
	ASSERT_EQ(c, d);
	ASSERT_EQ(crc, crc32_add(0, c, 20));
	PASS();
}

TEST test_alloc_fail(void)
{
	mcheap_reinit();
	char *a = mcheap_allocate(MCHEAP_SIZE/2);
	ASSERT(a);
	a = mcheap_allocate(MCHEAP_SIZE/2);	//overhead should cause this to fail
	ASSERT_EQ(a, NULL);
	PASS();
}

TEST test_max_free(void)
{
	mcheap_reinit();
	mcheap_allocate(1000);
	char *a = mcheap_allocate(1000);
	char *b = mcheap_allocate(1000);
	mcheap_allocate(MCHEAP_SIZE-4000);
	ASSERT(a);
	ASSERT(b);
	ASSERT(mcheap_largest_free() < 1000);	// top of heap should have just under 1000 due to overhead
	mcheap_free(a);
	ASSERT(mcheap_largest_free() == 1000);	// should now of 1000 where 'a' was
	mcheap_free(b);
	ASSERT(mcheap_largest_free() > 2000);	// should now have just over 2000 due to overhead

	a = mcheap_allocate(mcheap_largest_free());	// allocate just over 2000
	a = mcheap_allocate(mcheap_largest_free());	// allocate just under 1000, this should fill the heap
	ASSERT_EQ(mcheap_largest_free(), 0);
	PASS();
}

TEST test_intact(void)
{
	mcheap_reinit();
  				mcheap_allocate(100);
	char *c = 	mcheap_allocate(20);
				mcheap_allocate(100);
	ASSERT(mcheap_is_intact());
	c--;
	c[0] = 0xFF;	// break a used section
	ASSERT(!mcheap_is_intact());

	mcheap_reinit();
  			mcheap_allocate(100);
	c = 	mcheap_allocate(20);
			mcheap_allocate(100);
	mcheap_free(c);
	ASSERT(mcheap_is_intact());
	c[-1] = 0xFF;	//break the free list
	ASSERT(!mcheap_is_intact());

	PASS();
}

TEST test_random(void)
{
	char* ptrs[ALLOCATION_COUNT] = {0};
	uint32_t crcs[ALLOCATION_COUNT];
	int sizes[ALLOCATION_COUNT];
	int i;
	uint32_t count = RANDOM_OP_COUNT;
	mcheap_reinit();
	while(count--)
	{
		// allocate or free
		i = rand() % ALLOCATION_COUNT;
		if(ptrs[i])
		{
			ptrs[i] = mcheap_free(ptrs[i]);
		}
		else
		{
			sizes[i] = choose_allocation_size();
			if(sizes[i])
			{
				ptrs[i] = mcheap_allocate(sizes[i]);
				clutter(ptrs[i], sizes[i]);
				crcs[i] = crc32_add(0, ptrs[i], sizes[i]);
			};
		};

		//check existing allocations are intact
		i = 0;
		while(i != ALLOCATION_COUNT)
		{
			if(ptrs[i])
				ASSERT_EQ(crcs[i], crc32_add(0, ptrs[i], sizes[i]));
			i++;
		};

		// check heap integrity
		ASSERT(mcheap_is_intact());
	};
	mcheap_reinit();
	PASS();
}

static void clutter(char* dst, size_t sz)
{
	while(sz--)
		*dst++ = (char)rand();
}

int choose_allocation_size(void)
{
	int retval = 0;	
	size_t lf = mcheap_largest_free();

	if(lf == 1)
		retval = 1;
	else if(lf > 1)
		retval = rand()%((int)(lf-1));
	return retval;
}
