#include <variant>
#include <sstream>
#include "transpiler.hpp"
#include "../emitter/emitter.hpp"
#include "../debug.hpp"


#define flag_compare -1
#define multret -1
#define fix_multret(ast, node) ast->main_block->visit_previous_addr(node->address)->lex->operand_expr<lexer_dec::operand_types::dest>().front()->val;

/* Suffixes */
namespace suffixes {
	std::uintptr_t loop_variable_suffix = 0u; 
	std::uintptr_t iterator_prefix_suffix = 0u;
	std::uintptr_t loop_variable_prefix_2_suffix = 0u;
}

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
		std::string sub_data = ""; /* Mostly used for stuff that needs it's own string. */

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
			ptr->sub_data = this->sub_data;
			ptr->special.inside_expr = this->special.inside_expr;
			return ptr;
		}

		void replicate(const std::shared_ptr<reg> src) {
			this->type = src->type;
			this->data = src->data;
			this->sub_data = src->sub_data;
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

		template<registers::type tt>
		void set_append(const std::string& str) {
			this->type = tt;
			this->data += str;
			return;
		}


		template<registers::type tt>
		void set_sub(const std::string& str) {
			this->type = tt;
			this->sub_data = str;
			return;
		}

		template<registers::type tt>
		void set_sub_append(const std::string& str) {
			this->type = tt;
			this->sub_data += str;
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

					if (reg > -1 /* First flag */)
						ptr->type = registers::type::flag;

					return ptr;
				}

			}

			reg_scope clone() {

				reg_scope retn;

				for (const auto& i : this->registers)
					retn[i.first]->replicate(i.second);

				return retn;
			}

	};

}

