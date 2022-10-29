#pragma once
#include "../../luau-master/Common/include/Luau/Bytecode.h"
#include "../../luau-master/VM/src/lobject.h"
#include "../../luau-master/VM/src/lstate.h"
#include "../../dissassembler/Dissassembler.hpp"

/* 

	Standard lexer.

*/

namespace lexer_dec {

	enum class operand_types : std::uint8_t {
		reg, /* Normal register not a dest. */
		dest, /* Reg dest. */
		source, /* Reg source. */
		integer, /* Integer source. */
		compare, /* Compare register. */
		memaddr, /* Memory address. */
		proto, /* Proto idx. */
		kvalue, /* Kvalue source. */
		kvalue_dest, /* Kvalue dest. */
		upvalue, /* Upvalue idx. */
		table_idx, /* Table idx. */
		fastcall_idx, /* Fastcall function idx. */
		capture /* Capture type. */
	};

	enum class inst_type : std::uint8_t {
		nothing, /* Nop and Break opcodes. */
		arith, /* Arith opcodes add, sub, and, or, etc. */
		branch_condition, /* Condition branching opcodes. */
		branch, /* Branching opcodes. */
		load, /* Load opcodes. */
		fastcall, /* Fastcall opcodes. */
		for_, /* For loop opcodes. */
		unary, /* Minus, Not, and Lenght. */
		table_get, /* Table get. */
		table_set, /* Table set. */
		upvalue_gs, /* Table get/set. */
		expression, /* Everything else. */
		call /* Call */
	};

	struct lexerme {
		inst_type type = inst_type::expression;
		std::vector<operand_types> operands;
		std::shared_ptr<LuaU_dissassembler::dissassembly> dissassembly;
	};

	std::shared_ptr<lexerme> lexer(std::shared_ptr<LuaU_dissassembler::dissassembly>& dissassembly);


}