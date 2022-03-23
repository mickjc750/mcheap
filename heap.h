/*

 */

//********************************************************************************************************
// Public defines
//********************************************************************************************************

//	Provide formatted printing to a heap allocation, requires the prnf module
//	For cross platform compatibility with AVR use:
//	heap_prnf_SL("FORMAT", args...);
//
	#define HEAP_PROVIDE_PRNF

//	If defined heap allocations will include file+line info
//	Source files must define FILE_ENUM with an identifying value 0-255 by including file_enum.h
	#define HEAP_ID_SECTIONS

	#ifdef HEAP_ID_SECTIONS
		#define		heap_allocate(arg1)			heap_allocate_id((arg1), __FILE__, __LINE__)
		#define		heap_reallocate(arg1, arg2)	heap_reallocate_id((arg1), (arg2), __FILE__, __LINE__)
		#define		heap_free(arg1)				heap_free_id((arg1), __FILE__, __LINE__)
	#endif

	#ifdef HEAP_PROVIDE_PRNF
		#ifdef PLATFORM_AVR
			static inline void heap_fmttst(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
			static inline void heap_fmttst(const char* fmt, ...) {}
		#endif

		#ifdef HEAP_ID_SECTIONS
			#define heap_prnf(_fmtarg, ...) 	heap_prnf_id(__FILE__, __LINE__, _fmtarg ,##__VA_ARGS__)
			#ifdef PLATFORM_AVR
				#define heap_prnf_P(_fmtarg, ...) 	heap_prnf_P_id(__FILE__, __LINE__, _fmtarg ,##__VA_ARGS__)
				#define heap_prnf_SL(_fmtarg, ...) 	({char* _prv; _prv = heap_prnf_P_id(__FILE__, __LINE__, PSTR(_fmtarg) ,##__VA_ARGS__); while(0) heap_fmttst(_fmtarg ,##__VA_ARGS__); _prv;})
			#else
				#define heap_prnf_SL(_fmtarg, ...) 	heap_prnf_id(__FILE__, __LINE__, _fmtarg ,##__VA_ARGS__)
			#endif
		#else
			#ifdef PLATFORM_AVR
				#define heap_prnf_SL(_fmtarg, ...) 	({char* _prv; _prv = heap_prnf_P(PSTR(_fmtarg) ,##__VA_ARGS__); while(0) heap_fmttst(_fmtarg ,##__VA_ARGS__); _prv;})
			#else
				#define heap_prnf_SL(_fmtarg, ...) 	heap_prnf(_fmtarg ,##__VA_ARGS__)
			#endif
		#endif
	#endif

	struct heap_leakid_struct
	{
		const char* file_id;
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
		void*	heap_allocate_id(size_t size, const char* id_file, uint16_t id_line);
		void*	heap_reallocate_id(void* org_section, size_t size, const char* id_file, uint16_t id_line);
		void*	heap_free_id(void* address, const char* id_file, uint16_t id_line);
	#else
		void*	heap_allocate(size_t size);
		void*	heap_reallocate(void* org_section, size_t size);
		void*	heap_free(void* address);
	#endif


	#ifdef HEAP_PROVIDE_PRNF
		#ifdef HEAP_ID_SECTIONS
			#ifdef PLATFORM_AVR
				char* heap_prnf_P_id(PGM_P id_file, uint16_t id_line, PGM_P fmt, ...);
			#endif
			char* heap_prnf_id(const char* id_file, uint16_t id_line, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
		#else
			#ifdef PLATFORM_AVR
				char* heap_prnf_P(PGM_P fmt, ...);
			#endif
			char* heap_prnf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
		#endif
	#endif

	//return true if address is within heap space
	bool	heap_contains(void* address);

	//return id of the caller which currently has the most alloactions in the heap
	//requires HEAP_ID_SECTIONS, otherwise returns 0,0,0
	struct heap_leakid_struct heap_find_leak(void);
