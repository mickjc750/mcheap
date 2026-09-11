/*
*/

	#include <stdlib.h>
	#include <stdbool.h>
	#include <stdio.h>
	#include <assert.h>
	#include <limits.h>
	#include <stdint.h>
	#include <inttypes.h>
	#include <math.h>
	#include "../mcheap.h"
	#include "greatest.h"


//********************************************************************************************************
// Configurable defines
//********************************************************************************************************

	#define ALLOCATION_COUNT 8
	#define RANDOM_OP_COUNT 1000000

//********************************************************************************************************
// Local defines
//********************************************************************************************************

	#define ERR_REALLOC_BROKE_ON_INCREASE -1
	#define ERR_REALLOC_BROKE_ON_DECREASE -2
	#define REALLOC_SMALLER 1
	#define REALLOC_SAME 2
	#define REALLOC_BIGGER 3


	#define DBG(_fmtarg, ...) printf("%s:%.4i - "_fmtarg"\n" , __FILE__, __LINE__ ,##__VA_ARGS__)

	GREATEST_MAIN_DEFS();

//********************************************************************************************************
// Public variables
//********************************************************************************************************


//********************************************************************************************************
// Private variables
//********************************************************************************************************

	static uint8_t defrag_buffers[ALLOCATION_COUNT][MCHEAP_SIZE];
	static uint8_t evict_buffers[ALLOCATION_COUNT][MCHEAP_SIZE];
	static uint8_t resize_buffers[ALLOCATION_COUNT][MCHEAP_SIZE];

//********************************************************************************************************
// External prototypes
//********************************************************************************************************

	void*	mcheap_defrag_allocate(size_t size);
	void*	mcheap_defrag_reallocate(void* ptr, size_t size);
	void*	mcheap_defrag_free(void* ptr);
	size_t  mcheap_defrag_largest_free(void);
	bool	mcheap_defrag_is_intact(void);
	void	mcheap_defrag_reinit(void);
	
	void*	mcheap_resize_allocate(size_t size);
	void*	mcheap_resize_reallocate(void* ptr, size_t size);
	void*	mcheap_resize_free(void* ptr);
	size_t  mcheap_resize_largest_free(void);
	bool	mcheap_resize_is_intact(void);
	void	mcheap_resize_reinit(void);

	void*	mcheap_evict_allocate(size_t size);
	void*	mcheap_evict_reallocate(void* ptr, size_t size);
	void*	mcheap_evict_free(void* ptr);
	size_t  mcheap_evict_largest_free(void);
	bool	mcheap_evict_is_intact(void);
	void	mcheap_evict_reinit(void);


//********************************************************************************************************
// Private prototypes
//********************************************************************************************************

	SUITE(suite_defrag);
	TEST test_defrag_realloc_lower(void);
	TEST test_defrag_realloc_shrink_in_place(void);
	TEST test_defrag_realloc_ext_down(void);
	TEST test_defrag_realloc_ext_up(void);
	TEST test_defrag_realloc_higher(void);
	TEST test_defrag_alloc_fail(void);
	TEST test_defrag_max_free(void);
	TEST test_defrag_intact(void);
	TEST test_defrag_random(void);

	SUITE(suite_evict);
	TEST test_evict_realloc_lower(void);
	TEST test_evict_realloc_shrink_in_place(void);
	TEST test_evict_realloc_ext_down(void);
	TEST test_evict_realloc_ext_up(void);
	TEST test_evict_realloc_higher(void);
	TEST test_evict_alloc_fail(void);
	TEST test_evict_max_free(void);
	TEST test_evict_intact(void);
	TEST test_evict_random(void);

	SUITE(suite_resize);
	TEST test_resize_realloc_lower(void);
	TEST test_resize_realloc_shrink_in_place(void);
	TEST test_resize_realloc_ext_down(void);
	TEST test_resize_realloc_ext_up(void);
	TEST test_resize_realloc_higher(void);
	TEST test_resize_alloc_fail(void);
	TEST test_resize_max_free(void);
	TEST test_resize_intact(void);
	TEST test_resize_random(void);

	static int random_realloc(char **ptr_ptr, size_t *size_ptr, uint8_t buf[MCHEAP_SIZE], size_t new_size, void* (*realloc_func)(void*,size_t) );

	static void clutter(char* dst, size_t sz);
	static size_t choose_allocation_size(size_t largest_free);
	static size_t random_size(void);

