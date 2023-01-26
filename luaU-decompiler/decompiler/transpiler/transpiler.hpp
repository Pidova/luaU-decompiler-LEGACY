#pragma once
#include "../ast/ast_dec.hpp"

/*

	Convert ast to lua. 

*/

namespace transpiler {

      std::string transpile(const std::shared_ptr<ast_dec::ast> &main_ast, const std::shared_ptr<transpiler_data::transpiler_config> &config);

}