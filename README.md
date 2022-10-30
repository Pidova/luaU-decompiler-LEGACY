# luaU-decompiler
# Rn its supper unfinished but the old one i made is getting ported over to this
# Description

  * This is a luaU decompiler for https://github.com/Roblox/luau/
  
# Usage

  Code:
  * If you wan't to test it out with code just go to main.cpp where it'll get compiled and decompiled and you can compare results.
  
  Bytecode:
  * If you have something that has already been compiled just create a char point for it and pass it through luau_load get proto and make ast from it with     config. Just refer to main.cpp.


# Config

  * Configs for the decompiler are transpiler::transpiler_config
 
# Help
 
  * if you find a bug with it or something that can be added let me know in Issues.
  
# Modifications
  
  * Some stuff has been changed with luaU to make it easier to decompile and represent in the ast.
    
# Changes
    
  * Will somewhat try to keep the decompiler up to date whit the recent ISA changes for luaU.
