#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include "../luau-master/VM/src/lbytecode.h"

#define decode_opcode(inst) (inst & 0xFF)
#define decode_A(inst) (((inst) >> 8) & 0xFF) 
#define decode_B(inst) (((inst) >> 16) & 0xFF)
#define decode_C(inst) (((inst) >> 24) & 0xFF)
#define decode_D(inst) (std::int32_t (inst) >> 16)
#define decode_E(inst) (std::int32_t (inst) >> 8)


namespace op_table {

	static constexpr const char* const fastcall_array[] = {
	
		"assert",
		
		"abs",
		"acos",
		"asin",
		"atan2",
		"atan",
		"ceil",
		"cosh",
		"cos",
		"deg",
		"exp",
		"floor",
		"fmod",
		"frexp",
		"ldexp",
		"log10",
		"log",
		"max",
		"min",
		"modf",
		"pow",
		"rad",
		"sinh",
		"sin",
		"sqrt",
		"tanh",
		"tan",
		
		"arshift",
		"band",
		"bnot",
		"bor",
		"bxor",
		"btest",
		"extract",
		"lrotate",
		"lshift",
		"replace",
		"rrotate",
		"rshift",
		
		"type",
		
		"byte",
		"char",
		"len",
		
		"typeof",
		
		"sub",
		
		"clamp",
		"sign",
		"round",
		
		"rawset",
		"rawget",
		"rawequal",
		
		"tinsert",
		"tunpack",
		
		"vector",
		
		"countlz",
		"countrz",
		
		"select",
		
		"rawlen",
		
		"extractk",
		
		"getmetatable",
		"setmetatable",


	};

	enum class operands : std::uint8_t {
		A,
		B,
		C,
		D,
		E,
		AUX,
		AUX_24
	};

	enum class type : std::uint8_t {
		reg,
		val,
		jmp,
		k_idx,
		aux,
		slot,
		upvalue,
		table,
		proto,
		table_size,
		fastcall_idx,
		capture_ref,
		capture_idx,
		import_idx,
		k_value_nstr
	};

	struct optable {
		LuauOpcode op;
		std::vector<op_table::operands> operands;
		std::vector<op_table::type> types;
	};

