#pragma once
#include <memory>
#include <string>
#include "ast/ast_dec.hpp"
#include "transpiler/transpiler.hpp"
#include "../luau-master/VM/src/lobject.h"

namespace luaU_decompiler {

	std::string decompile(Proto* proto, const std::shared_ptr<transpiler::transpiler_config>& config);

}