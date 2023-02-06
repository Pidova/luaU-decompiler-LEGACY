#include "../debug.hpp"

#pragma region display_debug

#define display_analysis false        /* Prints pass throughs for ast. */
#define display_analysis_blocks false /* Prints pass throughs for blocks. */
#define display_warnings false        /* Prints warnings. */
#define display_dissassembly false    /* Prints dissassembly of blocks. */
#define display_data true             /* Prints tree and proto data after ast. */

#pragma endregion

#pragma region ast_debug

#define ast_debug true  /* Must be true for display_data too work. */
#define node_debug true /* Must be true for  TANSPILER_DEBUG_OPERANDS or debug_functions or display_data too work. */

#pragma endregion

#pragma region ast_func

#define lv_regs false /* Bases ALL lvs based on reg stack(Can be inaccurate). */

#pragma endregion