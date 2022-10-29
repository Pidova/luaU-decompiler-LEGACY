#include <variant>
#include "transpiler.hpp"
#include "../emitter/emitter.hpp"

namespace instruction_handler {

	namespace emitters {

		namespace assignment {

			/* Emit inst data to decompiled. */
			template<LuauOpcode o>
			void emit_inst(std::string& dest, const std::shared_ptr<ast_dec::registers::reg>& src_1, const std::variant <std::shared_ptr<ast_dec::registers::reg>, std::string> src_2 = nullptr) {

				switch (o) {

				/* Arith */
				case LuauOpcode::LOP_AND:
				case LuauOpcode::LOP_OR:
				case LuauOpcode::LOP_ADD:
				case LuauOpcode::LOP_SUB:
				case LuauOpcode::LOP_MUL:
				case LuauOpcode::LOP_DIV:
				case LuauOpcode::LOP_MOD:
				case LuauOpcode::LOP_POW: {

					/* Check too see if register getting passed is a vararg. */
					if (src_1->tt != ast_dec::registers::type::vararg)
						throw std::exception("Attempted to emit non-vararg in arith when trying too emit lvalue in ast.");

					const auto str = std::get<std::shared_ptr<ast_dec::registers::reg>>(src_2)->container;
					emitter::arith(o, true, dest, src_1->container, str);

					break;
				}

				case LuauOpcode::LOP_ANDK:
				case LuauOpcode::LOP_ORK:
				case LuauOpcode::LOP_ADDK:
				case LuauOpcode::LOP_SUBK:
				case LuauOpcode::LOP_MULK:
				case LuauOpcode::LOP_DIVK:
				case LuauOpcode::LOP_MODK:
				case LuauOpcode::LOP_POWK: {

					/* Check too see if register getting passed is a vararg. */
					if (src_1->tt != ast_dec::registers::type::vararg)
						throw std::exception("Attempted to emit non-vararg in arithk when trying too emit lvalue in ast.");

					const auto str = std::get<std::string>(src_2);
					emitter::arith(o, true, dest, src_1->container, str);

					break;
				}

				case LuauOpcode::LOP_SETGLOBAL: {
					const auto str = std::get<std::shared_ptr<ast_dec::registers::reg>>(src_2)->container;
					emitter::vararg_equal(dest, src_1->container, str);
					break;
				}

				default: {
					throw std::exception("Unkown opcode when trying too emit lvalue in ast.");
				}

				}

			}

		}

	}

	
}


std::string transpiler::transpile(const std::shared_ptr<ast_dec::ast>& main_ast, const std::shared_ptr<transpiler_config>& config) {
	return "";
}