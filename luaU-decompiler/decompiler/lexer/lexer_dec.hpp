#pragma once
#include <stdexcept>
#include <functional>
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
		comparek_aux, /* Compare kvalue for low bit will still be kvalue. */
		memaddr, /* Memory address. */
		proto, /* Proto idx. */
		kvalue, /* Kvalue source. */
		kvalue_dest, /* Kvalue dest. */
		upvalue, /* Upvalue idx. */
		table_idx, /* Table idx. */
		table_reg, /* Table idx reg. */
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
		set_table, /* Set table opcodes. (Not init) */
		upvalue_gs, /* Table get/set. */
		expression, /* Everything else. */
		call /* Call */
	};

	struct lexerme {
		inst_type type = inst_type::expression;
		std::vector<operand_types> operands;
		std::shared_ptr<LuaU_dissassembler::dissassembly> dissassembly;

		/* Gets first. */
		template <lexer_dec::operand_types type>
		std::vector <std::shared_ptr<LuaU_dissassembler::operand>> operand_expr() {

			std::vector <std::shared_ptr<LuaU_dissassembler::operand>> retn;

			for (auto i = 0u; i < this->operands.size(); ++i)
				if (this->operands[i] == type)
					retn.emplace_back(this->dissassembly->operands[i]);

			return retn;
		}

		/* See if opcode starts a scope. */
		bool scope_start() {
			return (this->type == lexer_dec::inst_type::for_ || this->type == lexer_dec::inst_type::branch_condition || this->type == lexer_dec::inst_type::branch);
		}

		template <lexer_dec::operand_types type>
		bool has_operand_expr() {
			return std::find(this->operands.begin(), this->operands.end(), type) != this->operands.end();
		}

		template <lexer_dec::operand_types type>
		std::size_t count_operand_expr() {
			return std::count(this->operands.begin(), this->operands.end(), type);
		}

		template <lexer_dec::operand_types type>
		void operand_expr_callback(std::function<void(const std::shared_ptr<LuaU_dissassembler::operand>& operand, const lexer_dec::operand_types tt)>& callback) {

			const auto operands = this->operand_expr<type>();
			for (const auto& i : operands)
				callback(i, type);

			return;
		}

	};

	std::shared_ptr<lexerme> lexer(std::shared_ptr<LuaU_dissassembler::dissassembly>& dissassembly);

}