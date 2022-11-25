#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "../luau-master/Common/include/Luau/Bytecode.h"
#include "../luau-master/VM/src/lobject.h"
#include "Op_table.hpp"

namespace LuaU_dissassembler {


	struct operand {

		op_table::operands operand;
		op_table::type type;

		union {
			std::uint16_t reg;
			std::intptr_t val;
			std::intptr_t jmp;
			std::uintptr_t k_idx;
			std::intptr_t aux;
			std::uintptr_t slot;
			std::uintptr_t upvalue;
			std::uintptr_t table;
			std::uintptr_t proto;
			std::uintptr_t table_size;
			std::uint8_t fastcall_idx;
			std::uint8_t capture_ref;
			std::uint8_t capture_reg;
			std::uintptr_t import_idx;
		};

		std::uintptr_t jmp_addr = 0u;
		std::string k_value = ""; /* Seperate value to represent as a string and idx. Will also serve as import str. */
	};

	struct dissassembly {

		std::uintptr_t addr = 0u;

		LuauOpcode op; 

		const char* mnenomic = "";
		const char* hint = "";

		std::string data = "";
		std::uint8_t len = 0u;

		Instruction* code;

		std::vector<std::shared_ptr<operand>> operands;

	};


	void dissassemble(const std::uintptr_t pc, const Proto* p, std::shared_ptr<dissassembly>& buffer);
}