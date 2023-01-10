#include <iostream>
#include <fstream>
#include <sstream>
#include "../../luau-master/VM/include/lua.h"
#include "../../luau-master/Compiler/include/luacode.h"
#include "../../luau-master/VM/include/lualib.h"
#include "loss.hpp"

float loss::loss(const std::shared_ptr<ast_dec::ast>& main_ast, const std::string& decompiled, bool& failed) {

	std::size_t size = 0u;
	const auto compilation = luau_compile(decompiled.c_str(), decompiled.size(), NULL, &size);
	const auto state = luaL_newstate();

	/* Load see if theres something wrong. */
	if (luau_load(state, "Bruh", compilation, size, 0)) {
		failed = true;
		return 0.0f;
	}

	/* Get main proto. */
	const auto proto = gco2cl((state->top - 1)->value.gc)->l.p;
	const auto ast = ast_dec::gen_ast(proto, std::make_shared<transpiler_data::transpiler_config>());

	auto retn = ((std::fmod(main_ast->total, 100)) - (std::fmod(ast->total, 100)));

	if (retn < 0) {
		retn = -retn;
	}

	return retn;
}
