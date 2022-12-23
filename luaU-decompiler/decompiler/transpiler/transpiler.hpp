#pragma once
#include "../ast/ast_dec.hpp"

/*

	Convert ast to lua. 

*/

namespace transpiler {

	struct transpiler_config {

		/* Prefixs. */
		std::string iterator_prefix = "i";
		std::string variable_prefix = "v_";
		std::string argument_prefix = "a";
		std::string function_prefix = "func_";
		std::string upvalue_prefix = "upv";
		std::string loop_variable_prefix = "k";
		std::string loop_variable_prefix_2 = "v";

		/* Comment */
		bool include_header = false; /* Includes header per proto and main along with extra stuff like time and full bytecode. */
		bool include_data = false; /* Includes bytecode, disasm per line */

		/* Misc */
		bool smart_variable = false; /* "Smart" variable names. **Ignores prefix(unless unkown) but keeps sufffix** */
	
	};

	std::string transpile(const std::shared_ptr<ast_dec::ast>& main_ast, const std::shared_ptr<transpiler_config>& config);
}