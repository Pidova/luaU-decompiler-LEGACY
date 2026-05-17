# luaU-decompiler
As of 5/17/2026, it has been almost 5 years since I started working on decompiler design. 
This project is one of my earliest attempts at building a full decompilation framework and was primarily used for me to learn how decompilers are made when I was around 15.

The implementation is outdated compared to my current designs. However, it served an important role in the architecture I use today.

The most valuable component of this project is the intermediate language (IL), 
which was designed with a structured and reusable representation of program semantics. 
This IL design has remained stable and continues to be reused in my newer decompiler project.

The overall architecture follows a full translation pipeline: Bytecode -> IL -> AST -> IR -> Code generation. 
While the early implementation of this pipeline was experimental, it influenced the design of my current decompiler framework.


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
