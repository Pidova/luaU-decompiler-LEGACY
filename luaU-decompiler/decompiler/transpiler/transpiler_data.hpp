#pragma once
#include <cstdint>
#include <string>

/* Global transpiler data (Public) */
namespace transpiler_data {

      /* Transpiler config */
      struct transpiler_config {

            /* Prefixs. */
            std::string iterator_prefix = "i_";
            std::string variable_prefix = "var_";
            std::string loop_variable_prefix = "k_";
            std::string loop_variable_prefix_2 = "v_";
            std::string arg_prefix = "arg";
            std::string upvalue_prefix = "up_";
            std::string function_prefix = "func_";

            /* Suffix */

            /* Turns suffix digits into english characters. */
            bool upvalue_suffix_char = true;
            bool arg_suffix_char = true;
            bool function_suffix_char = true;
            bool iteration_suffix_char = true;
            bool var_suffix_char = true; /* Not applicable to smart names. */

            /* Comment */
            bool include_header = false; /* Includes header per proto and main along with extra stuff like time and full bytecode. */
            bool include_data = false;   /* Includes bytecode, disasm per line */

            /* Misc */
            bool smart_variable = false; /* "Smart" variable names. **Ignores prefix(unless unkown) but keeps suffix as integer** (Won't do anything if suffixs chars config are enabled) */
            bool smart_variable_name = false; /* Same as smart variable but both must be true for suffix names. */

            /* Calculates loss of original and current decompilation. */
            bool post_loss = false;

            /* Emit (Change emittion codes) */
            bool emit_no_parenth_compare = false; /* Changes compare(if/elseif) emittion to if ?? instead of if (??). */
            bool emit_no_last_return = true;      /* Doesn't emit last return if it has no return. */
      };

} // namespace transpiler_data