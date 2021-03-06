/*

 */

//********************************************************************************************************
// Public defines
//********************************************************************************************************

//	If defined heap allocations will include file+line info
//	Source files must define FILE_ENUM with an identifying value 0-255
	#define HEAP_ID_SECTIONS

	#ifdef HEAP_ID_SECTIONS
		#define		heap_allocate(arg1)			heap_allocate_id((arg1), FILE_ENUM, __LINE__)
		#define		heap_reallocate(arg1, arg2)	heap_reallocate_id((arg1), (arg2), FILE_ENUM, __LINE__)
		#define		heap_free(arg1)				heap_free_id((arg1), FILE_ENUM, __LINE__)
	#endif

	struct heap_leakid_struct
	{
		uint8_t		file_id;
		uint16_t	line_id;
		uint32_t	cnt;
	};

//********************************************************************************************************
// Public variables
//********************************************************************************************************

	//the minimum free space which has occurred since heap_init() (requires TRACK_STATS)
	extern size_t	heap_head_room;	

	//the current largest free allocatable size (requires TRACK_STATS)
	extern size_t 	heap_largest_free;

	//the current number of allocations
	extern uint32_t	heap_allocations;

	//the maximum number of allocations that has occurred
	extern uint32_t	heap_allocations_max;

//********************************************************************************************************
// Public prototypes
//********************************************************************************************************

	void 		heap_init(void);

	#ifdef	HEAP_ID_SECTIONS
		void*	heap_allocate_id(size_t size, uint8_t id_file, uint16_t id_line);
		void*	heap_reallocate_id(void* org_section, size_t size, uint8_t id_file, uint16_t id_line);
		void*	heap_free_id(void* address, uint8_t id_file, uint16_t id_line);
	#else
		void*	heap_allocate(size_t size);
		void*	heap_reallocate(void* org_section, size_t size);
		void*	heap_free(void* address);
	#endif

	//return true if address is within heap space
	bool		heap_contains(void* address);

	//return id of the caller which currently has the most alloactions in the heap
	//requires HEAP_ID_SECTIONS, otherwise returns 0,0,0
	struct heap_leakid_struct heap_find_leak(void);