//********************************************************************************************************
// Public functions
//********************************************************************************************************

int main(int argc, const char* argv[])
{
	GREATEST_MAIN_BEGIN();
	RUN_SUITE(suite_defrag);
	RUN_SUITE(suite_evict);
	RUN_SUITE(suite_resize);
	GREATEST_MAIN_END();

	return 0;
}

//********************************************************************************************************
// Suits
//********************************************************************************************************

SUITE(suite_defrag)
{
	RUN_TEST(test_defrag_realloc_lower);
	RUN_TEST(test_defrag_realloc_shrink_in_place);
	RUN_TEST(test_defrag_realloc_ext_down);
	RUN_TEST(test_defrag_realloc_ext_up);
	RUN_TEST(test_defrag_realloc_higher);
	RUN_TEST(test_defrag_alloc_fail);
	RUN_TEST(test_defrag_max_free);
	RUN_TEST(test_defrag_intact);
	RUN_TEST(test_defrag_random);
}

SUITE(suite_evict)
{
	RUN_TEST(test_evict_realloc_lower);
	RUN_TEST(test_evict_realloc_shrink_in_place);
	RUN_TEST(test_evict_realloc_ext_down);
	RUN_TEST(test_evict_realloc_ext_up);
	RUN_TEST(test_evict_realloc_higher);
	RUN_TEST(test_evict_alloc_fail);
	RUN_TEST(test_evict_max_free);
	RUN_TEST(test_evict_intact);
	RUN_TEST(test_evict_random);
}

SUITE(suite_resize)
{
	RUN_TEST(test_resize_realloc_lower);
	RUN_TEST(test_resize_realloc_shrink_in_place);
	RUN_TEST(test_resize_realloc_ext_down);
	RUN_TEST(test_resize_realloc_ext_up);
	RUN_TEST(test_resize_realloc_higher);
	RUN_TEST(test_resize_alloc_fail);
	RUN_TEST(test_resize_max_free);
	RUN_TEST(test_resize_intact);
	RUN_TEST(test_resize_random);
}

//********************************************************************************************************
// Tests - defrag
//********************************************************************************************************

TEST test_defrag_realloc_lower(void)
{
	mcheap_defrag_reinit();
	char *a = mcheap_defrag_allocate(100);
			  mcheap_defrag_allocate(20);
	char *c = mcheap_defrag_allocate(20);
	char *d = mcheap_defrag_allocate(100);
	clutter(d, 100);
	memcpy(defrag_buffers[0], d, 100);
	mcheap_defrag_free(a);
	mcheap_defrag_free(c);
	d = mcheap_defrag_reallocate(d, 100);	// should not extend down into c, should relocate to a 
	ASSERT_EQ(a, d);
	ASSERT_MEM_EQ(defrag_buffers[0], d, 100);
	PASS();
}

TEST test_defrag_realloc_shrink_in_place(void)
{
	mcheap_defrag_reinit();
	char *a = mcheap_defrag_allocate(50);
			  mcheap_defrag_allocate(20);
	char *c = mcheap_defrag_allocate(100);
	char *d;
	clutter(c, 80);
	memcpy(defrag_buffers[0], c, 80);
	mcheap_defrag_free(a);
	d = mcheap_defrag_reallocate(c, 80);	// should not move, should shrink in place 
	ASSERT_EQ(d, c);
	ASSERT_MEM_EQ(defrag_buffers[0], d, 80);
	PASS();
}

