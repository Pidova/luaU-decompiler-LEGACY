#include "decompiler/transpiler/transpiler.hpp"
#include "decompiler/ast/ast.hpp"
#include <iostream>

std::int32_t main() {

	const char* const code = "local f = 0; f = nil; if (f == 0) then printf (a); end";

	/* Compile. */
	std::size_t size = 0u;
	const auto compilation = luau_compile(code, std::strlen(code), NULL, &size);

	/* Load. */
	const auto state = luaL_newstate();
	luau_load(state, "Bruh", compilation, size, 0);

	/* Get main proto and dissassemble. */
	auto buffer = std::make_shared<LuaU_dissassembler::dissassembly>();
	const auto proto = gco2cl((state->top - 1)->value.gc)->l.p;

	std::cout << transpiler::transpile(ast::gen_ast(proto), std::make_shared<transpiler::transpiler_config>()) << std::endl;
	std::cin.get();
	return 0;
}