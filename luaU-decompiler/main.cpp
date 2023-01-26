#include "decompiler/decompiler.hpp"
#include "decompiler/loss/loss.hpp"
#include "decompiler/transpiler/transpiler.hpp"
#include "luau-master/Compiler/include/luacode.h"
#include "luau-master/VM/include/lua.h"
#include "luau-master/VM/include/lualib.h"
#include <fstream>
#include <iostream>
#include <sstream>

#if _USRDLL

#endif

/* Throw any string as argument to compile */
std::string compile(const char *const code) {

      /* Transpiler config */
      const auto config = std::make_shared<transpiler_data::transpiler_config>();
      config->post_loss = false;

      /* Compilation data */
      std::size_t size = 0u;
      const auto compilation = luau_compile(code, std::strlen(code), NULL, &size);
      const auto state = luaL_newstate();

      /* Load see if theres something wrong. */
      if (luau_load(state, "Bruh", compilation, size, 0)) {
            throw std::exception("Fix your script pls.");
      }

      return luaU_decompiler::decompile(gco2cl((state->top - 1)->value.gc)->l.p /* Main proto */, config);
}

std::int32_t main() {

      /* Read from compile_me.lua */
      std::stringstream code;
      std::ifstream file("compile_me.lua");

      if (file.is_open()) {
            std::string line = "";
            while (std::getline(file, line)) {
                  code << line << std::endl;
            }
            file.close();
      }

      /* Compile. */
      std::cout << compile(code.str().c_str()) << std::endl;
      std::cin.get();
      return 0;
}