#include "luau-master/VM/include/lua.h"
#include "luau-master/Compiler/include/luacode.h"
#include "luau-master/VM/include/lualib.h"
#include "decompiler/decompiler.hpp"
#include "decompiler/transpiler/transpiler.hpp"
#include <iostream>

std::int32_t main() {

	const char* const code = "local myArray = { 1, {1, 3, 4}, {3, 4, 5}, 4, 1, {1, 3, 4}, {3, 4, 5}, 4, 1, {1, 3, 4}, {3, 4, 1, {1, 3, 4}, {3, 4, 5}, 4, 1}, 4, }\
						\
		for i, v in ipairs(myArray) do\
			print(tostring(i) .. \" - \" ..v)\
			end";
		/*
		
		repeat \
			if (cmp1 == cmp2) then \
				 print (119); \
				 break; \
			end; \
		 until(oaao and aoaoap == ajajk and ioaoa == aoak and aoapop == ujaja and lmao == kmao);\
		print(111);\
		oopp[\"g\"] = all; local a = { 10, [1000] = www, 1, 2, {[1000] = www, 1}, {[1000] = www, 1}, 1, [1000] = www }; print ({a[10]}); oopp[aaa] = all;
		*/
	/* Compile. */
	std::size_t size = 0u;
	const auto compilation = luau_compile(code, std::strlen(code), NULL, &size);


	const auto state = luaL_newstate();

	/* Load see if theres something wrong. */
	if (luau_load(state, "Bruh", compilation, size, 0))
		throw std::exception("Bruh");

	/* Get main proto. */
	const auto proto = gco2cl((state->top - 1)->value.gc)->l.p;
	std::cout << luaU_decompiler::decompile(proto, std::make_shared<transpiler::transpiler_config>()) << std::endl;
	std::cin.get();
	return 0;
}