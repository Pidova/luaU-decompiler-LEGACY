#include "luau-master/VM/include/lua.h"
#include "luau-master/Compiler/include/luacode.h"
#include "luau-master/VM/include/lualib.h"
#include "decompiler/decompiler.hpp"
#include "decompiler/transpiler/transpiler.hpp"
#include <iostream>

std::int32_t main() {

	const char* const code = " local function test(a, b, c, d, e, f, g, h) function fun (a, f, d) a = 10; b = 20; d = e; f = h; g = o; a = s; end fun(a, c, e, g, (function(x, z, y) x = d; z = e; g = h; a = o; end)); end test (test)";

	/*
	repeat \
			if(a) then  break; elseif (ai) then break; end;\
		print (1);\
		until(((ame or ai or ia or ame or ai or ia or aa or ame or ai or ia or aa or ame or ai or ia or aa) and (ame or ai or ia or ame or ai or ia or aa or ame or ai or ia or aa or ame or ai or ia or aa)) or ((ame or ai or ia or ame or ai or ia or aa or ame or ai or ia or aa or ame or ai or ia or aa) and (ame or ai or ia or ame or ai or ia or aa or ame or ai or ia or aa or ame or ai or ia or aa)));\
		print (1);\

	repeat \
				local a = peen(); \
					while (wowo and iaia and iaai and iao and aoioa) do \
						a = 1000;\
						if ((oaao and aoaoap == ajajk and ioaoa == aoak) and (oaao and aoaoap == ajajk and ioaoa == aoak) or (oaao and aoaoap == ajajk and ioaoa == aoak)) then break; end\
						printf (AA)\
					end\
					if (cmp1 == cmp2) then \
						 print (119); \
						 break; \
					end; \
				if (a or a == 100) then break; end;\
				if ((oaao and aoaoap == ajajk and ioaoa == aoak) and (oaao and aoaoap == ajajk and ioaoa == aoak) or (oaao and aoaoap == ajajk and ioaoa == aoak)) then break; end \
				a = 1000; \
		    until((oaao and aoaoap == ajajk and ioaoa == aoak) and (oaao and aoaoap == ajajk and ioaoa == aoak) or (oaao and aoaoap == ajajk and ioaoa == aoak));\
		print(111);\
		oopp[\"g\"] = all; \
		print ({a[10]}); oopp[aaa] = all;
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