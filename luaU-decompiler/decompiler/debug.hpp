
/* Transpiler */
#define TANSPILER_DEBUG false

#if TANSPILER_DEBUG
	#define TANSPILER_DEBUG_OPERANDS true /* Needs to be enabled to allow comment metrics. */
	#define TANSPILER_DEBUG_OPERANDS_PRINT_OVERRIDE true /* Allows you too override print in debug and just comment it. */
	#define TANSPILER_DEBUG_PREDECOMPILATION false /* Prints node data per iteration. */
	#define TANSPILER_DEBUG_POSTDECOMPILATION false /* Prints decompilation data when everything is done. */
#else /* Nothing */
	#define TANSPILER_DEBUG_OPERANDS false
	#define TANSPILER_DEBUG_OPERANDS_PRINT_OVERRIDE false
	#define TANSPILER_DEBUG_PREDECOMPILATION false 
	#define TANSPILER_DEBUG_POSTDECOMPILATION false
#endif


/* Universal */
#define debug_functions true /* Enable debug functions */