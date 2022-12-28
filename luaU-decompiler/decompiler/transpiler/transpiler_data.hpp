#pragma once
#include <cstdint>
#include <string>

/* Global transpiler data */
namespace transpiler_data {

	/* Transpiler config */
	struct transpiler_config {

		/* Prefixs. */
		std::string iterator_prefix = "i";
		std::string variable_prefix = "v_";
		std::string loop_variable_prefix = "k";
		std::string loop_variable_prefix_2 = "v";
		std::string arg_prefix = "arg";
		std::string upvalue_prefix = "up_";
		std::string function_prefix = "func_";

		/* Suffix */

		/* Turns suffix digits into english characters. */
		bool upvalue_suffix_char = true;
		bool arg_suffix_char = false;
		bool function_suffix_char = false;

		/* Comment */
		bool include_header = false; /* Includes header per proto and main along with extra stuff like time and full bytecode. */
		bool include_data = false; /* Includes bytecode, disasm per line */

		/* Misc */
		bool smart_variable = false; /* "Smart" variable names. **Ignores prefix(unless unkown) but keeps sufffix** */

	};


}