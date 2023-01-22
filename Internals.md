# Steps
Dissassembly -> Lexer -> AST -> Transpilation -> Decompiled

# Dissassembly
Each instruction gets dissassembly with some information about it. Can be found in dissassembly structure.

# Lexer 
Dissassembly gets marked with keywords too help indexing and see what everything is about, operands, instruction type, etc.
Operands like: dest, source, etc
Instruction Types like: arith, compare, return, normal

# AST
Ast is very huge and is very dependable too get everything right. Exprs tells whats what with all expr information can be found in ast header. (Most errors will occur here)
Steps for AST:
	* Construct ast for current proto and children proto.
	* Blocks act just like scopes which more information about them can be found in ast header.
	  Each block contains its body or called nodes which is all the nodes for that scope.
	  All visitors respect current scope which is why main block is used the most, gets everything more information can be found in ast header.
	* After current ast is constructed it, it gets analyzed with a ton of passes by ast. It adds exprs and data too each node too detail everything.
	  What each expr does can be found in ast header.
	* Then it gets analyzed by post ast which finishes up some stuff.
	* Done with current ast just repeat for all children proto ast. 

# Transpiler
Transpilation takes ast and turns it into code. Each opcode is in transpiler as too make things more accurate as they should instead of more
generalized. Everything will get analyzed and emmited too return as decompilation.



