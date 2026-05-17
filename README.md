# luaU-decompiler

As of 5/17/2026, it has been almost 5 years since I started making decompilers. This project is very poorly made and should only be referenced on how not to make a decompiler, somewhat.
The only good aspect of this project is that the IL is really well made, and it is still used to this day in my newer decompiler. The idea of this project was to go from
Bytecode -> IL -> AST -> IR -> Code gen, still kind of the same framework as my newer decompilers, just serves as mostly an attempt when I was around 15 and learning decompilers.



# Description

    * This is a decompiler LuaU: https://github.com/Roblox/luau/
    * It is based off of a ast, abstract syntax tree. Each routine and expression has it's own pass through with some of the routines sharing some stuff.
    * Everything you need for debugging can be found in the debugging files, "debug.hpp", "ast_config.hpp", etc.

# Usage

    Code:
        * If you wan't to test it out with code just go to compile_me.lua (MUST BE UTF-8) where it'll get compiled and decompiled and you can compare results.
          
    Bytecode:
        * If you have something that has already been compiled just create a char point for it and pass it through luau_load get proto and make ast from it with     config. Just refer to main.cpp.


# Config

    Configs for the decompiler are transpiler::transpiler_config
 
# Help
 
    If you find a bug with it or something that can be added let me know in Issues.
  
# Modifications
  
    Some stuff has been changed with LuaU to make it easier to decompile and represent in the ast (check luau_load).

# Updating

    Updating too the latest LuaU is pretty easy. You can add the new files from LuaU repository for the new stuff.
    If they added new instructions to their ISA this will be updated in some time too match it.