TEST test_defrag_realloc_ext_down(void)
{
	mcheap_defrag_reinit();
			  mcheap_defrag_allocate(100);
	char *c = mcheap_defrag_allocate(20);
	char *d = mcheap_defrag_allocate(100);
	clutter(d, 100);
	memcpy(defrag_buffers[0], d, 100);
	mcheap_defrag_free(c);
	d = mcheap_defrag_reallocate(d, 100);	// should not extend down into c, should relocate to a 
	ASSERT_EQ(d, c);
	ASSERT_MEM_EQ(defrag_buffers[0], d, 100);
	PASS();
}

TEST test_defrag_realloc_ext_up(void)
{
	mcheap_defrag_reinit();
	char *a = mcheap_defrag_allocate(100);
	char *b;
	clutter(a, 100);
	memcpy(defrag_buffers[0], a, 100);
	b = mcheap_defrag_reallocate(a, 200);	// should extend up
	ASSERT_EQ(b, a);
	ASSERT_MEM_EQ(defrag_buffers[0], b, 100);
	PASS();
}

TEST test_defrag_realloc_higher(void)
{
	mcheap_defrag_reinit();
			  mcheap_defrag_allocate(100);
	char *c = mcheap_defrag_allocate(20);
			  mcheap_defrag_allocate(100);
	char *d = mcheap_defrag_allocate(100);
	mcheap_defrag_free(d);
	clutter(c, 20);
	memcpy(defrag_buffers[0], c, 20);
	c = mcheap_defrag_reallocate(c, 50);	// should move to where d was
	ASSERT_EQ(c, d);
	ASSERT_MEM_EQ(defrag_buffers[0], c, 20);
	PASS();
}

TEST test_defrag_alloc_fail(void)
{
	mcheap_defrag_reinit();
	char *a = mcheap_defrag_allocate(MCHEAP_SIZE/2);
	ASSERT(a);
	a = mcheap_defrag_allocate(MCHEAP_SIZE/2);	//overhead should cause this to fail
	ASSERT_EQ(a, NULL);
	PASS();
}

TEST test_defrag_max_free(void)
{
	mcheap_defrag_reinit();
	mcheap_defrag_allocate(1000);
	char *a = mcheap_defrag_allocate(1000);
	char *b = mcheap_defrag_allocate(1000);
	mcheap_defrag_allocate(MCHEAP_SIZE-4000);
	ASSERT(a);
	ASSERT(b);
	ASSERT(mcheap_defrag_largest_free() < 1000);	// top of heap should have just under 1000 due to overhead
	mcheap_defrag_free(a);
	ASSERT(1000 <= mcheap_defrag_largest_free() &&  mcheap_defrag_largest_free() < 1016);	// should now of 1000 where 'a' was
	mcheap_defrag_free(b);
	ASSERT(mcheap_defrag_largest_free() > 2000);	// should now have just over 2000 due to overhead

	a = mcheap_defrag_allocate(mcheap_defrag_largest_free());	// allocate just over 2000
	a = mcheap_defrag_allocate(mcheap_defrag_largest_free());	// allocate just under 1000, this should fill the heap
	ASSERT_EQ(mcheap_defrag_largest_free(), 0);
	PASS();
}

TEST test_defrag_intact(void)
{
	mcheap_defrag_reinit();
  				mcheap_defrag_allocate(100);
	char *c = 	mcheap_defrag_allocate(20);
				mcheap_defrag_allocate(100);
	ASSERT(mcheap_defrag_is_intact());
	memset(c-16,0xFF, 16);	//break it
	ASSERT(!mcheap_defrag_is_intact());

	mcheap_defrag_reinit();
  			mcheap_defrag_allocate(100);
	c = 	mcheap_defrag_allocate(20);
			mcheap_defrag_allocate(100);
	mcheap_defrag_free(c);
	ASSERT(mcheap_defrag_is_intact());
	memset(c-16,0xFF, 16);	//break it
	ASSERT(!mcheap_defrag_is_intact());

	PASS();
}

