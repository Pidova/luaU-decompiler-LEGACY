#pragma once
#include <iostream>


/* Transpiler */
#define TANSPILER_DEBUG true

#if TANSPILER_DEBUG
	#define TANSPILER_DEBUG_OPERANDS true /* Needs to be enabled to allow comment metrics. */
	#define TANSPILER_DEBUG_OPERANDS_PRINT_OVERRIDE true /* Allows you too override print in debug and just comment it. */
	#define TANSPILER_DEBUG_PREDECOMPILATION false
	#define TANSPILER_DEBUG_POSTDECOMPILATION false
#endif