	static const optable op_table[] = {

		{ LuauOpcode::LOP_NOP, { }, { } }, // 0
		{ LuauOpcode::LOP_BREAK, { }, { } }, // 1

		{ LuauOpcode::LOP_LOADNIL, { op_table::operands::A }, { op_table::type::reg } }, // 2
		{ LuauOpcode::LOP_LOADB, { op_table::operands::A,  op_table::operands::B, op_table::operands::C }, { op_table::type::reg, op_table::type::val, op_table::type::jmp }  }, // 3
		{ LuauOpcode::LOP_LOADN, { op_table::operands::A,  op_table::operands::D }, { op_table::type::reg, op_table::type::val } }, // 4
		{ LuauOpcode::LOP_LOADK, { op_table::operands::A,  op_table::operands::D }, { op_table::type::reg, op_table::type::k_idx } }, // 5
		
		{ LuauOpcode::LOP_MOVE, { op_table::operands::A,  op_table::operands::B }, { op_table::type::reg, op_table::type::reg } }, // 6
		
		{ LuauOpcode::LOP_GETGLOBAL, { op_table::operands::A,  op_table::operands::C,  op_table::operands::AUX }, { op_table::type::reg, op_table::type::slot, op_table::type::k_value_nstr}}, // 7
		{ LuauOpcode::LOP_SETGLOBAL, { op_table::operands::A,  op_table::operands::C,  op_table::operands::AUX }, { op_table::type::reg, op_table::type::slot, op_table::type::k_value_nstr}}, // 8
		
		{ LuauOpcode::LOP_GETUPVAL, { op_table::operands::A,  op_table::operands::B }, { op_table::type::reg, op_table::type::upvalue } }, // 9
		{ LuauOpcode::LOP_SETUPVAL, { op_table::operands::A,  op_table::operands::B }, { op_table::type::reg, op_table::type::upvalue } }, // A
		
		{ LuauOpcode::LOP_CLOSEUPVALS, { op_table::operands::A }, { op_table::type::reg } }, // B
		{ LuauOpcode::LOP_GETIMPORT, { op_table::operands::A,  op_table::operands::D,  op_table::operands::AUX }, { op_table::type::reg, op_table::type::import_idx, op_table::type::val } }, // C
		
		{ LuauOpcode::LOP_GETTABLE, { op_table::operands::A,  op_table::operands::B,  op_table::operands::C }, { op_table::type::reg, op_table::type::reg, op_table::type::reg } }, // D
		{ LuauOpcode::LOP_SETTABLE, { op_table::operands::A,  op_table::operands::B,  op_table::operands::C }, { op_table::type::reg, op_table::type::reg, op_table::type::reg } }, // E
		
		{ LuauOpcode::LOP_GETTABLEKS, { op_table::operands::A,  op_table::operands::B,  op_table::operands::C,  op_table::operands::AUX }, { op_table::type::reg, op_table::type::reg, op_table::type::slot, op_table::type::k_idx } }, // F
		{ LuauOpcode::LOP_SETTABLEKS, { op_table::operands::A,  op_table::operands::B,  op_table::operands::C,  op_table::operands::AUX }, { op_table::type::reg, op_table::type::reg, op_table::type::slot, op_table::type::k_idx } }, // 10
		
		{ LuauOpcode::LOP_GETTABLEN, { op_table::operands::A,  op_table::operands::B,  op_table::operands::C }, { op_table::type::reg, op_table::type::reg, op_table::type::val } }, // 11
		{ LuauOpcode::LOP_SETTABLEN, { op_table::operands::A,  op_table::operands::B,  op_table::operands::C }, { op_table::type::reg, op_table::type::reg, op_table::type::val } }, // 12
	
		{ LuauOpcode::LOP_NEWCLOSURE, { op_table::operands::A,  op_table::operands::D }, { op_table::type::reg, op_table::type::proto } }, // 13
		{ LuauOpcode::LOP_NAMECALL, { op_table::operands::A,  op_table::operands::B,  op_table::operands::C,  op_table::operands::AUX }, { op_table::type::reg, op_table::type::reg, op_table::type::slot, op_table::type::k_idx } }, // 14
		
		{ LuauOpcode::LOP_CALL, { op_table::operands::A,  op_table::operands::B,  op_table::operands::C }, { op_table::type::reg, op_table::type::val, op_table::type::val } }, // 15
		{ LuauOpcode::LOP_RETURN, { op_table::operands::A,  op_table::operands::B }, { op_table::type::reg, op_table::type::val } }, // 16

		{ LuauOpcode::LOP_JUMP, { op_table::operands::D }, { op_table::type::jmp } }, // 17
		{ LuauOpcode::LOP_JUMPBACK, { op_table::operands::D }, { op_table::type::jmp } }, // 18

		{ LuauOpcode::LOP_JUMPIF, { op_table::operands::A, op_table::operands::D }, { op_table::type::reg, op_table::type::jmp } }, // 19
		{ LuauOpcode::LOP_JUMPIFNOT, { op_table::operands::A, op_table::operands::D }, { op_table::type::reg, op_table::type::jmp } }, // 1A

		{ LuauOpcode::LOP_JUMPIFEQ, { op_table::operands::A, op_table::operands::D, op_table::operands::AUX }, { op_table::type::reg, op_table::type::jmp, op_table::type::reg } }, // 1B
		{ LuauOpcode::LOP_JUMPIFLE, { op_table::operands::A, op_table::operands::D, op_table::operands::AUX }, { op_table::type::reg, op_table::type::jmp, op_table::type::reg } }, // 1C
		{ LuauOpcode::LOP_JUMPIFLT, { op_table::operands::A, op_table::operands::D, op_table::operands::AUX }, { op_table::type::reg, op_table::type::jmp, op_table::type::reg } }, // 1D
		{ LuauOpcode::LOP_JUMPIFNOTEQ, { op_table::operands::A, op_table::operands::D, op_table::operands::AUX }, { op_table::type::reg, op_table::type::jmp, op_table::type::reg } }, // 1E
		{ LuauOpcode::LOP_JUMPIFNOTLE, { op_table::operands::A, op_table::operands::D, op_table::operands::AUX }, { op_table::type::reg, op_table::type::jmp, op_table::type::reg } }, // 1F
		{ LuauOpcode::LOP_JUMPIFNOTLT, { op_table::operands::A, op_table::operands::D, op_table::operands::AUX }, { op_table::type::reg, op_table::type::jmp, op_table::type::reg } }, // 20

		{ LuauOpcode::LOP_ADD, { op_table::operands::A,  op_table::operands::B, op_table::operands::C }, { op_table::type::reg, op_table::type::reg,  op_table::type::reg } }, // 21
		{ LuauOpcode::LOP_SUB, { op_table::operands::A,  op_table::operands::B, op_table::operands::C }, { op_table::type::reg, op_table::type::reg,  op_table::type::reg } }, // 22
		{ LuauOpcode::LOP_MUL, { op_table::operands::A,  op_table::operands::B, op_table::operands::C }, { op_table::type::reg, op_table::type::reg,  op_table::type::reg } }, // 23
		{ LuauOpcode::LOP_DIV, { op_table::operands::A,  op_table::operands::B, op_table::operands::C }, { op_table::type::reg, op_table::type::reg,  op_table::type::reg } }, // 24
		{ LuauOpcode::LOP_MOD, { op_table::operands::A,  op_table::operands::B, op_table::operands::C }, { op_table::type::reg, op_table::type::reg,  op_table::type::reg } }, // 25
		{ LuauOpcode::LOP_POW, { op_table::operands::A,  op_table::operands::B, op_table::operands::C }, { op_table::type::reg, op_table::type::reg,  op_table::type::reg } }, // 26

		{ LuauOpcode::LOP_ADDK, { op_table::operands::A,  op_table::operands::B, op_table::operands::C }, { op_table::type::reg, op_table::type::reg,  op_table::type::k_idx } }, // 27
		{ LuauOpcode::LOP_SUBK, { op_table::operands::A,  op_table::operands::B, op_table::operands::C }, { op_table::type::reg, op_table::type::reg,  op_table::type::k_idx } }, // 28
		{ LuauOpcode::LOP_MULK, { op_table::operands::A,  op_table::operands::B, op_table::operands::C }, { op_table::type::reg, op_table::type::reg,  op_table::type::k_idx } }, // 29
		{ LuauOpcode::LOP_DIVK, { op_table::operands::A,  op_table::operands::B, op_table::operands::C }, { op_table::type::reg, op_table::type::reg,  op_table::type::k_idx } }, // 2A
		{ LuauOpcode::LOP_MODK, { op_table::operands::A,  op_table::operands::B, op_table::operands::C }, { op_table::type::reg, op_table::type::reg,  op_table::type::k_idx } }, // 2B
		{ LuauOpcode::LOP_POWK, { op_table::operands::A,  op_table::operands::B, op_table::operands::C }, { op_table::type::reg, op_table::type::reg,  op_table::type::k_idx } }, // 2C

		{ LuauOpcode::LOP_AND, { op_table::operands::A,  op_table::operands::B, op_table::operands::C }, { op_table::type::reg, op_table::type::reg,  op_table::type::reg } }, // 2D
		{ LuauOpcode::LOP_OR, { op_table::operands::A,  op_table::operands::B, op_table::operands::C }, { op_table::type::reg, op_table::type::reg,  op_table::type::reg } }, // 2E

		{ LuauOpcode::LOP_ANDK, { op_table::operands::A,  op_table::operands::B, op_table::operands::C }, { op_table::type::reg, op_table::type::reg,  op_table::type::k_idx } }, // 2F
		{ LuauOpcode::LOP_ORK, { op_table::operands::A,  op_table::operands::B, op_table::operands::C }, { op_table::type::reg, op_table::type::reg,  op_table::type::k_idx } }, // 30

		{ LuauOpcode::LOP_CONCAT, { op_table::operands::A,  op_table::operands::B, op_table::operands::C }, { op_table::type::reg, op_table::type::reg,  op_table::type::reg } }, // 31

		{ LuauOpcode::LOP_NOT, { op_table::operands::A,  op_table::operands::B }, { op_table::type::reg, op_table::type::reg } }, // 32
		{ LuauOpcode::LOP_MINUS, { op_table::operands::A,  op_table::operands::B }, { op_table::type::reg, op_table::type::reg } }, // 33
		{ LuauOpcode::LOP_LENGTH, { op_table::operands::A,  op_table::operands::B }, { op_table::type::reg, op_table::type::reg } }, // 34

		{ LuauOpcode::LOP_NEWTABLE, { op_table::operands::A,  op_table::operands::B,  op_table::operands::AUX }, { op_table::type::reg, op_table::type::table_size, op_table::type::val } }, // 35
		{ LuauOpcode::LOP_DUPTABLE, { op_table::operands::A,  op_table::operands::D }, { op_table::type::reg, op_table::type::k_idx } }, // 36
		{ LuauOpcode::LOP_SETLIST, { op_table::operands::A,  op_table::operands::B,  op_table::operands::C,  op_table::operands::AUX }, { op_table::type::reg, op_table::type::reg, op_table::type::val, op_table::type::val } }, // 37

		{ LuauOpcode::LOP_FORNPREP, { op_table::operands::A,  op_table::operands::D }, { op_table::type::reg, op_table::type::jmp } }, // 38
		{ LuauOpcode::LOP_FORNLOOP, { op_table::operands::A,  op_table::operands::D }, { op_table::type::reg, op_table::type::jmp } }, // 39
		{ LuauOpcode::LOP_FORGLOOP, { op_table::operands::A,  op_table::operands::D, op_table::operands::AUX }, { op_table::type::reg, op_table::type::jmp } }, // 3A
	    
		{ LuauOpcode::LOP_FORGPREP_INEXT, { op_table::operands::A }, { op_table::type::jmp } }, // 3B
		{ LuauOpcode::LOP_DEP_FORGLOOP_INEXT, { }, { } }, // 3C : Depricated

		{ LuauOpcode::LOP_FORGPREP_NEXT, { op_table::operands::A }, { op_table::type::jmp } }, // 3D
		{ LuauOpcode::LOP_DEP_FORGLOOP_NEXT, { }, { } }, // 3E : Depricated

		{ LuauOpcode::LOP_GETVARARGS, { op_table::operands::A,  op_table::operands::B }, { op_table::type::reg, op_table::type::val } }, // 3F
		{ LuauOpcode::LOP_DUPCLOSURE, { op_table::operands::A,  op_table::operands::D }, { op_table::type::reg, op_table::type::k_idx } }, // 40
		{ LuauOpcode::LOP_PREPVARARGS, { op_table::operands::A }, { op_table::type::val } }, // 41

		{ LuauOpcode::LOP_LOADKX, { op_table::operands::A,  op_table::operands::AUX }, { op_table::type::reg, op_table::type::k_idx } }, // 42
		{ LuauOpcode::LOP_JUMPX, { op_table::operands::E }, { op_table::type::jmp } }, // 43

		{ LuauOpcode::LOP_FASTCALL, { op_table::operands::A,  op_table::operands::C }, { op_table::type::fastcall_idx, op_table::type::jmp } }, // 44
		{ LuauOpcode::LOP_COVERAGE, { op_table::operands::E }, { op_table::type::val } }, // 45
		{ LuauOpcode::LOP_CAPTURE, { op_table::operands::A,  op_table::operands::B }, { op_table::type::capture_idx, op_table::type::capture_ref } }, // 46

		{ LuauOpcode::LOP_DEP_JUMPIFEQK, { }, { } }, // 47 : Depricated
		{ LuauOpcode::LOP_DEP_JUMPIFNOTEQK, { }, { } }, // 48 : Depricated

		{ LuauOpcode::LOP_FASTCALL1, { op_table::operands::A,  op_table::operands::B,  op_table::operands::C }, { op_table::type::fastcall_idx, op_table::type::reg, op_table::type::jmp } }, // 49
		{ LuauOpcode::LOP_FASTCALL2, { op_table::operands::A,  op_table::operands::B,  op_table::operands::C, op_table::operands::AUX }, { op_table::type::fastcall_idx, op_table::type::reg, op_table::type::jmp, op_table::type::val } }, // 4A
		{ LuauOpcode::LOP_FASTCALL2K, { op_table::operands::A,  op_table::operands::B,  op_table::operands::C, op_table::operands::AUX }, { op_table::type::fastcall_idx, op_table::type::reg, op_table::type::jmp, op_table::type::k_idx } }, // 4B

		{ LuauOpcode::LOP_FORGPREP, { op_table::operands::A,  op_table::operands::D }, { op_table::type::reg, op_table::type::jmp } }, // 4C

		{ LuauOpcode::LOP_JUMPXEQKNIL, { op_table::operands::A,  op_table::operands::D,  op_table::operands::AUX }, { op_table::type::reg, op_table::type::jmp,  op_table::type::val } }, // 4D
		{ LuauOpcode::LOP_JUMPXEQKB, { op_table::operands::A,  op_table::operands::D,  op_table::operands::AUX }, { op_table::type::reg, op_table::type::jmp,  op_table::type::val } }, // 4E

		{ LuauOpcode::LOP_JUMPXEQKN, { op_table::operands::A,  op_table::operands::D,  op_table::operands::AUX_24 }, { op_table::type::reg, op_table::type::jmp,  op_table::type::k_idx } }, // 4F
		{ LuauOpcode::LOP_JUMPXEQKS, { op_table::operands::A,  op_table::operands::D,  op_table::operands::AUX_24 }, { op_table::type::reg, op_table::type::jmp,  op_table::type::k_idx } } // 50

	};


}