TEST test_defrag_random(void)
{
	uint32_t count_realloc_bigger = 0;
	uint32_t count_realloc_smaller = 0;
	uint32_t count_realloc_same = 0;
	uint32_t count_allocate = 0;
	uint32_t count_free = 0;
	char* ptrs[ALLOCATION_COUNT] = {0};
	size_t sizes[ALLOCATION_COUNT];
	size_t new_size;
	int i;
	int err;
	uint32_t count = RANDOM_OP_COUNT;
	mcheap_defrag_reinit();
	printf("Testing random heap activity with %"PRIu32" operations\n", count);
	while(count--)
	{
		// allocate or free
		i = rand() % ALLOCATION_COUNT;
		if(ptrs[i])
		{
			if(rand() % 2)
			{
				ptrs[i] = mcheap_defrag_free(ptrs[i]);
				count_free++;
			}
			else
			{
				new_size = choose_allocation_size(mcheap_defrag_largest_free());
				err = random_realloc(&ptrs[i], &sizes[i], defrag_buffers[i], new_size, &mcheap_defrag_reallocate);
				ASSERT_NEQ(ERR_REALLOC_BROKE_ON_DECREASE, err);
				ASSERT_NEQ(ERR_REALLOC_BROKE_ON_INCREASE, err);
				count_realloc_bigger += (err == REALLOC_BIGGER);
				count_realloc_smaller += (err == REALLOC_SMALLER);
				count_realloc_same += (err == REALLOC_SAME);
			};
		}
		else
		{
			sizes[i] = choose_allocation_size(mcheap_defrag_largest_free());
			if(sizes[i])
			{
				ptrs[i] = mcheap_defrag_allocate(sizes[i]);
				clutter(ptrs[i], sizes[i]);
				memcpy(defrag_buffers[i], ptrs[i], sizes[i]);
				count_allocate++;
			};
		};

		//check all existing allocations are intact
		i = 0;
		while(i != ALLOCATION_COUNT)
		{
			if(ptrs[i])
				ASSERT_MEM_EQ(defrag_buffers[i], ptrs[i], sizes[i]);
			i++;
		};

		// check heap integrity
		ASSERT(mcheap_defrag_is_intact());
		if((count & 0x0000FFFF) == 0)
			printf("allocate=%"PRIu32", free=%"PRIu32", realloc_bigger=%"PRIu32", realloc_same=%"PRIu32", realloc_smaller=%"PRIu32", total=%"PRIu32"\n", count_allocate, count_free, count_realloc_bigger, count_realloc_same, count_realloc_smaller, count_allocate+count_free+count_realloc_bigger+count_realloc_same+count_realloc_smaller);
	};
	mcheap_defrag_reinit();
	PASS();
}









//********************************************************************************************************
// Tests - evict
//********************************************************************************************************

TEST test_evict_realloc_lower(void)
{
	mcheap_evict_reinit();
	char *a = mcheap_evict_allocate(100);
			  mcheap_evict_allocate(20);
	char *c = mcheap_evict_allocate(20);
	char *d = mcheap_evict_allocate(100);
	clutter(d, 100);
	memcpy(evict_buffers[0], d, 100);
	mcheap_evict_free(a);
	mcheap_evict_free(c);
	d = mcheap_evict_reallocate(d, 100);	// should not extend down into c, should relocate to a 
	ASSERT_EQ(a, d);
	ASSERT_MEM_EQ(evict_buffers[0], d, 100);
	PASS();
}

TEST test_evict_realloc_shrink_in_place(void)
{
	mcheap_evict_reinit();
	char *a = mcheap_evict_allocate(50);
			  mcheap_evict_allocate(20);
	char *c = mcheap_evict_allocate(100);
	char *d;
	clutter(c, 80);
	memcpy(evict_buffers[0], c, 80);
	mcheap_evict_free(a);
	d = mcheap_evict_reallocate(c, 80);	// should not move, should shrink in place 
	ASSERT_EQ(d, c);
	ASSERT_MEM_EQ(evict_buffers[0], d, 80);
	PASS();
}

