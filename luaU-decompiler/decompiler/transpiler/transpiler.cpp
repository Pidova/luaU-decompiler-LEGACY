#include <variant>
#include "transpiler.hpp"
#include "../emitter/emitter.hpp"

namespace registers {

	enum class type : std::uint8_t {
		none,
		flag,
		expr,
		var,
		arg,
		global
	};

	struct reg {

		/* Only used by special purpose registers. */
		struct special {
			std::size_t inside_expr = false; /* Inc's for encapsulation. */
		} special;

		type type = type::none;
		std::string data = "";

		std::shared_ptr<reg> clone() {
			auto ptr = std::make_shared<reg>();
			ptr->type = this->type;
			ptr->data = this->data;
			ptr->special.inside_expr = this->special.inside_expr;
			return ptr;
		}

		void replicate(const std::shared_ptr<reg> src) {
			this->type = src->type;
			this->data = src->data;
			this->special.inside_expr = src->special.inside_expr;
			return;
		}

		void clear(const bool keep_type = false) {
			if (!keep_type)
				this->type = type::none;
			this->special.inside_expr = false;
			data.clear();
			return;
		}

		template<registers::type tt>
		void set(const std::string& str) {
			this->type = tt;
			this->data = str;
			return;
		}

	};

	class reg_scope {

		private:
			/*
				-1 : compare flag 
				0+ : general purpose registers.
			*/
			std::unordered_map<std::intptr_t, std::shared_ptr<reg>> registers;

		public:

			/* See if register already exists. */
			bool reg_exists (const std::intptr_t reg) {
				return this->registers.find(reg) != this->registers.end();
			}
			
			std::shared_ptr<reg> operator[](const std::intptr_t reg) {

				if (this->reg_exists(reg))
					return this->registers[reg];
				else {

					auto ptr = std::make_shared<registers::reg>();
					this->registers.insert(std::make_pair(reg, ptr));

					if (reg > -1)
						ptr->type = registers::type::flag;

					return ptr;
				}

			}

			reg_scope clone() {

				reg_scope retn;

				for (const auto i : this->registers)
					retn[i.first]->replicate(i.second);

				return retn;
			}

	};

}

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
#include <iostream>
std::string transpile_block(const std::shared_ptr <ast_dec::block> block, const std::shared_ptr<transpiler::transpiler_config>& config, std::vector<registers::reg_scope>& regs) {

	std::string decompilation = "";

	for (const auto& node : block->nodes) {

		/* Pass expr. */
		for (const auto expr : node->expr) {
			
			for (auto i = 0u; i < expr.second; ++i)
				switch (expr.first) {

					case ast_dec::expr_type::do_: {

						/* Replicate scope. */
						regs.emplace_back(regs.back().clone());

						/* Emit do */
						emitter::str(decompilation, "do {\n");

						break;
					}


					case ast_dec::expr_type::concat_routine_start:
					case ast_dec::expr_type::call_routine_start: {

						/* Inc expr. */
						++regs.back()[-1]->special.inside_expr;

						break;
					}

					case ast_dec::expr_type::concat_routine_end:
					case ast_dec::expr_type::call_routine_end: {

						/* Dec expr. */
						--regs.back()[-1]->special.inside_expr;

						break;
					}

					case ast_dec::expr_type::end_: {

						/* Remove current register. */
						regs.pop_back();

						/* Emit end */
						emitter::str(decompilation, "end\n");

						break;
					}

					case ast_dec::expr_type::break_: {

						/* Emit break */
						emitter::str(decompilation, "break;\n");

						break;
					}

					case ast_dec::expr_type::if_:
					case ast_dec::expr_type::elseif_: {

						/* Emit if/elseif based on compare operand count. */
						if (node->lex->count_operand_expr<lexer_dec::operand_types::compare>() == 2u) {

							/* if/elseif (?? ?? ??) */

							const auto cmp_1 = regs.back()[node->lex->dissassembly->operands[0]->reg];
							const auto cmp_2 = regs.back()[node->lex->dissassembly->operands[2]->reg];

							emitter::compare(node->lex->dissassembly->op, node->branch_extra.opposite, regs.back()[-1]->special.inside_expr, decompilation, (expr.first == ast_dec::expr_type::elseif_) ? "elseif" : "if", cmp_1->data, cmp_2->data);
						}
						else {

							/* if/elseif (??) */

							const auto cmp = regs.back()[node->lex->dissassembly->operands[0]->reg];

							emitter::compare(node->lex->dissassembly->op, node->branch_extra.opposite, regs.back()[-1]->special.inside_expr, decompilation, (expr.first == ast_dec::expr_type::elseif_) ? "elseif" : "if", "", cmp->data);
						}

						/* Remove last scope and replicate next. */
						regs.pop_back();
						regs.emplace_back(regs.back().clone());

						/* Clear compare flag. */
						regs.back()[-1]->clear(true);
						
						break;
					}

					case ast_dec::expr_type::else_: {

						/* Emit else. */
						emitter::str(decompilation, "else\n");

						/* Remove last scope and replicate next. */
						regs.pop_back();
						regs.emplace_back(regs.back().clone());

						/* Clear compare flag. */
						regs.back()[-1]->clear(true);

						break;
					}

				}

		}
		
		/* Instruction handler. */
		switch (node->lex->dissassembly->op) {
		 
			case LuauOpcode::LOP_GETIMPORT: {

				const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>()->reg];
				const auto source = node->lex->operand_expr<lexer_dec::operand_types::kvalue>()->k_value;

				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, source);

				}
				else {

					/* Create arg. */
					if (node->dest_loc.is_dest_loc) {

						dest->set<registers::type::var>(node->dest_loc.name);
						emitter::vararg_equal(decompilation, dest->data, source);

					}
					else {

						/* General purpose. */
						dest->set<registers::type::expr>(source);

					}
				}

				break;
			}

		}

	}

	return decompilation;
}

void transpile_ast(const std::shared_ptr<ast_dec::ast>& main_ast, const std::shared_ptr<transpiler::transpiler_config>& config, std::string& str) {

	registers::reg_scope main_scope;
	std::vector<registers::reg_scope> scopes = { main_scope };

	/* Transpile main block. */
	const auto main = transpile_block(main_ast->main_block, config, scopes);
	
	return;
}

std::string transpiler::transpile(const std::shared_ptr<ast_dec::ast>& main_ast, const std::shared_ptr<transpiler_config>& config) {
	
	std::string retn = "";

	/* Transpile main. */
	transpile_ast(main_ast, config, retn);

	return retn;

}