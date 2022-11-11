#pragma once
#include <iostream>


/* Transpiler */
#define TANSPILER_DEBUG false

#if TANSPILER_DEBUG
	#define TANSPILER_DEBUG_OPERANDS true
	#define TANSPILER_DEBUG_PREDECOMPILATION false
	#define TANSPILER_DEBUG_POSTDECOMPILATION false
#endif