TEST test_evict_realloc_ext_down(void)
{
	mcheap_evict_reinit();
			  mcheap_evict_allocate(100);
	char *c = mcheap_evict_allocate(20);
	char *d = mcheap_evict_allocate(100);
	clutter(d, 100);
	memcpy(evict_buffers[0], d, 100);
	mcheap_evict_free(c);
	d = mcheap_evict_reallocate(d, 100);	// should not extend down into c, should relocate to a 
	ASSERT_EQ(d, c);
	ASSERT_MEM_EQ(evict_buffers[0], d, 100);
	PASS();
}

TEST test_evict_realloc_ext_up(void)
{
	mcheap_evict_reinit();
	char *a = mcheap_evict_allocate(100);
	char *b;
	clutter(a, 100);
	memcpy(evict_buffers[0], a, 100);
	b = mcheap_evict_reallocate(a, 200);	// should extend up
	ASSERT_EQ(b, a);
	ASSERT_MEM_EQ(evict_buffers[0], b, 100);
	PASS();
}

TEST test_evict_realloc_higher(void)
{
	mcheap_evict_reinit();
			  mcheap_evict_allocate(100);
	char *c = mcheap_evict_allocate(20);
			  mcheap_evict_allocate(100);
	char *d = mcheap_evict_allocate(100);
	mcheap_evict_free(d);
	clutter(c, 20);
	memcpy(evict_buffers[0], c, 20);
	c = mcheap_evict_reallocate(c, 50);	// should move to where d was
	ASSERT_EQ(c, d);
	ASSERT_MEM_EQ(evict_buffers[0], c, 20);
	PASS();
}

TEST test_evict_alloc_fail(void)
{
	mcheap_evict_reinit();
	char *a = mcheap_evict_allocate(MCHEAP_SIZE/2);
	ASSERT(a);
	a = mcheap_evict_allocate(MCHEAP_SIZE/2);	//overhead should cause this to fail
	ASSERT_EQ(a, NULL);
	PASS();
}

TEST test_evict_max_free(void)
{
	mcheap_evict_reinit();
	mcheap_evict_allocate(1000);
	char *a = mcheap_evict_allocate(1000);
	char *b = mcheap_evict_allocate(1000);
	mcheap_evict_allocate(MCHEAP_SIZE-4000);
	ASSERT(a);
	ASSERT(b);
	ASSERT(mcheap_evict_largest_free() < 1000);	// top of heap should have just under 1000 due to overhead
	mcheap_evict_free(a);
	ASSERT(1000 <= mcheap_evict_largest_free() &&  mcheap_evict_largest_free() < 1016);	// should now of 1000 where 'a' was
	mcheap_evict_free(b);
	ASSERT(mcheap_evict_largest_free() > 2000);	// should now have just over 2000 due to overhead

	a = mcheap_evict_allocate(mcheap_evict_largest_free());	// allocate just over 2000
	a = mcheap_evict_allocate(mcheap_evict_largest_free());	// allocate just under 1000, this should fill the heap
	ASSERT_EQ(mcheap_evict_largest_free(), 0);
	PASS();
}

TEST test_evict_intact(void)
{
	mcheap_evict_reinit();
  				mcheap_evict_allocate(100);
	char *c = 	mcheap_evict_allocate(20);
				mcheap_evict_allocate(100);
	ASSERT(mcheap_evict_is_intact());
	memset(c-16,0xFF, 16);	//break it
	ASSERT(!mcheap_evict_is_intact());

	mcheap_evict_reinit();
  			mcheap_evict_allocate(100);
	c = 	mcheap_evict_allocate(20);
			mcheap_evict_allocate(100);
	mcheap_evict_free(c);
	ASSERT(mcheap_evict_is_intact());
	memset(c-16,0xFF, 16);	//break it
	ASSERT(!mcheap_evict_is_intact());

	PASS();
}

