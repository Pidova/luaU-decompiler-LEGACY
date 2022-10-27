#include "luau-master/VM/include/lua.h"
#include "luau-master/Compiler/include/luacode.h"
#include "luau-master/VM/include/lualib.h"
#include "decompiler/transpiler/transpiler.hpp"
#include "decompiler/ast/ast_dec.hpp"
#include <iostream>

std::int32_t main() {

	const char* const code = "for i,v in pairs(1, 2) do print (1, i, v) local p = 0; print(p /0); end ";

	/* Compile. */
	std::size_t size = 0u;
	const auto compilation = luau_compile(code, std::strlen(code), NULL, &size);

	/* Load. */
	const auto state = luaL_newstate();
	luau_load(state, "Bruh", compilation, size, 0);

	/* Get main proto and dissassemble. */
	auto buffer = std::make_shared<LuaU_dissassembler::dissassembly>();
	const auto proto = gco2cl((state->top - 1)->value.gc)->l.p;

	std::cout << transpiler::transpile(ast_dec::gen_ast(proto), std::make_shared<transpiler::transpiler_config>()) << std::endl;
	std::cin.get();
	return 0;
}