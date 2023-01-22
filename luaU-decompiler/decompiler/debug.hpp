#include "color.hpp"

#define debug_functions true /* Enable debug functions */


#define universal_debug true /* Enables universal debug good for debugging. */
#if universal_debug

	/* 1st arg can be str, va_list has too be cstr. */
	#if defined (_WIN32) || defined (_WIN64)
		
		/* Generic */
		#define debug_line(str, ...) { \
					std::printf(color_fontcolor_brightcyan color_background_black "[AST-DEBUG](%s) " color_fontcolor_yellow color_background_black, __FUNCTION__); \
					std::printf(str, __VA_ARGS__); \
					std::printf(color_fontcolor_white color_background_black "\n"); \
				};
		
		/* Undesirable result */
		#define debug_warning(str, ...) { \
					std::printf(color_fontcolor_brightcyan color_background_black "[AST-DEBUG](%s) " color_fontcolor_red color_background_black, __FUNCTION__); \
					std::printf(str, __VA_ARGS__); \
					std::printf(color_fontcolor_white color_background_black "\n"); \
				};
		
		/* Desirable result */
		#define debug_success(str, ...) { \
					std::printf(color_fontcolor_brightcyan color_background_black "[AST-DEBUG](%s) " color_fontcolor_green color_background_black, __FUNCTION__); \
					std::printf(str, __VA_ARGS__); \
					std::printf(color_fontcolor_white color_background_black "\n"); \
				};

		/* Important change */
		#define debug_result(str, ...) { \
					std::printf(color_fontcolor_brightcyan color_background_black "[AST-DEBUG](%s) " color_fontcolor_magenta color_background_black, __FUNCTION__); \
					std::printf(str, __VA_ARGS__); \
					std::printf(color_fontcolor_white color_background_black "\n"); \
				};
		
		#define debug_init(str) std::printf (color_fontcolor_green color_background_black "[" str "-DEBUG] START" color_fontcolor_white color_background_black "\n")
		#define debug_close(str) std::printf (color_fontcolor_green color_background_black "[" str "-DEBUG] CLOSE" color_fontcolor_white color_background_black "\n")
	
	#else

		#define debug_line(str, ...) { \
					std::printf("[AST-DEBUG](%s) ", __FUNCTION__); \
					std::printf(str, __VA_ARGS__); \
					std::printf("\n"); \
				};
		
		#define debug_warning(str, ...) { \
					std::printf("[AST-DEBUG](%s) ", __FUNCTION__); \
					std::printf(str, __VA_ARGS__); \
					std::printf("\n"); \
				};
		
		#define debug_success(str, ...) { \
					std::printf("[AST-DEBUG](%s) ", __FUNCTION__); \
					std::printf(str, __VA_ARGS__); \
					std::printf("\n"); \
				};

		#define debug_result(str, ...) { \
					std::printf("[AST-DEBUG](%s) ", __FUNCTION__); \
					std::printf(str, __VA_ARGS__); \
					std::printf("\n"); \
				};
		
		#define debug_init(str) std::printf ("[" str "-DEBUG] START\n")
		#define debug_close(str) std::printf ("[" str "-DEBUG] CLOSE\n")

	#endif

#else

	#define debug_init(str, ...)
	#define debug_line(str, ...)
	#define debug_warning(str, ...)
	#define debug_success(str, ...)
	#define debug_result(str, ...)
	#define debug_close(str, ...)

#endif