TEST test_evict_random(void)
{
	uint32_t count_realloc_bigger = 0;
	uint32_t count_realloc_smaller = 0;
	uint32_t count_realloc_same = 0;
	uint32_t count_allocate = 0;
	uint32_t count_free = 0;
	char* ptrs[ALLOCATION_COUNT] = {0};
	size_t sizes[ALLOCATION_COUNT];
	size_t new_size;
	int i;
	int err;
	uint32_t count = RANDOM_OP_COUNT;
	mcheap_evict_reinit();
	printf("Testing random heap activity with %"PRIu32" operations\n", count);
	while(count--)
	{
		// allocate or free
		i = rand() % ALLOCATION_COUNT;
		if(ptrs[i])
		{
			if(rand() % 2)
			{
				ptrs[i] = mcheap_evict_free(ptrs[i]);
				count_free++;
			}
			else
			{
				new_size = choose_allocation_size(mcheap_evict_largest_free());
				err = random_realloc(&ptrs[i], &sizes[i], evict_buffers[i], new_size, &mcheap_evict_reallocate);
				ASSERT_NEQ(ERR_REALLOC_BROKE_ON_DECREASE, err);
				ASSERT_NEQ(ERR_REALLOC_BROKE_ON_INCREASE, err);
				count_realloc_bigger += (err == REALLOC_BIGGER);
				count_realloc_smaller += (err == REALLOC_SMALLER);
				count_realloc_same += (err == REALLOC_SAME);
			};
		}
		else
		{
			sizes[i] = choose_allocation_size(mcheap_evict_largest_free());
			if(sizes[i])
			{
				ptrs[i] = mcheap_evict_allocate(sizes[i]);
				clutter(ptrs[i], sizes[i]);
				memcpy(evict_buffers[i], ptrs[i], sizes[i]);
				count_allocate++;
			};
		};

		//check all existing allocations are intact
		i = 0;
		while(i != ALLOCATION_COUNT)
		{
			if(ptrs[i])
				ASSERT_MEM_EQ(evict_buffers[i], ptrs[i], sizes[i]);
			i++;
		};

		// check heap integrity
		ASSERT(mcheap_evict_is_intact());
		if((count & 0x0000FFFF) == 0)
			printf("allocate=%"PRIu32", free=%"PRIu32", realloc_bigger=%"PRIu32", realloc_same=%"PRIu32", realloc_smaller=%"PRIu32", total=%"PRIu32"\n", count_allocate, count_free, count_realloc_bigger, count_realloc_same, count_realloc_smaller, count_allocate+count_free+count_realloc_bigger+count_realloc_same+count_realloc_smaller);
	};
	mcheap_evict_reinit();
	PASS();
}


















//********************************************************************************************************
// Tests - resize
//********************************************************************************************************

TEST test_resize_realloc_lower(void)
{
	mcheap_resize_reinit();
	char *a = mcheap_resize_allocate(100);
			  mcheap_resize_allocate(20);
	char *c = mcheap_resize_allocate(20);
	char *d = mcheap_resize_allocate(100);
	clutter(d, 100);
	memcpy(resize_buffers[0], d, 100);
	mcheap_resize_free(a);
	mcheap_resize_free(c);
	d = mcheap_resize_reallocate(d, 100);	// should not extend down into c, should relocate to a 
	ASSERT_EQ(a, d);
	ASSERT_MEM_EQ(resize_buffers[0], d, 100);
	PASS();
}

TEST test_resize_realloc_shrink_in_place(void)
{
	mcheap_resize_reinit();
	char *a = mcheap_resize_allocate(50);
			  mcheap_resize_allocate(20);
	char *c = mcheap_resize_allocate(100);
	char *d;
	clutter(c, 80);
	memcpy(resize_buffers[0], c, 80);
	mcheap_resize_free(a);
	d = mcheap_resize_reallocate(c, 80);	// should not move, should shrink in place 
	ASSERT_EQ(d, c);
	ASSERT_MEM_EQ(resize_buffers[0], d, 80);
	PASS();
}

TEST test_resize_realloc_ext_down(void)
{
	mcheap_resize_reinit();
			  mcheap_resize_allocate(100);
	char *c = mcheap_resize_allocate(20);
	char *d = mcheap_resize_allocate(100);
	clutter(d, 100);
	memcpy(resize_buffers[0], d, 100);
	mcheap_resize_free(c);
	d = mcheap_resize_reallocate(d, 100);	// should not extend down into c, should relocate to a 
	ASSERT_EQ(d, c);
	ASSERT_MEM_EQ(resize_buffers[0], d, 100);
	PASS();
}

