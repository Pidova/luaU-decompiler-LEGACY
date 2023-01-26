#pragma once
#include "../luau-master/VM/src/lobject.h"
#include "ast/ast_dec.hpp"
#include "transpiler/transpiler.hpp"
#include <memory>
#include <string>

namespace luaU_decompiler {

      std::string decompile(Proto *proto, const std::shared_ptr<transpiler_data::transpiler_config> &config);

}