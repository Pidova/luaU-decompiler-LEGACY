#include <variant>
#include <sstream>
#include "transpiler.hpp"
#include "../emitter/emitter.hpp"
#include "../debug.hpp"

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

		const char* const str_type() {

			switch (this->type) {

				case type::none: {
					return "none";
				}
				case type::flag: {
					return "flag";
				}
				case type::expr: {
					return "expr";
				}
				case type::var: {
					return "var";
				}
				case type::arg: {
					return "arg";
				}
				case type::global: {
					return "global";
				}

			}

			throw std::exception("Unkown type for register str.");
		}

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

std::string transpile_block(const std::shared_ptr<ast_dec::ast>& ast, const std::shared_ptr <ast_dec::block> block, const std::shared_ptr<transpiler::transpiler_config>& config, std::vector<registers::reg_scope>& regs) {

	std::string decompilation = "";

	for (const auto& node : block->nodes) {

		/* Pass expr. */
		for (const auto expr : node->expr) {
			
			for (auto i = 0u; i < expr.second; ++i)
				switch (expr.first) {

					case ast_dec::expr_type::repeat_: {

						/* Replicate scope. */
						regs.emplace_back(regs.back().clone());

						/* Emit do */
						emitter::str(decompilation, "repeat\n");

						break;
					}

					case ast_dec::expr_type::table_start:
					case ast_dec::expr_type::concat_routine_start:
					case ast_dec::expr_type::call_routine_start: {

						/* Write table start to reg. */
						if (expr.first == ast_dec::expr_type::table_start) {

							if (!node->lex->has_operand_expr<lexer_dec::operand_types::dest>())
								throw std::exception("No dest operand for table start.");

							const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg];
							dest->set<registers::type::expr>("{ ");
						}
						
						/* Inc expr. */
						++regs.back()[-1]->special.inside_expr;

						break;
					}

					case ast_dec::expr_type::table_end:
					case ast_dec::expr_type::concat_routine_end:
					case ast_dec::expr_type::call_routine_end: {

						/* Concat with table end dest. */
						if (expr.first == ast_dec::expr_type::table_end) {

							if (!node->lex->has_operand_expr<lexer_dec::operand_types::dest>())
								throw std::exception("No dest operand for table end.");

							const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg];
							dest->data += " }";
						}

						/* Dec expr. */
						--regs.back()[-1]->special.inside_expr;

						break;
					}

					case ast_dec::expr_type::scope_end: {

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

					case ast_dec::expr_type::while_: {

						/* Emit else. */
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

				}

		}
		
		#if TANSPILER_DEBUG_OPERANDS 

			std::stringstream str;

			#if !TANSPILER_DEBUG_OPERANDS_PRINT_OVERRIDE
				str << "[transpiler.cpp] " << node->lex->dissassembly->data << std::endl;
			#elif TANSPILER_DEBUG_OPERANDS_PRINT_OVERRIDE
				str << "[Metric]: " << node->lex->dissassembly->data << std::endl;
			#endif

			for (auto i = 0u; i < node->lex->operands.size(); ++i) {
				
				const auto oper = node->lex->dissassembly->operands[i];

				switch (node->lex->operands[i]) {

					case lexer_dec::operand_types::reg: {

						if (!regs.back().reg_exists(oper->reg))
							str << "*	[reg( " << oper->reg << " )]: NULL\n";
						else {
							const auto reg = regs.back ()[oper->reg];
							str << "*	[reg( " << oper->reg << " )]: data: " << reg->data << " : " << reg->str_type() << std::endl;
						}

						break;
					} 

					case lexer_dec::operand_types::dest: {

						if (!regs.back().reg_exists(oper->reg))
							str << "*	[dest( " << oper->reg << " )]: NULL\n";
						else {
							const auto reg = regs.back()[oper->reg];
							str << "*	[dest( " << oper->reg << " )]: data: " << reg->data << " : " << reg->str_type() << std::endl;
						}

						break;
					}

					case lexer_dec::operand_types::source: {

						if (!regs.back().reg_exists(oper->reg))
							str << "*	[source( " << oper->reg << " )]: NULL\n";
						else {
							const auto reg = regs.back()[oper->reg];
							str << "*	[source( " << oper->reg << " )]: data: " << reg->data << " : " << reg->str_type() << std::endl;
						}

						break;
					}

					case lexer_dec::operand_types::integer: {
						str << "*	[integer]: " << std::to_string(oper->val) << std::endl;
						break;
					}

					case lexer_dec::operand_types::compare: {

						if (!regs.back().reg_exists(oper->reg))
							str << "*	[compare( " << oper->reg << " )]: NULL\n";
						else {
							const auto reg = regs.back()[oper->reg];
							str << "*	[compare( " << oper->reg << " )]: data: " << reg->data << " : " << reg->str_type() << std::endl;
						}

						break;
					}

					case lexer_dec::operand_types::memaddr: {
						str << "*	[memaddr]: " << std::to_string(node->address + oper->jmp_addr) << std::endl;
						break;
					}

					case lexer_dec::operand_types::proto: {
						str << "*	[proto]: " << std::to_string(oper->proto) << std::endl;
						break;
					}

					case lexer_dec::operand_types::kvalue: {
						str << "*	[kvalue]: " << node->lex->operand_expr<lexer_dec::operand_types::kvalue>().front()->k_value << std::endl;
						break;
					}

					case lexer_dec::operand_types::kvalue_dest: {
						str << "*	[kvalue_dest]: " << std::to_string(oper->k_idx) << std::endl;
						break;
					}

					case lexer_dec::operand_types::upvalue: {
						str << "*	[upvalue]: " << std::to_string(oper->upvalue) << std::endl;
						break;
					}

					case lexer_dec::operand_types::table_idx: {
						str << "*	[table_idx]: " << std::to_string(oper->table) << std::endl;
						break;
					}

					case lexer_dec::operand_types::fastcall_idx: {
						str << "*	[fastcall_idx]: " << std::to_string(oper->fastcall_idx) << std::endl;
						break;
					}

					case lexer_dec::operand_types::capture: {
						str << "*	[capture_idx]: " << std::to_string(oper->capture_ref) << std::endl;
						break;
					}

					default: {
						throw std::exception("Unkown operand type for debug.");
					}

				}

			}

			#if !TANSPILER_DEBUG_OPERANDS_PRINT_OVERRIDE
				std::cout << str.str ();
			#elif TANSPILER_DEBUG_OPERANDS_PRINT_OVERRIDE
				emitter::expandable_comment(decompilation, str.str());
			#endif

		#endif

		#if TANSPILER_DEBUG_PREDECOMPILATION 
			std::cout << "[transpiler.cpp] (pre-decompilation): " << decompilation << std::endl;
		#endif

		if (node->has_expr(ast_dec::expr_type::dead_instruction))
			continue;

		/* Instruction handler. */
		switch (node->lex->dissassembly->op) {
		

			/* Nop, Break, Depricated opcodes */
			case LuauOpcode::LOP_NOP:
			case LuauOpcode::LOP_BREAK: 
			case LuauOpcode::LOP_DEP_FORGLOOP_INEXT:
			case LuauOpcode::LOP_DEP_FORGLOOP_NEXT:
			case LuauOpcode::LOP_DEP_JUMPIFEQK:
			case LuauOpcode::LOP_DEP_JUMPIFNOTEQK: {
				break;
			}


			/* Load */
			case LuauOpcode::LOP_LOADNIL: {

				const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg];
				const auto source = "nil";

				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, source);

				}
				else {

					/* Create arg. */
					if (node->dest_loc.is_dest_loc) {

						dest->set<registers::type::var>(node->dest_loc.name);
						emitter::new_vararg_equal(decompilation, dest->data, source);

					}
					else {

						/* General purpose. */
						dest->set<registers::type::expr>(source);

					}

				}

				break;
			}
			case LuauOpcode::LOP_LOADN:
			case LuauOpcode::LOP_LOADB: {

				const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg];
			    auto source = std::to_string(node->lex->operand_expr<lexer_dec::operand_types::integer>().front()->val);


				/* Jump for loadb, jump target will be a dead instruction. */
				if (node->lex->dissassembly->op == LuauOpcode::LOP_LOADB && node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp)
					source = regs.back()[-1]->data;


				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, source);

				}
				else {

					/* Create arg. */
					if (node->dest_loc.is_dest_loc) {

						dest->set<registers::type::var>(node->dest_loc.name);
						emitter::new_vararg_equal(decompilation, dest->data, source);

					}
					else {

						/* General purpose. */
						dest->set<registers::type::expr>(source);

					}

				}

				break;
			}
			case LuauOpcode::LOP_LOADKX:
			case LuauOpcode::LOP_LOADK: {

				const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg];
				const auto source = node->lex->operand_expr<lexer_dec::operand_types::kvalue>().front()->k_value;

				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, source);

				}
				else {

					/* Create arg. */
					if (node->dest_loc.is_dest_loc) {

						dest->set<registers::type::var>(node->dest_loc.name);
						emitter::new_vararg_equal(decompilation, dest->data, source);

					}
					else {

						/* General purpose. */
						dest->set<registers::type::expr>(source);

					}

				}

				break;
			}
		
			case LuauOpcode::LOP_MOVE: {

				const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg];
				const auto source = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg]->data;

				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, source);

				}
				else {

					/* Create arg. */
					if (node->dest_loc.is_dest_loc) {

						dest->set<registers::type::var>(node->dest_loc.name);
						emitter::new_vararg_equal(decompilation, dest->data, source);

					}
					else {

						/* General purpose. */
						dest->set<registers::type::expr>(source);

					}

				}

				break;
			}


			/* Minus, Not, Lenght */
			case LuauOpcode::LOP_MINUS: {

				const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg];
				const auto source = "-" + regs.back()[node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg]->data;

				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, source);

				}
				else {

					/* Create arg. */
					if (node->dest_loc.is_dest_loc) {

						dest->set<registers::type::var>(node->dest_loc.name);
						emitter::new_vararg_equal(decompilation, dest->data, source);

					}
					else {

						/* General purpose. */
						dest->set<registers::type::expr>(source);

					}

				}

				break;
			}
			case LuauOpcode::LOP_NOT: {

				const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg];
				const auto source = "not " + regs.back()[node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg]->data;

				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, source);

				}
				else {

					/* Create arg. */
					if (node->dest_loc.is_dest_loc) {

						dest->set<registers::type::var>(node->dest_loc.name);
						emitter::new_vararg_equal(decompilation, dest->data, source);

					}
					else {

						/* General purpose. */
						dest->set<registers::type::expr>(source);

					}

				}

				break;
			}
			case LuauOpcode::LOP_LENGTH: {

				const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg];
				const auto source = "#" + regs.back()[node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg]->data;

				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, source);

				}
				else {

					/* Create arg. */
					if (node->dest_loc.is_dest_loc) {

						dest->set<registers::type::var>(node->dest_loc.name);
						emitter::new_vararg_equal(decompilation, dest->data, source);

					}
					else {

						/* General purpose. */
						dest->set<registers::type::expr>(source);

					}

				}

				break;
			}


			/* If */
			case LuauOpcode::LOP_JUMPIF:
			case LuauOpcode::LOP_JUMPIFNOT:
			case LuauOpcode::LOP_JUMPIFEQ:
			case LuauOpcode::LOP_JUMPIFLE:
			case LuauOpcode::LOP_JUMPIFLT:
			case LuauOpcode::LOP_JUMPIFNOTEQ:
			case LuauOpcode::LOP_JUMPIFNOTLE:
			case LuauOpcode::LOP_JUMPIFNOTLT: {

				/* Only handles conditional for compare flag. */

				/* Gets handled ahead of time just skip opcode. */
				if (node->has_expr(ast_dec::expr_type::if_) || node->has_expr(ast_dec::expr_type::elseif_) ||
					node->has_expr(ast_dec::expr_type::while_) || node->has_expr(ast_dec::expr_type::until_))
						break;



				break;
			}


			/* Arith */
			case LuauOpcode::LOP_ADD:
			case LuauOpcode::LOP_SUB:
			case LuauOpcode::LOP_MUL:
			case LuauOpcode::LOP_DIV:
			case LuauOpcode::LOP_MOD:
			case LuauOpcode::LOP_POW:
			case LuauOpcode::LOP_OR:
			case LuauOpcode::LOP_AND: {

				const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg];
				const auto sources = node->lex->operand_expr<lexer_dec::operand_types::source>();

				const auto source_1 = regs.back()[sources.front()->reg]->data;
				const auto source_2 = regs.back()[sources.back()->reg]->data;


				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::arith(node->lex->dissassembly->op, true, decompilation, source_1, source_2);

				}
				else {

					/* Create arg. */
					if (node->dest_loc.is_dest_loc) {

						std::string compiled = "";

						dest->set<registers::type::var>(node->dest_loc.name);
						emitter::arith(node->lex->dissassembly->op, false, compiled, source_1, source_2);
						emitter::new_vararg_equal(decompilation, dest->data, compiled);

					}
					else {

						/* General purpose. */
						dest->data.clear();
						emitter::arith(node->lex->dissassembly->op, false, dest->data, source_1, source_2);
						dest->set<registers::type::expr>(dest->data);

					}

				}

				break;
			}


			case LuauOpcode::LOP_ADDK:
			case LuauOpcode::LOP_SUBK:
			case LuauOpcode::LOP_MULK:
			case LuauOpcode::LOP_DIVK:
			case LuauOpcode::LOP_MODK:
			case LuauOpcode::LOP_POWK:
			case LuauOpcode::LOP_ORK:
			case LuauOpcode::LOP_ANDK: {

				const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg];

				const auto source_1 = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg]->data;
				const auto source_2 = node->lex->operand_expr<lexer_dec::operand_types::kvalue>().front()->k_value;


				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::arith(node->lex->dissassembly->op, true, decompilation, source_1, source_2);

				}
				else {

					/* Create arg. */
					if (node->dest_loc.is_dest_loc) {

						std::string compiled = "";

						dest->set<registers::type::var>(node->dest_loc.name);
						emitter::arith(node->lex->dissassembly->op, false, compiled, source_1, source_2);
						emitter::new_vararg_equal(decompilation, dest->data, compiled);

					}
					else {

						/* General purpose. */
						dest->data.clear();
						emitter::arith(node->lex->dissassembly->op, false, dest->data, source_1, source_2);
						dest->set<registers::type::expr>(dest->data);

					}

				}

				break;
			}


			/* Getimport, Call, Namecall */
			case LuauOpcode::LOP_GETIMPORT: {

				const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg];
				const auto source = node->lex->operand_expr<lexer_dec::operand_types::kvalue>().front()->k_value;
				
				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, source);

				}
				else {

					/* Create arg. */
					if (node->dest_loc.is_dest_loc) {

						dest->set<registers::type::var>(node->dest_loc.name);
						emitter::new_vararg_equal(decompilation, dest->data, source);

					}
					else {
						
						/* General purpose. */
						dest->set<registers::type::expr>(source);

					}

				}

				break;
			}
			case LuauOpcode::LOP_CALL: {

				const auto prev = ast->main_block->visit_previous_addr(node->address);

				auto arg = node->lex->dissassembly->operands[1]->val;
				auto retn = node->lex->dissassembly->operands[2]->val;

				const auto call = node->lex->dissassembly->operands[0]->reg;
				const auto dest = regs.back()[call];

				std::string compiled_call = dest->data + '(';


				/* Fix for mulret */
				if (arg == -1) {
					
					if (!prev->lex->has_operand_expr<lexer_dec::operand_types::dest>())
						throw std::exception("Previous doesn't have dest for call mulret.");

					arg = prev->lex->operand_expr<lexer_dec::operand_types::dest>().front()->val - call;
				}
				if (retn == -1) {

					if (!prev->lex->has_operand_expr<lexer_dec::operand_types::dest>())
						throw std::exception("Previous doesn't have dest for call mulret.");

					retn = prev->lex->operand_expr<lexer_dec::operand_types::dest>().front()->val - call;
				}


				/* Has args so parse them. */
				if (arg)
					for (auto i = 0u; i < arg; ++i) {

						/* Skip this with args. */
						if (!i && prev->lex->dissassembly->op == LuauOpcode::LOP_NAMECALL)
							continue;
						else {

							/* Compile call with previous register relative to arg count for args. */

							const auto idx = i + 1u + call;
							const auto& str = regs.back()[idx]->data;

							compiled_call += str;

							/* Add split. */
							if ((i + 1u) != arg)
								compiled_call += ", ";

						}

					}

				compiled_call += ")";

				/* No return just emit. */
				if (!retn) {
					emitter::write_line(decompilation, compiled_call);
					continue;
				}


				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, compiled_call);

				}
				else {

					/* Create arg. */
					if (node->dest_loc.is_dest_loc) {

						dest->set<registers::type::var>(node->dest_loc.name);
						emitter::new_vararg_equal(decompilation, dest->data, compiled_call);

					}
					else {

						/* General purpose. */
						dest->set<registers::type::expr>(compiled_call);

					}

				}


				/* Return has multiple returns fill upper with nils. */
				if (retn > 0)
					for (auto i = 0u; i < (retn - 1u); ++i)   /* Fill with nil. */
						regs.back()[i + call + 1u]->set<registers::type::expr>("nil");
					

				break;
			}
			case LuauOpcode::LOP_NAMECALL: {

				const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg];
				const auto source = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg]->data;
				const auto kvalue = node->lex->operand_expr<lexer_dec::operand_types::kvalue>().front()->k_value;

				const auto compiled = source + ':' + kvalue;

				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, compiled);

				}
				else {

					/* Create arg. */
					if (node->dest_loc.is_dest_loc) {

						dest->set<registers::type::var>(node->dest_loc.name);
						emitter::new_vararg_equal(decompilation, dest->data, compiled);

					}
					else {

						/* General purpose. */
						dest->set<registers::type::expr>(compiled);

					}

				}

				break;
			}


			/* Concat, Return */
			case LuauOpcode::LOP_CONCAT: {


				std::string source = "";

				const auto sources = node->lex->operand_expr<lexer_dec::operand_types::source>();
				const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg];

				const auto start = sources.front()->reg;
				const auto end = sources.front()->reg;


				/* Form data. */
				for (auto i = start; i <= end; ++i)
					source += regs.back()[i]->data + ((i != end) ? " .. " : "");


				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, source);

				}
				else {

					/* Create arg. */
					if (node->dest_loc.is_dest_loc) {

						dest->set<registers::type::var>(node->dest_loc.name);
						emitter::new_vararg_equal(decompilation, dest->data, source);

					}
					else {

						/* General purpose. */
						dest->set<registers::type::expr>(source);

					}

				}

				break;
			}
			case LuauOpcode::LOP_RETURN: {


			    auto val = node->lex->operand_expr<lexer_dec::operand_types::integer>().front()->val;
				const auto original_val = val;
				const auto dest = node->lex->operand_expr<lexer_dec::operand_types::reg>().front()->reg;


				/* If its not main proto then write a return. */
				if (ast->closure_type != ast_dec::closure_type::main || ast->main_block->has_next_inst<LuauOpcode::LOP_RETURN>(node->address) /* Has next return. */) { /* Skip op */

					/* Data */
					std::string compiled = "";

					/* Fix for mulret */
					if (val == -1)
						val = ast->main_block->visit_previous_addr(node->address)->lex->operand_expr<lexer_dec::operand_types::dest>().front()->val;

					/* Check to make sure if last dest reg is current dest reg and was multret if so just one return format. */
					if (original_val == -1 && dest == val)
						compiled = ' ' + regs.back()[dest]->data;
					else {

						for (auto i = 0u; i < val; ++i) {

							compiled += regs.back ()[dest + i]->data;

							if ((i + 1u) != val)
								compiled += (!i) ? " " : ", ";

						}

					}

					emitter::str(decompilation, "return" + compiled + ";\n");

				}

				break;
			}


			/* Set/Get global */
			case LuauOpcode::LOP_SETGLOBAL: {

				const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg];
				const auto source = node->lex->operand_expr<lexer_dec::operand_types::kvalue>().front()->k_value;

				emitter::vararg_equal(decompilation, dest->data, source);

				break;
			}
			case LuauOpcode::LOP_GETGLOBAL: {

				const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg];
				const auto source = node->lex->operand_expr<lexer_dec::operand_types::kvalue>().front()->k_value;


				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, source);

				}
				else {

					/* Create arg. */
					if (node->dest_loc.is_dest_loc) {

						dest->set<registers::type::var>(node->dest_loc.name);
						emitter::new_vararg_equal(decompilation, dest->data, source);

					}
					else {

						/* General purpose. */
						dest->set<registers::type::expr>(source);

					}

				}

				break;
			}


		    /* Set/Get/Close upvalue */
			case LuauOpcode::LOP_SETUPVAL: {

				const auto reg = node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;
				const auto dest = regs.back()[reg];
				const auto idx = node->lex->operand_expr<lexer_dec::operand_types::integer>().front()->val;


				/* Not vararg */
				if (dest->type != registers::type::var && dest->type != registers::type::arg) {

					/* Create arg. */
					if (node->dest_loc.is_dest_loc) {

						const auto data = dest->data;
						dest->set<registers::type::var>(node->dest_loc.name);
						emitter::new_vararg_equal(decompilation, dest->data, data);

					}
					else {
						throw std::exception("Tried to create upvalue on not vararg register.");
					}

				}


				/* Set upvalue for children ast. */
				for (const auto& i : ast->protos)
					i->upvalues.insert(std::make_pair(idx, std::make_pair (dest->data, reg)));

				break;
			}
			case LuauOpcode::LOP_GETUPVAL: {

				const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg];
				const auto source = ast->upvalues[node->lex->operand_expr<lexer_dec::operand_types::upvalue>().front()->upvalue].first;

				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, source);

				}
				else {

					/* Create arg. */
					if (node->dest_loc.is_dest_loc) {

						dest->set<registers::type::var>(node->dest_loc.name);
						emitter::new_vararg_equal(decompilation, dest->data, source);

					}
					else {

						/* General purpose. */
						dest->set<registers::type::expr>(source);

					}

				}

				break;
			}
			case LuauOpcode::LOP_CLOSEUPVALS: {


				const auto reg = node->lex->operand_expr<lexer_dec::operand_types::reg>().front()->reg;


				for (const auto& i : ast->protos)
					for (const auto& upv : i->upvalues) 
						if (upv.second.second >= reg)
							i->upvalues.erase(upv.first);			
				

				break;
			}
			

			/* Getvarargs */
			case LuauOpcode::LOP_GETVARARGS: {

				auto val = node->lex->operand_expr<lexer_dec::operand_types::integer>().front()->val;
				const auto start = node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;

				/* Fix for mulret */
				if (val == -1)
					val = ast->main_block->visit_previous_addr(node->address)->lex->operand_expr<lexer_dec::operand_types::dest>().front()->val;

				/* Iterate */
				for (auto on = start; on < (start + val); ++on) {

					const auto dest = regs.back()[on];

					/* Vararg.*/
					if (dest->type == registers::type::var || dest->type == registers::type::arg) {

						emitter::vararg_equal(decompilation, dest->data, "...");

					}
					else {

						/* Create arg. */
						if (node->dest_loc.is_dest_loc) {

							dest->set<registers::type::var>(node->dest_loc.name);
							emitter::new_vararg_equal(decompilation, dest->data, "...");

						}
						else {

							/* General purpose. */
							dest->data.clear();
							dest->set<registers::type::expr>("...");

						}

					}

				}

				break;
			}
										   

			/* Loop */
			case LuauOpcode::LOP_FORNLOOP: {
				break;
			}

			/* Table stuff. */
			/* Get */
			case LuauOpcode::LOP_GETTABLEN: {

				const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg];
				const auto source = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg]->data;
				const auto idx = node->lex->operand_expr<lexer_dec::operand_types::table_idx>().front()->table;

				const auto compiled = source + '[' + std::to_string(idx) + ']';

				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, compiled);

				}
				else {

					/* Create arg. */
					if (node->dest_loc.is_dest_loc) {

						dest->set<registers::type::var>(node->dest_loc.name);
						emitter::new_vararg_equal(decompilation, dest->data, compiled);

					}
					else {

						/* General purpose. */
						dest->set<registers::type::expr>(compiled);

					}

				}

				break;
			}
			case LuauOpcode::LOP_GETTABLEKS: {

				const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg];
				const auto source = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg]->data;
				const auto idx = node->lex->operand_expr<lexer_dec::operand_types::kvalue>().front()->k_value;

				const auto compiled = source + "[\"" + idx + "\"]";

				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, compiled);

				}
				else {

					/* Create arg. */
					if (node->dest_loc.is_dest_loc) {

						dest->set<registers::type::var>(node->dest_loc.name);
						emitter::new_vararg_equal(decompilation, dest->data, compiled);

					}
					else {

						/* General purpose. */
						dest->set<registers::type::expr>(compiled);

					}

				}

				break;
			}
			case LuauOpcode::LOP_GETTABLE: {

				const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg];
				const auto source = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg]->data;
				const auto idx = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::table_idx>().front()->reg]->data;

				const auto compiled = source + '[' + idx + ']';

				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, compiled);

				}
				else {

					/* Create arg. */
					if (node->dest_loc.is_dest_loc) {

						dest->set<registers::type::var>(node->dest_loc.name);
						emitter::new_vararg_equal(decompilation, dest->data, compiled);

					}
					else {

						/* General purpose. */
						dest->set<registers::type::expr>(compiled);

					}

				}

				break;
			}

		}

		#if TANSPILER_DEBUG_POSTDECOMPILATION 
				std::cout << "[transpiler.cpp] (post-decompilation): " << decompilation << std::endl;
		#endif

	}

	return decompilation;
}

void transpile_ast(const std::shared_ptr<ast_dec::ast>& main_ast, const std::shared_ptr<transpiler::transpiler_config>& config, std::string& str) {

	registers::reg_scope main_scope;
	std::vector<registers::reg_scope> scopes = { main_scope };

	/* Transpile main block. */
	const auto main = transpile_block(main_ast, main_ast->main_block, config, scopes);
	str += main; 

	return;
}

std::string transpiler::transpile(const std::shared_ptr<ast_dec::ast>& main_ast, const std::shared_ptr<transpiler_config>& config) {
	
	std::string retn = "";

	/* Transpile main. */
	transpile_ast(main_ast, config, retn);

	return retn;

}