TEST test_resize_realloc_ext_up(void)
{
	mcheap_resize_reinit();
	char *a = mcheap_resize_allocate(100);
	char *b;
	clutter(a, 100);
	memcpy(resize_buffers[0], a, 100);
	b = mcheap_resize_reallocate(a, 200);	// should extend up
	ASSERT_EQ(b, a);
	ASSERT_MEM_EQ(resize_buffers[0], b, 100);
	PASS();
}

TEST test_resize_realloc_higher(void)
{
	mcheap_resize_reinit();
			  mcheap_resize_allocate(100);
	char *c = mcheap_resize_allocate(20);
			  mcheap_resize_allocate(100);
	char *d = mcheap_resize_allocate(100);
	mcheap_resize_free(d);
	clutter(c, 20);
	memcpy(resize_buffers[0], c, 20);
	c = mcheap_resize_reallocate(c, 50);	// should move to where d was
	ASSERT_EQ(c, d);
	ASSERT_MEM_EQ(resize_buffers[0], c, 20);
	PASS();
}

TEST test_resize_alloc_fail(void)
{
	mcheap_resize_reinit();
	char *a = mcheap_resize_allocate(MCHEAP_SIZE/2);
	ASSERT(a);
	a = mcheap_resize_allocate(MCHEAP_SIZE/2);	//overhead should cause this to fail
	ASSERT_EQ(a, NULL);
	PASS();
}

TEST test_resize_max_free(void)
{
	mcheap_resize_reinit();
	mcheap_resize_allocate(1000);
	char *a = mcheap_resize_allocate(1000);
	char *b = mcheap_resize_allocate(1000);
	mcheap_resize_allocate(MCHEAP_SIZE-4000);
	ASSERT(a);
	ASSERT(b);
	ASSERT(mcheap_resize_largest_free() < 1000);	// top of heap should have just under 1000 due to overhead
	mcheap_resize_free(a);
	ASSERT(1000 <= mcheap_resize_largest_free() &&  mcheap_resize_largest_free() < 1016);	// should now of 1000 where 'a' was
	mcheap_resize_free(b);
	ASSERT(mcheap_resize_largest_free() > 2000);	// should now have just over 2000 due to overhead

	a = mcheap_resize_allocate(mcheap_resize_largest_free());	// allocate just over 2000
	a = mcheap_resize_allocate(mcheap_resize_largest_free());	// allocate just under 1000, this should fill the heap
	ASSERT_EQ(mcheap_resize_largest_free(), 0);
	PASS();
}

TEST test_resize_intact(void)
{
	mcheap_resize_reinit();
  				mcheap_resize_allocate(100);
	char *c = 	mcheap_resize_allocate(20);
				mcheap_resize_allocate(100);
	ASSERT(mcheap_resize_is_intact());
	memset(c-16,0xFF, 16);	//break it
	ASSERT(!mcheap_resize_is_intact());

	mcheap_resize_reinit();
  			mcheap_resize_allocate(100);
	c = 	mcheap_resize_allocate(20);
			mcheap_resize_allocate(100);
	mcheap_resize_free(c);
	ASSERT(mcheap_resize_is_intact());
	memset(c-16,0xFF, 16);	//break it
	ASSERT(!mcheap_resize_is_intact());

	PASS();
}