std::string transpile_block(const std::shared_ptr<ast_dec::ast>& ast, const std::shared_ptr <ast_dec::block> block, const std::shared_ptr<transpiler::transpiler_config>& config, std::vector<registers::reg_scope>& regs) {

	std::string decompilation = "";

	for (const auto& node : block->nodes) {

		/* Fix lv name. */
		if (node->dest_loc.is_dest_loc && !node->dest_loc.set_prefix) {
			node->dest_loc.name = config->variable_prefix + node->dest_loc.name;
			node->dest_loc.set_prefix = true;
		}


		bool table_start_new_node = false; /* For old nodes append and so on. */
	

		/* Pass expr. */
		for (const auto& expr : node->expr) {
			
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
						
							const auto reg = node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;

							if (!node->lex->has_operand_expr<lexer_dec::operand_types::dest>())
								throw std::exception("No dest operand for table start.");

							if (!table_start_new_node) {
								regs.back()[reg]->sub_data.clear();
								table_start_new_node = true;
							}

							regs.back()[reg]->set_sub_append<registers::type::expr>("{ ");
						}
						
						/* Inc expr. */
						++regs.back()[flag_compare]->special.inside_expr;

						break;
					}

					case ast_dec::expr_type::table_end:
					case ast_dec::expr_type::concat_routine_end:
					case ast_dec::expr_type::call_routine_end: {

						/* Dec expr. */
						--regs.back()[flag_compare]->special.inside_expr;

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

							emitter::compare(node->lex->dissassembly->op, node->branch_extra.opposite, regs.back()[flag_compare]->special.inside_expr, decompilation, (expr.first == ast_dec::expr_type::elseif_) ? "elseif" : "if", cmp_1->data, cmp_2->data);
						}
						else {

							/* if/elseif (??) */

							const auto cmp = regs.back()[node->lex->dissassembly->operands[0]->reg];

							emitter::compare(node->lex->dissassembly->op, node->branch_extra.opposite, regs.back()[flag_compare]->special.inside_expr, decompilation, (expr.first == ast_dec::expr_type::elseif_) ? "elseif" : "if", "", cmp->data);
						}

						/* Remove last */
						if (expr.first == ast_dec::expr_type::elseif_) {
							regs.pop_back();
						}
						
						/* Replicate next. */
						regs.emplace_back(regs.back().clone());

						/* Clear compare flag. */
						regs.back()[flag_compare]->clear(true);
						
						break;
					}
					case ast_dec::expr_type::else_: {

						/* Emit else. */
						emitter::str(decompilation, "else\n");

						/* Remove last scope and replicate next. */
						regs.pop_back();
						regs.emplace_back(regs.back().clone());

						/* Clear compare flag. */
						regs.back()[flag_compare]->clear(true);

						break;
					}

					case ast_dec::expr_type::while_: {

						
						/* Emit while cmp based on compare operand count. */
						if (node->lex->count_operand_expr<lexer_dec::operand_types::compare>() == 2u) {

							/* while (?? ?? ??) */

							const auto cmp_1 = regs.back()[node->lex->dissassembly->operands[0]->reg];
							const auto cmp_2 = regs.back()[node->lex->dissassembly->operands[2]->reg];

							emitter::compare(node->lex->dissassembly->op, node->branch_extra.opposite, regs.back()[flag_compare]->special.inside_expr, decompilation, (expr.first == ast_dec::expr_type::elseif_) ? "elseif" : "if", cmp_1->data, cmp_2->data);
						}
						else {

							/* while (??) */

							const auto cmp = regs.back()[node->lex->dissassembly->operands[0]->reg];

							emitter::compare(node->lex->dissassembly->op, node->branch_extra.opposite, regs.back()[flag_compare]->special.inside_expr, decompilation, (expr.first == ast_dec::expr_type::elseif_) ? "elseif" : "if", "", cmp->data);
						}

						/* Replicate next. */
						regs.emplace_back(regs.back().clone());

						/* Clear compare flag. */
						regs.back()[flag_compare]->clear(true);

						break;
					}

					case ast_dec::expr_type::for_iv_start: {

						std::string iter = "";
						static constexpr auto reserved = 2u + 1u; /* 2 is reserved 1 for new slot. */

						/* Replicate next. */
						regs.emplace_back(regs.back().clone());
						const auto begin = node->loop_extra.end_node->lex->dissassembly->operands.front()->reg;

						/* K */
						const auto K = config->loop_variable_prefix + std::to_string(suffixes::loop_variable_suffix++);
						regs.back()[begin + reserved]->set<registers::type::var>(K);

						/* V */
						const auto V = config->loop_variable_prefix_2 + std::to_string(suffixes::loop_variable_prefix_2_suffix++);
						regs.back()[begin + reserved + 1u]->set<registers::type::var>(V);
						
						/* Compile iter */
						for (auto i = 0u; i < 3u; ++i) {

							const auto& dest = regs.back ()[begin + i];

							/* invalid */
							if (dest->data == "nil")
								break;

							iter += dest->data;

							/* Next not found/nil so end. */
							if (regs.back().reg_exists(begin + i + 1u) || regs.back()[begin + i + 1u]->data == "nil")
								break;
							else /* Add split */
								iter += dest->data;

						}

						/* Emit */
						emitter::for_g_loop(decompilation, K + ", " + V, iter);

						break;
					}
					case ast_dec::expr_type::for_start: {

						static constexpr auto reserved = 2u + 1u; /* 2 is reserved 1 for new slot. */

						/* Replicate next. */
						regs.emplace_back(regs.back().clone());

						const auto begin = node->loop_extra.end_node->lex->dissassembly->operands.front()->reg;
						const auto count = node->loop_extra.end_node->lex->dissassembly->operands.back()->val;

						/* Compile variables in loop statement. */
						std::string compiled_vars = "";
						for (auto i = 0u; i < count; ++i) {

							/* Make iterating variable name and compile it. */
							const auto name = config->loop_variable_prefix + std::to_string(suffixes::loop_variable_suffix++);
							compiled_vars += (name + (((i + 1u) == count) ? "" : ", "));

							regs.back()[i + begin + reserved]->set<registers::type::var>(name);

						}

						/* Compile iterators. */
						std::string compiled_iterator = "";
						for (auto i = begin; i < (begin + reserved); ++i) {

							/* Make iterator and compile it. */
							if (regs.back().reg_exists(i)) {

								/* Register exists so compile it if its not nil. */
								const auto reg = regs.back()[i];

								/* Add split, check for nils */
								if (!compiled_iterator.empty() && reg->data != "nil")
									compiled_iterator += ", ";

								if (reg->data != "nil")
									compiled_iterator += reg->data;
							}
							else
								break;

						}
						
						/* Remove ", " *This is here just incase it hangs. */
						if (compiled_iterator[compiled_iterator.length() - 2u] == ',')
							compiled_iterator.erase(compiled_iterator.length() - 2u);

						/* Emit */
						emitter::for_g_loop(decompilation, compiled_vars, compiled_iterator);

						break;
					}
					case ast_dec::expr_type::for_n_start: {

						/* Replicate next. */
						regs.emplace_back(regs.back().clone());

						const auto end = node->loop_extra.end_node->lex->dissassembly->operands.front()->reg;
						const auto inc = (end + 1u);
						const auto start = (end + 2u);

						/* Write start end etc and finalize. */
						const auto iterate = config->iterator_prefix + std::to_string(suffixes::iterator_prefix_suffix++);
						auto iteration = regs.back()[start]->data + ", " + regs.back()[end]->data;

						/* Set reg as var. */
						regs.back()[start]->set<registers::type::var>(iterate);

						/* Check for incrementor. */
						if (regs.back().reg_exists(inc) && regs.back()[inc]->data != "1")
							iteration += ", " + regs.back()[inc]->data;

						/* Emit */
						emitter::for_n_loop(decompilation, iterate, iteration);

						break;
					}

					default: {
						break;
					}

				}

		}
		
		#if TANSPILER_DEBUG_OPERANDS 

			std::stringstream str;

			#if !TANSPILER_DEBUG_OPERANDS_PRINT_OVERRIDE
				str << "[transpiler.cpp] " << node->lex->dissassembly->data << std::endl;
			#elif TANSPILER_DEBUG_OPERANDS_PRINT_OVERRIDE
				str << "[INSTRUCTION]: " << node->lex->dissassembly->data << std::endl;
			#endif

			for (auto i = 0u; i < node->lex->operands.size(); ++i) {
				
				const auto oper = node->lex->dissassembly->operands[i];

				switch (node->lex->operands[i]) {

					case lexer_dec::operand_types::reg: {

						if (!regs.back().reg_exists(oper->reg))
							str << "*	[reg( " << oper->reg << " )]: NULL\n";
						else {
							const auto reg = regs.back ()[oper->reg];
							str << "*	[reg( " << oper->reg << " )]: " << reg->str_type() << std::endl;
							str << "*		[data]: " << reg->data << std::endl;
							str << "*		[sub_data]: " << reg->sub_data << std::endl;
						}

						break;
					} 

					case lexer_dec::operand_types::dest: {

						if (!regs.back().reg_exists(oper->reg))
							str << "*	[dest( " << oper->reg << " )]: NULL\n";
						else {
							const auto reg = regs.back()[oper->reg];
							str << "*	[dest( " << oper->reg << " )]: " <<  reg->str_type() << std::endl;
							str << "*		[data]: " << reg->data << std::endl;
							str << "*		[sub_data]: " << reg->sub_data << std::endl;
						}

						break;
					}

					case lexer_dec::operand_types::source: {

						if (!regs.back().reg_exists(oper->reg))
							str << "*	[source( " << oper->reg << " )]: NULL\n";
						else {
							const auto reg = regs.back()[oper->reg];
							str << "*	[source( " << oper->reg << " )]: " << reg->str_type() << std::endl;
							str << "*		[data]: " << reg->data << std::endl;
							str << "*		[sub_data]: " << reg->sub_data << std::endl;
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
							str << "*	[compare( " << oper->reg << " )]: " << reg->str_type() << std::endl;
							str << "*		[data]: " << reg->data << std::endl;
							str << "*		[sub_data]: " << reg->sub_data << std::endl;
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

			str << "*	[exprs]: " << std::endl;
			for (const auto& p : node->expr)
				str << "*		" << node->expr_str(p) << std::endl;

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
			case LuauOpcode::LOP_DEP_JUMPIFNOTEQK: 
			case LuauOpcode::LOP_PREPVARARGS: /* Unused */ {
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
					source = regs.back()[flag_compare]->data;


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
			case LuauOpcode::LOP_JUMPIFNOTLT: 
			case LuauOpcode::LOP_JUMPXEQKB:
			case LuauOpcode::LOP_JUMPXEQKN:
			case LuauOpcode::LOP_JUMPXEQKS:
			case LuauOpcode::LOP_JUMPXEQKNIL: {

				/* Only handles conditional for compare flag. */

				/* Gets handled ahead of time just skip opcode. */
				if (node->has_expr(ast_dec::expr_type::if_) || node->has_expr(ast_dec::expr_type::elseif_) ||
					node->has_expr(ast_dec::expr_type::while_) || node->has_expr(ast_dec::expr_type::until_))
						break;

				/* Get compare. */
				std::string cmp1 = "";
				std::string cmp2 = "";

				switch (node->lex->dissassembly->op) {
			
					case LuauOpcode::LOP_JUMPXEQKB:
					case LuauOpcode::LOP_JUMPXEQKN:
					case LuauOpcode::LOP_JUMPXEQKS:
					case LuauOpcode::LOP_JUMPXEQKNIL: {
						cmp1 = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::compare>().front()->reg]->data;
						cmp2 = node->lex->operand_expr<lexer_dec::operand_types::kvalue>()[1]->k_value;
						break;
					}

					case LuauOpcode::LOP_JUMPIF:
					case LuauOpcode::LOP_JUMPIFNOT: {
						cmp1 = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::compare>().front()->reg]->data;
						break;
					}

					case LuauOpcode::LOP_JUMPIFEQ:
					case LuauOpcode::LOP_JUMPIFLE:
					case LuauOpcode::LOP_JUMPIFLT:
					case LuauOpcode::LOP_JUMPIFNOTEQ:
					case LuauOpcode::LOP_JUMPIFNOTLE:
					case LuauOpcode::LOP_JUMPIFNOTLT: {
						cmp1 = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::compare>().front()->reg]->data;
						cmp2 = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::compare>()[1]->reg]->data;
						break;
					}

					default: {
						break;
					}

				}

				/* See pre-condition expr. */
				for (const auto& expr : node->expr) {

					for (auto i = 0u; i < expr.second; ++i)
						switch (expr.first) {

							case ast_dec::expr_type::open: {
								emitter::str(regs.back()[flag_compare]->data, " ( ");
								break;
							}


							default: {
								break;
							}

						}

				}

				/* Emit compare to compare flag. */
				emitter::compare(node->lex->dissassembly->op, node->branch_extra.opposite, true, regs.back()[flag_compare]->data, NULL, cmp1, cmp2);

				/* See post-condition expr. */
				for (const auto& expr : node->expr) {

					for (auto i = 0u; i < expr.second; ++i)
						switch (expr.first) {

							case ast_dec::expr_type::condition_and: {
								emitter::str(regs.back()[flag_compare]->data, " and ");
								break;
							}

							case ast_dec::expr_type::condition_or: {
								emitter::str(regs.back()[flag_compare]->data, " or ");
								break;
							}

							case ast_dec::expr_type::close: {
								emitter::str(regs.back()[flag_compare]->data, " ) ");
								break;
							}

							default: {
								break;
							}

						}

				}

				/* Emit conditional. */
				if (node->has_expr(ast_dec::expr_type::conditional)) {

					const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::compare>().front()->reg];

					/* Vararg.*/
					if (dest->type == registers::type::var || dest->type == registers::type::arg) {

						emitter::vararg_equal(decompilation, dest->data, regs.back()[flag_compare]->data);

					}
					else {

						/* Create arg. */
						if (node->dest_loc.is_dest_loc) {

							dest->set<registers::type::var>(node->dest_loc.name);
							emitter::new_vararg_equal(decompilation, dest->data, regs.back()[flag_compare]->data);

						}
						else {

							/* General purpose. */
							dest->set<registers::type::expr>(regs.back()[flag_compare]->data);

						}

					}

					regs.back()[flag_compare]->clear();

				}

				break;
			}

			/* Gets handled ahead of time. */
			case LuauOpcode::LOP_JUMP:
			case LuauOpcode::LOP_JUMPX:
			case LuauOpcode::LOP_JUMPBACK: {
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
				if (arg == multret) {
					
					if (!prev->lex->has_operand_expr<lexer_dec::operand_types::dest>())
						throw std::exception("Previous doesn't have dest for call mulret.");

					arg = prev->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg - call;
				}
				if (retn == multret) {

					if (!prev->lex->has_operand_expr<lexer_dec::operand_types::dest>())
						throw std::exception("Previous doesn't have dest for call mulret.");

					retn = prev->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg - call;
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
				if (!node->lex->dissassembly->operands[2]->val) {
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
				const auto end = sources.back()->reg;


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
					if (val == multret)
						val = fix_multret(ast, node);

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
				if (val == multret)
					val = fix_multret(ast, node);

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
			

			/* Fastcall */
			case LuauOpcode::LOP_FASTCALL:
			case LuauOpcode::LOP_FASTCALL1:
			case LuauOpcode::LOP_FASTCALL2:
			case LuauOpcode::LOP_FASTCALL2K: {
				break;
			}


			/* Loop */
			case LuauOpcode::LOP_FORNPREP:
			case LuauOpcode::LOP_FORNLOOP:
			case LuauOpcode::LOP_FORGLOOP:
			case LuauOpcode::LOP_FORGPREP:
			case LuauOpcode::LOP_FORGPREP_INEXT:
			case LuauOpcode::LOP_FORGPREP_NEXT: { /* Gets handled ahead of time. */
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
			/* Set */
			case LuauOpcode::LOP_SETTABLEN: 
			case LuauOpcode::LOP_SETTABLEKS:
			case LuauOpcode::LOP_SETTABLE: {

				const auto value = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg];
				auto table = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::reg>().front()->reg];
				std::string idx;

				/* Set idx */
				switch (node->lex->dissassembly->op) {

					case LuauOpcode::LOP_SETTABLEKS: {
						idx = node->lex->operand_expr<lexer_dec::operand_types::kvalue>().front()->k_value;
						break;
					}

					case LuauOpcode::LOP_SETTABLEN: {
						idx = std::to_string(node->lex->operand_expr<lexer_dec::operand_types::table_idx>().front()->table);
						break;
					}   

					case LuauOpcode::LOP_SETTABLE: {
						idx = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::table_idx>().front()->reg]->data;
						break;
					}

					default: {
						throw std::exception("Unkown instruction for set tables.");
					}

				}


				auto compiled = std::string("[") + idx + std::string("]");


				/* Element */
				if (node->has_expr(ast_dec::expr_type::table_element)) {
					
					compiled += std::string (" = ") + value->data;

					/* Append */
					table->sub_data += compiled + ((node->table_extra.end_table == node->lex->dissassembly->addr) ? "" : ", ");

					/* Not a table end so add end. */
					const auto table_ends = node->count_expr<ast_dec::expr_type::table_end>();
					for (auto i = 0u; i < table_ends; ++i)
						table->sub_data += " }";
				}
				

				/* Table */
				if (node->table_extra.end_table == node->lex->dissassembly->addr) {

					/* Vararg.*/
					if (table->type == registers::type::var || table->type == registers::type::arg) {

						emitter::vararg_equal(decompilation, table->data, table->sub_data);

					}
					else {

						/* Create arg. */
						if (node->dest_loc.is_dest_loc) {

							table->set<registers::type::var>(node->dest_loc.name);
							emitter::new_vararg_equal(decompilation, table->data, table->sub_data);

						}
						else {

							/* General purpose. */
							table->set<registers::type::expr>(table->sub_data);

						}

					}

				}
				else if (!node->table_extra.end_table) {
					emitter::vararg_equal(decompilation, table->data + compiled, value->data);
				}

				
				break;
			}
			/* Table */
			case LuauOpcode::LOP_DUPTABLE: 
			case LuauOpcode::LOP_NEWTABLE: {
				
				/* Table is already created before hand just decide like locvar or something. */
				const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg];

				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, dest->sub_data);

				}
				else {

					/* Create arg. */
					if (node->dest_loc.is_dest_loc) {

						dest->set<registers::type::var>(node->dest_loc.name);
						emitter::new_vararg_equal(decompilation, dest->data, dest->sub_data);

					}
					else {

						/* General purpose. */
						dest->set<registers::type::expr>(dest->sub_data);
		
					}

				}
				break;
			}
			case LuauOpcode::LOP_SETLIST: {

				/* Table is already created before hand just decide like locvar or something. */
				const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg];
				const auto start = node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg;
				auto amt = node->lex->operand_expr<lexer_dec::operand_types::integer>().front()->val;

				if (amt == multret)
					amt = fix_multret(ast, node);
			
				/* Add setlist data. */
				for (auto i = 0u; i < unsigned(amt); ++i) 
					dest->sub_data += regs.back()[start + i]->data + (((i + 1u) == amt) ? " }" : ", ");
				

				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, dest->sub_data);

				}
				else {

					/* Create arg. */
					if (node->dest_loc.is_dest_loc) {

						dest->set<registers::type::var>(node->dest_loc.name);
						emitter::new_vararg_equal(decompilation, dest->data, dest->sub_data);

					}
					else {

						/* General purpose. */
						dest->set<registers::type::expr>(dest->sub_data);

					}

				}
				break;
			}

			default: {
				throw std::exception("Unkown instruction for transpiler.");
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