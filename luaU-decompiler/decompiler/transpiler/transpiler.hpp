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
		std::string global_variable_prefix = "g";
		std::string argument_prefix = "a";
		std::string function_prefix = "func_";
		std::string upvalue_prefix = "upv";
		std::string loop_variable_prefix = "k";
		std::string loop_variable_prefix_2 = "r";

		/* View */
		bool include_dissassembly = false;
		bool include_bytecode = false;

	};

	std::string transpile(const std::shared_ptr<ast_dec::ast>& main_ast, const std::shared_ptr<transpiler_config>& config);
}