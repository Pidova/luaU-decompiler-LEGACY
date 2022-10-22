#include "lexer.hpp"


std::shared_ptr<lexer::lexerme> lexer::lexer(std::shared_ptr<LuaU_dissassembler::dissassembly>& dissassembly) {

	auto retn = std::make_shared<lexer::lexerme>();
	retn->dissassembly = dissassembly;

	switch (dissassembly->op) {

		/* Nothing */
		case LuauOpcode::LOP_NOP:
		case LuauOpcode::LOP_BREAK : {
			retn->type = lexer::inst_type::nothing;
			retn->operands = {};
			break;
		}
		
		/* Call and return. */
		case LuauOpcode::LOP_CALL: {
			retn->type = lexer::inst_type::expression;
			retn->operands = { lexer::operand_types::dest, lexer::operand_types::integer, lexer::operand_types::integer }; /* Operands are dest, int, int */
			break;
		}
		case LuauOpcode::LOP_RETURN: {
			retn->type = lexer::inst_type::expression;
			retn->operands = { lexer::operand_types::reg, lexer::operand_types::integer }; /* Operands are dest, int */
			break;
		}

		/* Branch */
		case LuauOpcode::LOP_JUMPIFEQ:
		case LuauOpcode::LOP_JUMPIFLE:
		case LuauOpcode::LOP_JUMPIFLT:
		case LuauOpcode::LOP_JUMPIFNOTEQ:
		case LuauOpcode::LOP_JUMPIFNOTLE:
		case LuauOpcode::LOP_JUMPIFNOTLT : {
			retn->type = lexer::inst_type::branch;
			retn->operands = { lexer::operand_types::compare, lexer::operand_types::memaddr, lexer::operand_types::compare }; /* Operands are compare, addr, compare */
			break;
		}
		case LuauOpcode::LOP_JUMP:
		case LuauOpcode::LOP_JUMPBACK: {
			retn->type = lexer::inst_type::branch;
			retn->operands = { lexer::operand_types::memaddr }; /* Operands are addr */
			break;

		}
		case LuauOpcode::LOP_JUMPIF:
		case LuauOpcode::LOP_JUMPIFNOT: {
			retn->type = lexer::inst_type::branch;
			retn->operands = { lexer::operand_types::compare, lexer::operand_types::memaddr }; /* Operands are addr */
			break;
		}


		/* Arith */
		case LuauOpcode::LOP_AND:
		case LuauOpcode::LOP_OR:
		case LuauOpcode::LOP_ADD:
		case LuauOpcode::LOP_SUB:
		case LuauOpcode::LOP_MUL:
		case LuauOpcode::LOP_DIV:
		case LuauOpcode::LOP_MOD:
		case LuauOpcode::LOP_POW: {
			retn->type = lexer::inst_type::arith;
			retn->operands = { lexer::operand_types::dest, lexer::operand_types::source, lexer::operand_types::source }; /* Operands are dest, source, source */
			break;
		}
		case LuauOpcode::LOP_ANDK:
		case LuauOpcode::LOP_ORK:
		case LuauOpcode::LOP_ADDK: /* Arith with constant still follows same logic as normal but with a constant. */
		case LuauOpcode::LOP_SUBK:
		case LuauOpcode::LOP_MULK:
		case LuauOpcode::LOP_DIVK:
		case LuauOpcode::LOP_MODK:
		case LuauOpcode::LOP_POWK: {
			retn->type = lexer::inst_type::arith;
			retn->operands = { lexer::operand_types::dest, lexer::operand_types::source, lexer::operand_types::kvalue }; /* Operands are dest, source, kvalue */
			break;
		}
		
		/* Load */
		case LuauOpcode::LOP_LOADN: {
			retn->type = lexer::inst_type::load;
			retn->operands = { lexer::operand_types::dest, lexer::operand_types::integer };
			break;
		}
		case LuauOpcode::LOP_LOADK: {
			retn->type = lexer::inst_type::load;
			retn->operands = { lexer::operand_types::dest, lexer::operand_types::kvalue };
			break;
		}
		case LuauOpcode::LOP_LOADNIL: {
			retn->type = lexer::inst_type::load;
			retn->operands = { lexer::operand_types::dest };
			break;
		}
		case LuauOpcode::LOP_LOADB: {
			retn->type = lexer::inst_type::load;
			retn->operands = { lexer::operand_types::dest, lexer::operand_types::integer, lexer::operand_types::memaddr };
			break;
		}

		/* Global */
		case LuauOpcode::LOP_GETGLOBAL: {
			retn->type = lexer::inst_type::expression;
			retn->operands = { lexer::operand_types::dest, lexer::operand_types::integer, lexer::operand_types::kvalue };
			break;
		}
		case LuauOpcode::LOP_SETGLOBAL: {
			retn->type = lexer::inst_type::expression;
			retn->operands = { lexer::operand_types::source, lexer::operand_types::integer, lexer::operand_types::kvalue_dest };
			break;
		}

		/* Move */
		case LuauOpcode::LOP_MOVE: {
			retn->type = lexer::inst_type::expression;
			retn->operands = { lexer::operand_types::dest, lexer::operand_types::source };
			break;
		}

		/* Upvalue */
		case LuauOpcode::LOP_GETUPVAL: {
			retn->type = lexer::inst_type::upvalue_gs;
			retn->operands = { lexer::operand_types::dest, lexer::operand_types::upvalue };
			break;
		}
		case LuauOpcode::LOP_SETUPVAL: {
			retn->type = lexer::inst_type::upvalue_gs;
			retn->operands = { lexer::operand_types::source, lexer::operand_types::upvalue };
			break;
		}

		/* Closeupvalues, Getimport */
		case LuauOpcode::LOP_CLOSEUPVALS: {
			retn->type = lexer::inst_type::expression;
			retn->operands = { lexer::operand_types::reg };
			break;
		}
		case LuauOpcode::LOP_GETIMPORT: {
			retn->type = lexer::inst_type::expression;
			retn->operands = { lexer::operand_types::dest, lexer::operand_types::kvalue,  lexer::operand_types::integer };
			break;
		}

		/* Table */
		case LuauOpcode::LOP_GETTABLE: {
			retn->type = lexer::inst_type::table_gs;
			retn->operands = { lexer::operand_types::dest, lexer::operand_types::source,  lexer::operand_types::table_idx };
			break;
		}
		case LuauOpcode::LOP_SETTABLE: {
			retn->type = lexer::inst_type::table_gs;
			retn->operands = { lexer::operand_types::source, lexer::operand_types::reg,  lexer::operand_types::table_idx };
			break;
		}
									 
		/* TableK */
		case LuauOpcode::LOP_GETTABLEKS: {
			retn->type = lexer::inst_type::table_gs;
			retn->operands = { lexer::operand_types::dest, lexer::operand_types::source, lexer::operand_types::integer,  lexer::operand_types::table_idx };
			break;
		}
		case LuauOpcode::LOP_SETTABLEKS: {
			retn->type = lexer::inst_type::table_gs;
			retn->operands = { lexer::operand_types::source, lexer::operand_types::reg, lexer::operand_types::integer,  lexer::operand_types::table_idx };
		    break;
		}
		
		/* TablenN */
		case LuauOpcode::LOP_GETTABLEN: {
			retn->type = lexer::inst_type::table_gs;
			retn->operands = { lexer::operand_types::dest, lexer::operand_types::source, lexer::operand_types::table_idx };
			break;
		}
		case LuauOpcode::LOP_SETTABLEN: {
			retn->type = lexer::inst_type::table_gs;
			retn->operands = { lexer::operand_types::source, lexer::operand_types::reg, lexer::operand_types::table_idx };
			break;
		}

		/* Newclosure, Namecall */
		case LuauOpcode::LOP_NEWCLOSURE: {
			retn->type = lexer::inst_type::expression;
			retn->operands = { lexer::operand_types::proto };
			break;
	    }
		case LuauOpcode::LOP_NAMECALL : {
			retn->type = lexer::inst_type::expression;
			retn->operands = { lexer::operand_types::dest, lexer::operand_types::source, lexer::operand_types::integer, lexer::operand_types::table_idx };
			break;
		}

		/* Concat */
		case LuauOpcode::LOP_CONCAT: {
			retn->type = lexer::inst_type::expression;
			retn->operands = { lexer::operand_types::dest, lexer::operand_types::source, lexer::operand_types::source };
			break;
		}

		/* Not, Minus, Length */
		case LuauOpcode::LOP_NOT:
		case LuauOpcode::LOP_MINUS:
		case LuauOpcode::LOP_LENGTH: {
			retn->type = lexer::inst_type::unary;
			retn->operands = { lexer::operand_types::dest, lexer::operand_types::source };
			break;
		}

	    /* Table stuff */
		case LuauOpcode::LOP_NEWTABLE: {
			retn->type = lexer::inst_type::expression;
			retn->operands = { lexer::operand_types::dest, lexer::operand_types::integer, lexer::operand_types::integer };
			break;
		}
		case LuauOpcode::LOP_DUPTABLE: {
			retn->type = lexer::inst_type::expression;
			retn->operands = { lexer::operand_types::dest, lexer::operand_types::integer };
			break;
		}
		case LuauOpcode::LOP_SETLIST: {
			retn->type = lexer::inst_type::expression;
			retn->operands = { lexer::operand_types::dest, lexer::operand_types::source, lexer::operand_types::integer, lexer::operand_types::integer };
			break;
		}
	
		/* Forloop */
		case LuauOpcode::LOP_FORNPREP: {
			retn->type = lexer::inst_type::for_;
			retn->operands = { lexer::operand_types::source, lexer::operand_types::memaddr };
			break;
		}
		case LuauOpcode::LOP_FORNLOOP: {
			retn->type = lexer::inst_type::for_;
			retn->operands = { lexer::operand_types::source, lexer::operand_types::memaddr };
			break;
		}
		case LuauOpcode::LOP_FORGLOOP: {
			retn->type = lexer::inst_type::for_;
			retn->operands = { lexer::operand_types::source, lexer::operand_types::memaddr, lexer::operand_types::integer };
			break;
		}
		case LuauOpcode::LOP_FORGPREP_INEXT: {
			retn->type = lexer::inst_type::for_;
			retn->operands = { lexer::operand_types::source };
			break;
		}
		case LuauOpcode::LOP_FORGPREP_NEXT: {
			retn->type = lexer::inst_type::for_;
			retn->operands = { lexer::operand_types::source };
			break;
		}
 

		/* Getvarargs, Dupclosure, Prepvarargs */
		case LuauOpcode::LOP_GETVARARGS: {
			retn->type = lexer::inst_type::expression;
			retn->operands = { lexer::operand_types::dest, lexer::operand_types::integer };
			break;
		}
		case LuauOpcode::LOP_DUPCLOSURE: {
			retn->type = lexer::inst_type::expression;
			retn->operands = { lexer::operand_types::dest, lexer::operand_types::kvalue };
			break;
		}
		case LuauOpcode::LOP_PREPVARARGS: {
			retn->type = lexer::inst_type::expression;
			retn->operands = { lexer::operand_types::dest };
			break;
		}
		
		/* Extended */
		case LuauOpcode::LOP_LOADKX: {
			retn->type = lexer::inst_type::load;
			retn->operands = { lexer::operand_types::dest, lexer::operand_types::kvalue };
			break;
		}
		case LuauOpcode::LOP_JUMPX: {
			retn->type = lexer::inst_type::branch;
			retn->operands = { lexer::operand_types::memaddr };
			break;
		}

		/* Fasctcall, Coverage, Capture */
		case LuauOpcode::LOP_FASTCALL: {
			retn->type = lexer::inst_type::fastcall;
			retn->operands = { lexer::operand_types::fastcall_idx, lexer::operand_types::memaddr };
			break;
		}
		case LuauOpcode::LOP_COVERAGE: {
			retn->type = lexer::inst_type::expression;
			retn->operands = { lexer::operand_types::integer };
			break;
		}
		case LuauOpcode::LOP_CAPTURE: {
			retn->type = lexer::inst_type::expression;
			if (retn->dissassembly->operands.front()->capture_ref == 2u)
				retn->operands = { lexer::operand_types::capture, lexer::operand_types::upvalue };
			else
				retn->operands = { lexer::operand_types::capture, lexer::operand_types::source };
			break;
		}


		/* Fastcalls */
		case LuauOpcode::LOP_FASTCALL1:	{
			retn->type = lexer::inst_type::fastcall;
			retn->operands = { lexer::operand_types::fastcall_idx, lexer::operand_types::source,  lexer::operand_types::memaddr };
			break;
		}
		case LuauOpcode::LOP_FASTCALL2:	{
			retn->type = lexer::inst_type::fastcall;
			retn->operands = { lexer::operand_types::fastcall_idx, lexer::operand_types::source,  lexer::operand_types::memaddr, lexer::operand_types::source };
			break;
		}
		case LuauOpcode::LOP_FASTCALL2K: {
			retn->type = lexer::inst_type::fastcall;
			retn->operands = { lexer::operand_types::fastcall_idx, lexer::operand_types::source,  lexer::operand_types::memaddr, lexer::operand_types::kvalue };
			break;
		}

	    /* Forloop */
		case LuauOpcode::LOP_FORGPREP: {
			retn->type = lexer::inst_type::for_;
			retn->operands = { lexer::operand_types::reg, lexer::operand_types::memaddr };
			break;
		}

		/* Extended compares */
		case LuauOpcode::LOP_JUMPXEQKNIL: {
			retn->type = lexer::inst_type::branch;
			retn->operands = { lexer::operand_types::compare, lexer::operand_types::memaddr, lexer::operand_types::kvalue };
			break;
		}
		case LuauOpcode::LOP_JUMPXEQKB: {
			retn->type = lexer::inst_type::branch;
			retn->operands = { lexer::operand_types::compare, lexer::operand_types::memaddr, lexer::operand_types::kvalue };
			break;
		}
		case LuauOpcode::LOP_JUMPXEQKN: {
			retn->type = lexer::inst_type::branch;
			retn->operands = { lexer::operand_types::compare, lexer::operand_types::memaddr, lexer::operand_types::kvalue };
			break;
		}
		case LuauOpcode::LOP_JUMPXEQKS: {
			retn->type = lexer::inst_type::branch;
			retn->operands = { lexer::operand_types::compare, lexer::operand_types::memaddr, lexer::operand_types::kvalue };
			break;
		}

		default: {
			throw std::exception("Unkown opcode when lexing.");
		}

	}
			

	return retn;
}