TEST test_resize_random(void)
{
	uint32_t count_realloc_bigger = 0;
	uint32_t count_realloc_smaller = 0;
	uint32_t count_realloc_same = 0;
	uint32_t count_allocate = 0;
	uint32_t count_free = 0;
	char* ptrs[ALLOCATION_COUNT] = {0};
	size_t sizes[ALLOCATION_COUNT];
	size_t new_size;
	int i;
	int err;
	uint32_t count = RANDOM_OP_COUNT;
	mcheap_resize_reinit();
	printf("Testing random heap activity with %"PRIu32" operations\n", count);
	while(count--)
	{
		// allocate or free
		i = rand() % ALLOCATION_COUNT;
		if(ptrs[i])
		{
			if(rand() % 2)
			{
				ptrs[i] = mcheap_resize_free(ptrs[i]);
				count_free++;
			}
			else
			{
				new_size = choose_allocation_size(mcheap_resize_largest_free());
				err = random_realloc(&ptrs[i], &sizes[i], resize_buffers[i], new_size, &mcheap_resize_reallocate);
				ASSERT_NEQ(ERR_REALLOC_BROKE_ON_DECREASE, err);
				ASSERT_NEQ(ERR_REALLOC_BROKE_ON_INCREASE, err);
				count_realloc_bigger += (err == REALLOC_BIGGER);
				count_realloc_smaller += (err == REALLOC_SMALLER);
				count_realloc_same += (err == REALLOC_SAME);
			};
		}
		else
		{
			sizes[i] = choose_allocation_size(mcheap_resize_largest_free());
			if(sizes[i])
			{
				ptrs[i] = mcheap_resize_allocate(sizes[i]);
				clutter(ptrs[i], sizes[i]);
				memcpy(resize_buffers[i], ptrs[i], sizes[i]);
				count_allocate++;
			};
		};

		//check all existing allocations are intact
		i = 0;
		while(i != ALLOCATION_COUNT)
		{
			if(ptrs[i])
				ASSERT_MEM_EQ(resize_buffers[i], ptrs[i], sizes[i]);
			i++;
		};

		// check heap integrity
		ASSERT(mcheap_resize_is_intact());
		if((count & 0x0000FFFF) == 0)
			printf("allocate=%"PRIu32", free=%"PRIu32", realloc_bigger=%"PRIu32", realloc_same=%"PRIu32", realloc_smaller=%"PRIu32", total=%"PRIu32"\n", count_allocate, count_free, count_realloc_bigger, count_realloc_same, count_realloc_smaller, count_allocate+count_free+count_realloc_bigger+count_realloc_same+count_realloc_smaller);
	};
	mcheap_resize_reinit();
	PASS();
}




//********************************************************************************************************
// static functions
//********************************************************************************************************

static int random_realloc(char **ptr_ptr, size_t *size_ptr, uint8_t buf[MCHEAP_SIZE], size_t new_size, void* (*realloc_func)(void*,size_t) )
{
	char *ptr = *ptr_ptr;
	size_t old_size = *size_ptr;
	int retval = 0;

	if(new_size >= old_size)
	{
		ptr = realloc_func(ptr, new_size);				// potentially increase allocation size
		if(memcmp(buf, ptr, old_size))					// check content was not destroyed on size increase
			retval = ERR_REALLOC_BROKE_ON_INCREASE;
		clutter(ptr, new_size);							// create new content
		memcpy(buf, ptr, new_size);
		if(new_size > old_size)
			retval = REALLOC_BIGGER;
		else
			retval = REALLOC_SAME;
	}
	else
	{
		memcpy(buf, ptr, new_size);							// update buffer with smaller content
		ptr = realloc_func(ptr, new_size);					// decrease allocation size
		if(memcmp(buf, ptr, new_size))						// check remaining content was not destroyed
			retval = ERR_REALLOC_BROKE_ON_DECREASE;
		retval = REALLOC_SMALLER;
	};

	*ptr_ptr = ptr; 	
	*size_ptr = new_size;
	return retval;
}

static void clutter(char* dst, size_t sz)
{
	while(sz--)
		*dst++ = (char)rand();
}

static size_t choose_allocation_size(size_t largest_free)
{
	return largest_free == SIZE_MAX ? random_size():(random_size() % (largest_free + 1));
}

static size_t random_size(void)
{
    size_t value = 0;

    for (size_t i = 0; i < sizeof(value) * CHAR_BIT; ++i)
    {
        value <<= 1;
        value |= (size_t)(rand() > RAND_MAX / 2);
    };

    return value;
}