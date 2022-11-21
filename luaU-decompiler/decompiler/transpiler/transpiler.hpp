#pragma once
#include "../ast/ast_dec.hpp"

/*

	Convert ast to lua. 

*/

namespace transpiler {

	struct transpiler_config {

		/* Prefixs. */
		std::string iterator_prefix = "i";
		std::string variable_prefix = "v";
		std::string argument_prefix = "a";
		std::string function_prefix = "func_";
		std::string upvalue_prefix = "upv";
		std::string loop_variable_prefix = "k";
		std::string loop_variable_prefix_2 = "r";

		/* Comment */
		bool include_header = false; /* Includes header per proto and main along with extra stuff like time and full bytecode. */
		bool include_data = false; /* Includes bytecode, disasm per line */
		bool include_metrics = false; /* Includes registers data TANSPILER_DEBUG, TANSPILER_DEBUG_OPERANDS need to be true for it too work can be found in debug.hpp (more info there) */
	};

	std::string transpile(const std::shared_ptr<ast_dec::ast>& main_ast, const std::shared_ptr<transpiler_config>& config);
}