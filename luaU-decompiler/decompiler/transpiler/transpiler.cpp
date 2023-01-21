#include <variant>
#include <sstream>
#include <unordered_map>
#include "transpiler.hpp"
#include "../emitter/emitter.hpp"
#include "../generic/generic.hpp"
#include "transpiler_debug.hpp"
#include "transpiler_macros.hpp"




/* Suffixes */
namespace str {


	/* Sees if character exists respect to string scopes. (Use this for character search for register data, makes things safe). */
	template<char c /* DONT USE, "\"", "\'"*/>
    bool find(const std::string& str) {
		
		bool v = true;

		for (const auto ch : str) {

			if (ch == '\"' || ch == '\'')
				v ^= true;

			if (v && ch == c)
				return true;

		}

		return false;
	}

	/* Splits string with respect to string scope. Valid function name.  */
	template<char target /* DONT USE, "\"", "\'"*/>
	std::vector<std::string> split(const std::string& str) {

		bool v = true;
		std::string append = "";

		std::vector<std::string> retn;

		if (!str::find<target>(str))
			return retn;

		/* Split by character with respect to strings. */
		for (auto i = 0u; i < str.size(); ++i) {

			const auto ch = str[i];

			if (ch != target) {
				append += ch;
			}

			if (ch == '\"' || ch == '\'') {
				v ^= true;
				append.clear();
			}

			if (v && ((i + 1u) != str.size() && str[i + 1u] == target) && ch != target) {
				retn.emplace_back(append);
				append.clear();
			}

		}

		/* Append end */
		if (append.size()) {
			retn.emplace_back(append);
		}

		/* Check for valid entries. */
		for (auto& i : retn) {

			for (auto a = 0u; a < i.size(); ++a)
				if (!char_valid(i[a])) /* Invalid? */ {
					i = i.substr(0, a);
					break;
				}

		}

		return retn;
	}

}

namespace suffixes {
	std::uintptr_t loop_variable_suffix = 0u; 
	std::uintptr_t iterator_prefix_suffix = 0u;
	std::uintptr_t loop_variable_prefix_2_suffix = 0u;
	std::uintptr_t smart_variable_prefix = 0u;
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

			throw std::runtime_error("Unkown type for register str.");
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
			    -2 : multret flag 
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

					if (reg > flag_first /* First flag */)
						ptr->type = registers::type::flag;

					return ptr;
				}

			}

			reg_scope clone() {

				reg_scope retn;

				for (const auto& i : this->registers) {
					retn[i.first]->replicate(i.second);
				}

				return retn;
			}

	};

}

namespace lv {

	std::string smart_name(std::vector<registers::reg_scope>& regs, const std::shared_ptr<ast_dec::node>& node, const std::string& def) {

		/* Go through exprs and decide. */
		for (const auto& expr : node->expr) {

			switch (expr.first) {

				case ast_dec::expr_type::table_end: {
					return "table_" + std::to_string(suffixes::smart_variable_prefix++);
				}

				case ast_dec::expr_type::concat_routine_end: {
					return "concat_" + std::to_string(suffixes::smart_variable_prefix++);
				}

				case ast_dec::expr_type::call_routine_end: {

					const auto str = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg]->data;

					/* Compiled var name */
					std::string compiled = "";

					/* example.example1 = example, **example1** */
					if (str::find<'.'>(str)) {
						compiled = str::split<'.'>(str).back();
					}
				
					
					if (!compiled.empty()) { /* First so add : if any. */

						if (str::find<':'>(str)) { /* example.example1:example2, example1_example2_??  */
							compiled += "_" + str::split<':'>(str).back() + "_";
						}
						else { /* No, ":" example.example1 example1_?? */
							compiled += "_";
						}

					}
					else {

						if (str::find<':'>(str)) { /* Compiled = "" example:example1, example_example1_?? */
							const auto split = str::split<':'>(str);
							compiled += split[split.size() - 2u] + "_" + split.back() + "_";
						}
						else { /* call_?? */
							compiled = str + "_";
						}

					}

					return compiled + std::to_string(suffixes::smart_variable_prefix++);
				}

				case ast_dec::expr_type::table_index: {
					return "idx_" + std::to_string(suffixes::smart_variable_prefix++);
				}

				case ast_dec::expr_type::arithK:
				case ast_dec::expr_type::arith: {
					return "arith_" + std::to_string(suffixes::smart_variable_prefix++);
				}

				default: {
					return def;
				}

			}

		}

		return def;
	}

}

namespace type_handler {

	/* Handle compare type. */
	std::pair<std::string /* cmp1*/, std::string /* cmp2 */> handle_compare(std::vector<registers::reg_scope>& regs, const std::shared_ptr<ast_dec::node>& node) {

		std::pair<std::string /* cmp1*/, std::string /* cmp2 */> retn;

		switch (node->lex->dissassembly->op) {

			/* These are completely different from one another. */
			case LuauOpcode::LOP_JUMPXEQKB:
			case LuauOpcode::LOP_JUMPXEQKN:
			case LuauOpcode::LOP_JUMPXEQKS:
			case LuauOpcode::LOP_JUMPXEQKNIL: {
				retn.first = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::compare>().front()->reg]->data;
				retn.second = node->lex->operand_expr<lexer_dec::operand_types::comparek_aux>().front()->k_value;
				break;
			}

			case LuauOpcode::LOP_JUMPIF:
			case LuauOpcode::LOP_JUMPIFNOT: {
				retn.first = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::compare>().front()->reg]->data;
				retn.second = "";
				break;
			}

			case LuauOpcode::LOP_JUMPIFEQ:
			case LuauOpcode::LOP_JUMPIFLE:
			case LuauOpcode::LOP_JUMPIFLT:
			case LuauOpcode::LOP_JUMPIFNOTEQ:
			case LuauOpcode::LOP_JUMPIFNOTLE:
			case LuauOpcode::LOP_JUMPIFNOTLT: {
				retn.first  = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::compare>().front()->reg]->data;
				retn.second = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::compare>()[1]->reg]->data;
				break;
			}

			default: {
				break;
			}

		}

		return retn;
	}

}





std::string transpile_blocks(const std::shared_ptr<ast_dec::ast>& ast, const std::shared_ptr<transpiler_data::transpiler_config>& config, std::vector<registers::reg_scope>& regs) {
	
	std::string decompilation = "";

	/* Go through everything linearly everything has already passed through doing it linearly lessers headaches. */
	const auto all = ast->main_block->visit_all();
	for (const auto& node : all) {

		/* Fix lv name. */
		if (node->has_expr(ast_dec::expr_type::locvar) && (config->smart_variable || !node->dest_loc.set_prefix) && !node->has_expr(ast_dec::expr_type::locvar_upvalue) && !(config_char(config))) {
			node->dest_loc.name = ((config->smart_variable) ? lv::smart_name(regs, node, config->variable_prefix) : node->dest_loc.name);
			node->dest_loc.set_prefix = true;
		} 


		bool table_start_new_node = false; /* For old nodes append and so on. */
		
		#if TANSPILER_DEBUG_PREEXPR
			node->debug_print_all("[TRANSPILER-PREEXPR]");
		#endif

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
								throw std::runtime_error("No dest operand for table start.");

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
						if (regs.back()[flag_compare]->special.inside_expr) {
							--regs.back()[flag_compare]->special.inside_expr;
						}
						else {
							#if TANSPILER_DEBUG_WARNINGS
								std::printf("[TRANSPILER-WARNING] Trying too dec 0 unsigned flag.\n");
							#endif
						}


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


					case ast_dec::expr_type::conditional_expression_end: {

						if (node->lex->has_operand_expr<lexer_dec::operand_types::source>()) {

							regs.back()[node->lex->operand_expr <lexer_dec::operand_types::source>().front()->reg]->set<registers::type::expr>(regs.back()[flag_compare]->data);
							regs.back()[flag_compare]->clear(true);

						}

						break;
					}

					case ast_dec::expr_type::if_:
					case ast_dec::expr_type::elseif_: {
					
						/* if/elseif (?? (??) ??) */
						const auto compares = type_handler::handle_compare(regs, node);
						const auto cmp_1 = compares.first;
						const auto cmp_2 = compares.second;
						
						emitter::compare(node->lex->dissassembly->op, node->branch_extra.opposite, regs.back()[flag_compare]->special.inside_expr, decompilation, (expr.first == ast_dec::expr_type::elseif_) ? "elseif" : "if", regs.back()[flag_compare]->data + cmp_1, cmp_2, config->emit_no_parenth_compare);
	
						
						/* Only scope for if, elseif gets handled speratly. */
						if (expr.first == ast_dec::expr_type::if_) {
							regs.emplace_back(regs.back().clone());
						}
	

						/* Clear compare flag. */
						regs.back()[flag_compare]->clear(true);
						
						/* Add break */
						if (node->has_expr(ast_dec::expr_type::condition_break)) {

							/* Emit break */
							emitter::str(decompilation, "break;\n");

							/* Emit end */
							emitter::str(decompilation, "end\n");

						}

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
					case ast_dec::expr_type::jump_elseif: {

						/* Remove last */
						regs.pop_back();
						
						/* Replicate next. */
						regs.emplace_back(regs.back().clone());

						break;
					}

					case ast_dec::expr_type::condition_true: {
						regs.back()[flag_compare]->set<registers::type::expr>("true");
						break;
					}
					case ast_dec::expr_type::while_: {

						std::string compiled = "";

						/* Has compare **Compile compare too get emitted** */
						if (node->lex->has_operand_expr<lexer_dec::operand_types::compare>()) {

							/* while (?? (??) ??) */
							const auto compares = type_handler::handle_compare(regs, node);
							const auto cmp_1 = compares.first;
							const auto cmp_2 = compares.second;

							emitter::compare(node->lex->dissassembly->op, node->branch_extra.opposite, true, compiled, "", regs.back()[flag_compare]->data + cmp_1, cmp_2, config->emit_no_parenth_compare);

						}
						else {

							/* Append compare flag. */
							compiled = regs.back()[flag_compare]->data;

						}

						/* Emit compiled */
						emitter::loop(decompilation, "while", compiled);

						/* Clear compare flag. */
						regs.back()[flag_compare]->clear(true);

						/* Replicate next. */
						regs.emplace_back(regs.back().clone());

						break;
					}
					case ast_dec::expr_type::until_: {

						std::string compiled = "";
						
						/* Has compare **Compile compare too get emitted** */
						if (node->lex->has_operand_expr<lexer_dec::operand_types::compare>()) {

							/* until (?? (??) ??) */
							const auto compares = type_handler::handle_compare(regs, node);
							const auto cmp_1 = compares.first;
							const auto cmp_2 = compares.second;

							emitter::compare(node->lex->dissassembly->op, node->branch_extra.opposite, true, compiled, "", regs.back()[flag_compare]->data + cmp_1, cmp_2, config->emit_no_parenth_compare);

						}
						else {

							/* Append compare flag. */
							compiled = regs.back()[flag_compare]->data;

						}

						/* Emit compiled */
						emitter::loop(decompilation, "until", compiled, ";\n");

						/* Clear compare flag. */
						regs.back()[flag_compare]->clear(true);

						/* Remove current register. */
						regs.pop_back();

						break;
					}

					case ast_dec::expr_type::for_iv_start: {

						std::string iter = "";
						static constexpr auto reserved = 2u + 1u; /* 2 is reserved 1 for new slot. */

						/* Replicate next. */
						regs.emplace_back(regs.back().clone());
						const auto begin = node->loop_extra.end_node->lex->dissassembly->operands.front()->reg;

						/* K */
						const auto K = (node->loop_extra.iteration_names.find(begin + reserved) != node->loop_extra.iteration_names.end()) ? node->loop_extra.iteration_names[begin + reserved] : emitter::create::locvar_name(config->loop_variable_prefix, suffixes::loop_variable_suffix++, config->iteration_suffix_char);
						regs.back()[begin + reserved]->set<registers::type::var>(K);

						/* V */
						const auto V = (node->loop_extra.iteration_names.find(begin + reserved + 1u) != node->loop_extra.iteration_names.end()) ? node->loop_extra.iteration_names[begin + reserved + 1u] : emitter::create::locvar_name(config->loop_variable_prefix_2, suffixes::loop_variable_prefix_2_suffix++, config->iteration_suffix_char);
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
							const auto name = (node->loop_extra.iteration_names.find(i + begin + reserved) != node->loop_extra.iteration_names.end()) ? node->loop_extra.iteration_names[i + begin + reserved] : emitter::create::locvar_name(config->loop_variable_prefix, suffixes::loop_variable_suffix++, config->iteration_suffix_char);
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
						const auto iterate = (node->loop_extra.iteration_names.find(start) != node->loop_extra.iteration_names.end()) ? node->loop_extra.iteration_names[start] : emitter::create::locvar_name(config->iterator_prefix, suffixes::iterator_prefix_suffix++, config->iteration_suffix_char);
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
		
		if (config->include_data) {
			emitter::expandable_comment(decompilation,  std::to_string(node->lex->dissassembly->addr) + " : " + node->lex->dissassembly->data);
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

					case lexer_dec::operand_types::comparek_aux: {
						str << "*	[comparek_aux]: " << node->lex->operand_expr<lexer_dec::operand_types::comparek_aux>().front()->k_value << std::endl;
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
															
					case lexer_dec::operand_types::table_reg: {
						str << "*	[table_reg]: " << std::to_string(oper->reg) << std::endl;
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
						throw std::runtime_error("Unkown operand type for debug.");
					}

				}

			}

			str << "*	[exprs]: " << std::endl;
			for (const auto& p : node->expr)
				str << "*		" << node->expr_str(p) << std::endl;

			#if !TANSPILER_DEBUG_OPERANDS_PRINT_OVERRIDE
				std::cout << str.str () << std::endl;
			#elif TANSPILER_DEBUG_OPERANDS_PRINT_OVERRIDE
				emitter::expandable_comment(decompilation, str.str() + std::string ("\n"));
			#endif

		#endif

		#if TANSPILER_DEBUG_PREDECOMPILATION 
			std::cout << "[transpiler.cpp] (pre-decompilation): " << decompilation << std::endl;
		#endif

		if (node->has_expr(ast_dec::expr_type::dead_instruction)) {
			continue;
		}


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

					/* Create var. */
					if (node->has_expr(ast_dec::expr_type::locvar)) {

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
				if (node->lex->dissassembly->op == LuauOpcode::LOP_LOADB && node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp) {
					source = regs.back()[flag_compare]->data;
				}
				else if (node->lex->dissassembly->op == LuauOpcode::LOP_LOADB) {

					if (node->has_expr(ast_dec::expr_type::conditional_expression_end)) {

						source = regs.back()[flag_compare]->data;
						regs.back()[flag_compare]->clear(true);

					}
					else {
						source = std::stoi(source) ? "true" : "false"; /* Change too string. */
					}

				}


				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, source);

				}
				else {

					/* Create var. */
					if (node->has_expr(ast_dec::expr_type::locvar)) {

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

					/* Create var. */
					if (node->has_expr(ast_dec::expr_type::locvar)) {

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

				/* Multret call */
				if (node->has_expr(ast_dec::expr_type::call_mulret_start) || node->has_expr(ast_dec::expr_type::call_mulret_member) || node->has_expr(ast_dec::expr_type::call_mulret_end)) {

					auto mul = regs.back()[flag_mulret];
					
					if (mul->data.back() != ',' && !mul->data.empty()) {
						mul->data += ", ";
					}

					mul->data += dest->data;
					
					if (node->has_expr(ast_dec::expr_type::call_mulret_end)) {
						emitter::vararg_equal(decompilation, mul->data, mul->sub_data);
						mul->clear();
					}
					
					continue;
				}

				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, source);

				}
				else {

					/* Create var. */
					if (node->has_expr(ast_dec::expr_type::locvar)) {

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

					/* Create var. */
					if (node->has_expr(ast_dec::expr_type::locvar)) {

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

					/* Create var. */
					if (node->has_expr(ast_dec::expr_type::locvar)) {

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

					/* Create var. */
					if (node->has_expr(ast_dec::expr_type::locvar)) {

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
				const auto compares = type_handler::handle_compare(regs, node);
				const auto cmp1 = compares.first;
				const auto cmp2 = compares.second;


				/* See pre-condition expr. */
				for (const auto& expr : node->expr) {

					for (auto i = 0u; i < expr.second; ++i)
						switch (expr.first) {

							case ast_dec::expr_type::condition_open: {
								emitter::str(regs.back()[flag_compare]->data, " ( ");
								break;
							}

							default: {
								break;
							}

						}

				}

				/* Get next conditional loadb. */
				std::shared_ptr<registers::reg> next_condition = nullptr;
				if (node->has_expr(ast_dec::expr_type::condition_emit_next)) {

					next_condition = regs.back()[(*(&node + 1u))->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg];
					next_condition->clear();
					next_condition->type = registers::type::expr;

				}


				/* Emit compare to compare flag. */
				emitter::compare(node->lex->dissassembly->op, node->branch_extra.opposite, true, (next_condition != nullptr) ? next_condition->data : regs.back()[flag_compare]->data, NULL, cmp1, cmp2, config->emit_no_parenth_compare);


				/* Automatically emit parenthesis for condition emitter. */
				if ((node->has_expr(ast_dec::expr_type::condition_emit_next))) {
					next_condition->data = "( " + next_condition->data + " )";
				}

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

							case ast_dec::expr_type::condition_close: {
								emitter::str(regs.back()[flag_compare]->data, " ) ");
								break;
							}

							case ast_dec::expr_type::condition_open_post: {
								emitter::str(regs.back()[flag_compare]->data, " ( ");
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

						/* Create var. */
						if (node->has_expr(ast_dec::expr_type::locvar)) {

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

					/* Create var. */
					if (node->has_expr(ast_dec::expr_type::locvar)) {

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

					/* Create var. */
					if (node->has_expr(ast_dec::expr_type::locvar)) {

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
				auto source = node->lex->operand_expr<lexer_dec::operand_types::kvalue>().front()->k_value;
				

				if (node->has_expr(ast_dec::expr_type::conditional_expression_end)) {

					source = regs.back()[flag_compare]->data;
					regs.back()[flag_compare]->clear(true);

				}

				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, source);

				}
				else {

					/* Create var. */
					if (node->has_expr(ast_dec::expr_type::locvar)) {

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
				if (arg == LUA_MULTRET) {
					
					if (!prev->lex->has_operand_expr<lexer_dec::operand_types::dest>())
						throw std::runtime_error("Previous doesn't have dest for call mulret.");

					arg = generic::fix_mulret(ast, node->address) - call;
				}
				if (retn == LUA_MULTRET) {

					if (!prev->lex->has_operand_expr<lexer_dec::operand_types::dest>())
						throw std::runtime_error("Previous doesn't have dest for call mulret.");

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
			
				/* Return has multiple returns fill. */
				if (retn > 1) { 

					if (node->has_expr(ast_dec::expr_type::locvar)) {
					
						std::string compiled = "";

						for (auto i = 0u; i < retn; ++i) {
							
							regs.back()[call + i]->set<registers::type::var>(node->dest_loc.multret_names[i]);

							/* Compile locvars */
							compiled += node->dest_loc.multret_names[i];
							if ((i + 1u) != retn) {
								compiled += ", ";
							}

						}

						emitter::new_vararg_equal(decompilation, compiled, compiled_call);

					}
					else {

						regs.back()[call]->set<registers::type::expr>(compiled_call);
						regs.back()[flag_mulret]->sub_data = compiled_call;

					}

					continue;
				}

				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {
					
					emitter::vararg_equal(decompilation, dest->data, compiled_call);

				}
				else {

					/* Create var. */
					if (node->has_expr(ast_dec::expr_type::locvar)) {

						dest->set<registers::type::var>(node->dest_loc.name);
						emitter::new_vararg_equal(decompilation, dest->data, compiled_call);

					}
					else {
						
						/* General purpose. */
						dest->set<registers::type::expr>(compiled_call);

					}

				}

				break;
			}
			case LuauOpcode::LOP_NAMECALL: {

				const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg];
				const auto source = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg]->data;
				auto& kvalue = node->lex->operand_expr<lexer_dec::operand_types::kvalue>().front()->k_value;

				/* Remove qoutes */
				kvalue.erase(std::remove(kvalue.begin(), kvalue.end(), '\"'), kvalue.end());

				const auto compiled = source + ':' + kvalue;

				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, compiled);

				}
				else {

					/* Create var. */
					if (node->has_expr(ast_dec::expr_type::locvar)) {

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

					/* Create var. */
					if (node->has_expr(ast_dec::expr_type::locvar)) {

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
					if (val == LUA_MULTRET)
						val = generic::fix_mulret(ast, node->address);

					/* Skip, "emit_no_last_return" */
					if (!val && config->emit_no_last_return && node == ast->main_block->visit_all().back()) {
						continue;
					}

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

					/* Add space */
					if (!compiled.empty())
						compiled = " " + compiled;

					emitter::str(decompilation, "return" + compiled + ";\n");

				}

				break;
			}


			/* Set/Get global */
			case LuauOpcode::LOP_SETGLOBAL: {

				const auto source = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg];
				const auto dest = node->lex->operand_expr<lexer_dec::operand_types::kvalue>().front()->k_value;

				/* Multret call */
				if (node->has_expr(ast_dec::expr_type::call_mulret_start) || node->has_expr(ast_dec::expr_type::call_mulret_member) || node->has_expr(ast_dec::expr_type::call_mulret_end)) {

					auto mul = regs.back()[flag_mulret];

					if (mul->data.back() != ',' && !mul->data.empty()) {
						mul->data += ", ";
					}

					mul->data += dest;

					if (node->has_expr(ast_dec::expr_type::call_mulret_end)) {
						emitter::vararg_equal(decompilation, mul->data, mul->sub_data);
						mul->clear();
					}

					continue;
				}

				emitter::vararg_equal(decompilation, dest, source->data);

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

					/* Create var. */
					if (node->has_expr(ast_dec::expr_type::locvar)) {

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

				const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg];
				const auto idx = node->lex->operand_expr<lexer_dec::operand_types::upvalue>().front()->val;

				/* Multret call */
				if (node->has_expr(ast_dec::expr_type::call_mulret_start) || node->has_expr(ast_dec::expr_type::call_mulret_member) || node->has_expr(ast_dec::expr_type::call_mulret_end)) {

					auto mul = regs.back()[flag_mulret];

					if (mul->data.back() != ',' && !mul->data.empty()) {
						mul->data += ", ";
					}

					mul->data += ast->upvalues[idx].first;

					if (node->has_expr(ast_dec::expr_type::call_mulret_end)) {
						emitter::vararg_equal(decompilation, mul->data, mul->sub_data);
						mul->clear();
					}

					continue;
				}


				emitter::vararg_equal(decompilation, ast->upvalues[idx].first, dest->data);

				break;
			}
			case LuauOpcode::LOP_GETUPVAL: {

				const auto dest = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg];
				auto source = ast->upvalues[node->lex->operand_expr<lexer_dec::operand_types::upvalue>().front()->upvalue].first;

				if (node->has_expr(ast_dec::expr_type::conditional_expression_end)) {

					source = regs.back()[flag_compare]->data;
					regs.back()[flag_compare]->clear(true);

				}

				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, source);

				}
				else {

					/* Create var. */
					if (node->has_expr(ast_dec::expr_type::locvar)) {

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
			
			/* Not needed upvalues are already set ahead of time by ast. */
			case LuauOpcode::LOP_CAPTURE: {
				break;
			}

			/* Closures */
			case LuauOpcode::LOP_NEWCLOSURE:
			case LuauOpcode::LOP_DUPCLOSURE: {

				std::size_t proto_idx = 0u;

				/* Get idx */
				if (node->lex->has_operand_expr<lexer_dec::operand_types::kvalue>()) {
					proto_idx = node->lex->operand_expr<lexer_dec::operand_types::kvalue>().front()->k_idx;
				}
				else {
					proto_idx = node->lex->operand_expr<lexer_dec::operand_types::proto>().front()->proto;
				}

				
				const auto proto_ast = ast->protos[proto_idx];

				/* Compile args */
				std::string args = "";
				for (const auto& arg : proto_ast->arg_regs) {
					args += (arg.second + ((arg == proto_ast->arg_regs.back()) ? "" : ", "));
				}

				std::string func = "";

				/* Compile function */
				switch (proto_ast->closure_type) {

					case ast_dec::closure_type::global: {
						emitter::function(decompilation, "function", proto_ast->closure_name, args, proto_ast->closure_decompilation, "end\n");
						regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg]->set<registers::type::expr>(proto_ast->closure_name);
						break;
					}
					
					case ast_dec::closure_type::newclosure: {
						emitter::function(func, "\n(function", "", args, proto_ast->closure_decompilation, "end)");
						regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg]->set<registers::type::expr>(func);
						break;
					}

					case ast_dec::closure_type::local: {
						emitter::function(decompilation, "local function", proto_ast->closure_name, args, proto_ast->closure_decompilation, "end\n");
						regs.back()[node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg]->set<registers::type::var>(proto_ast->closure_name);
						break;
					}

					default: {
						throw std::runtime_error("Unkown ast closure type for dupclosure/newclosure.");
					}

				}

				break;
			}

			/* Getvarargs */
			case LuauOpcode::LOP_GETVARARGS: {

				auto val = node->lex->operand_expr<lexer_dec::operand_types::integer>().front()->val;
				const auto start = node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;

				/* Fix for mulret */
				if (val == LUA_MULTRET)
					val = generic::fix_mulret(ast, node->address);

				/* Fix val. */
				if (!val)
					val = 1u;

				/* Iterate */
				for (auto on = start; on < (start + val); ++on) {

					const auto dest = regs.back()[on];

					/* Vararg.*/
					if (dest->type == registers::type::var || dest->type == registers::type::arg) {

						emitter::vararg_equal(decompilation, dest->data, "...");

					}
					else {

						/* Create var. */
						if (node->has_expr(ast_dec::expr_type::locvar)) {

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

					/* Create var. */
					if (node->has_expr(ast_dec::expr_type::locvar)) {

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
				auto idx = node->lex->operand_expr<lexer_dec::operand_types::kvalue>().front()->k_value;


				std::string compiled = "";

				/* Erase , ", ' */
				if (idx.find('\"') != std::string::npos) {
					idx.erase(std::remove(idx.begin(), idx.end(), '\"'), idx.end());
				}

				if (idx.find('\'') != std::string::npos) {
					idx.erase(std::remove(idx.begin(), idx.end(), '\''), idx.end());
				}
				
				compiled = ((str_idx(idx))) ? (source + "[\"" + idx + "\"]") : (source + '.' + idx);
				

				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, compiled);

				}
				else {

					/* Create var. */
					if (node->has_expr(ast_dec::expr_type::locvar)) {

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
				const auto idx = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::table_reg>().front()->reg]->data;

				const auto compiled = source + '[' + idx + ']';

				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, compiled);

				}
				else {

					/* Create var. */
					if (node->has_expr(ast_dec::expr_type::locvar)) {

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
				bool legal = false; 

				/* Set idx */
				switch (node->lex->dissassembly->op) {

					case LuauOpcode::LOP_SETTABLEKS: {
						idx = node->lex->operand_expr<lexer_dec::operand_types::kvalue>().front()->k_value;

						/* Erase , ", ' */
						if (idx.find('\"') != std::string::npos) {
							idx.erase(std::remove(idx.begin(), idx.end(), '\"'), idx.end());
						}

						if (idx.find('\'') != std::string::npos) {
							idx.erase(std::remove(idx.begin(), idx.end(), '\''), idx.end());
						}

						if (!(legal = ((str_idx(idx))) ? false : true)) {
							idx.insert(idx.begin(), '\"');
							idx.insert(idx.end(), '\"');
						}

						break;
					}

					case LuauOpcode::LOP_SETTABLEN: {
						idx = std::to_string(node->lex->operand_expr<lexer_dec::operand_types::table_idx>().front()->table);
						break;
					}   

					case LuauOpcode::LOP_SETTABLE: {
						idx = regs.back()[node->lex->operand_expr<lexer_dec::operand_types::table_reg>().front()->reg]->data;
						break;
					}

					default: {
						throw std::runtime_error("Unkown instruction for set tables.");
					}

				}

				
				std::string compiled = "";
				if (legal) {
					compiled = std::string(".") + idx;
				}
				else {
					compiled = std::string("[") + idx + std::string("]");
				}

				/* Multret call */
				if (node->has_expr(ast_dec::expr_type::call_mulret_start) || node->has_expr(ast_dec::expr_type::call_mulret_member) || node->has_expr(ast_dec::expr_type::call_mulret_end)) {

					auto mul = regs.back()[flag_mulret];

					if (mul->data.back() != ',' && !mul->data.empty()) {
						mul->data += ", ";
					}

					mul->data += table->data + compiled;

					if (node->has_expr(ast_dec::expr_type::call_mulret_end)) {
						emitter::vararg_equal(decompilation, mul->data, mul->sub_data);
						mul->clear();
					}

					continue;
				}


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

						/* Create var. */
						if (node->has_expr(ast_dec::expr_type::locvar)) {

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

				/* Fix table ends */
				const auto table_ends = node->count_expr<ast_dec::expr_type::table_end>();
				for (auto i = 0u; i < table_ends; ++i)
					dest->sub_data += " }";

				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, dest->sub_data);

				}
				else {

					/* Create var. */
					if (node->has_expr(ast_dec::expr_type::locvar)) {

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

				/* Fix for multret. */
				if (amt == LUA_MULTRET) {
					amt = generic::fix_mulret(ast, node->address);
				}

				/* Add setlist data. */
				for (auto i = 0u; i < unsigned(amt); ++i) 
					dest->sub_data += regs.back()[start + i]->data + (((i + 1u) == amt) ? " }" : ", ");
				

				/* Vararg.*/
				if (dest->type == registers::type::var || dest->type == registers::type::arg) {

					emitter::vararg_equal(decompilation, dest->data, dest->sub_data);

				}
				else {

					/* Create var. */
					if (node->has_expr(ast_dec::expr_type::locvar)) {

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
				throw std::runtime_error("Unkown instruction for transpiler.");
			}

		}

		#if TANSPILER_DEBUG_POSTDECOMPILATION 
				std::cout << "[transpiler.cpp] (post-decompilation): " << decompilation << std::endl;
		#endif

	}

	return decompilation;
}


void transpile_ast(const std::shared_ptr<ast_dec::ast>& main_ast, const std::shared_ptr<transpiler_data::transpiler_config>& config, std::string& str) {

	registers::reg_scope main_scope;
	std::vector<registers::reg_scope> scopes = { main_scope };


	/* Set args for registers. */
	auto reg = 0u;
	for (const auto& arg : main_ast->arg_regs) {
		if (arg.first != -1 /* ... */) {
			scopes.front()[reg++]->set<registers::type::arg>(arg.second);
		}
	}

	const auto transpiled = transpile_blocks(main_ast, config, scopes);
	str.reserve(transpiled.size());

	/* Transpile main block. */
	str.append(transpiled);

	return;
}


std::string transpiler::transpile(const std::shared_ptr<ast_dec::ast>& main_ast, const std::shared_ptr<transpiler_data::transpiler_config>& config) {

	/* Form protos linearly. */
	std::vector <std::shared_ptr<ast_dec::ast>> linear_on;
	std::unordered_map <std::shared_ptr<ast_dec::ast>, std::vector<std::shared_ptr<ast_dec::ast>>> linear;

	linear.insert(std::make_pair(main_ast, main_ast->protos));
	linear_on.insert(linear_on.end(), main_ast->protos.begin(), main_ast->protos.end());


	while (!linear_on.empty()) {

		auto on = linear_on.back();

		/* Doesnt exists create new entry. */
		if (linear.find(on) == linear.end()) {
			linear.insert(std::make_pair(on, on->protos));
			linear_on.insert(linear_on.end(), on->protos.begin(), on->protos.end());
		}

		/* Remove dupes */
		std::sort(linear_on.begin(), linear_on.end());
		linear_on.erase(std::unique(linear_on.begin(), linear_on.end()), linear_on.end());

		/* Remove current */
		linear_on.erase(std::remove(linear_on.begin(), linear_on.end(), on), linear_on.end());

	}


	/* Transpile by each. */
	std::vector <std::shared_ptr<ast_dec::ast>> comleted;

	/* First do ones with no protos. */
	for (const auto& i : linear)
		if (i.second.empty() && std::find(comleted.begin(), comleted.end(), i.first) == comleted.end()) {
			
			/* Transpile */
			transpile_ast(i.first, config, i.first->closure_decompilation);
			i.first->tanspiled = true;
			
			/* Add to complete. */
			comleted.emplace_back(i.first);
			linear.erase(i.first);

		}

	/* Do ones that have been completed. */
	while (!linear.empty()) {
		
		for (const auto& i : linear)
			if (std::find(comleted.begin(), comleted.end(), i.first) == comleted.end()) {
			
				/* Check if all protos have been analyzed. */
				auto all = true;
				for (const auto& p : i.second)
					if (std::find(comleted.begin(), comleted.end(), p) == comleted.end()) {
						all = false;
						break;
					}
				if (!all) {
					continue;
				}

				/* Transpile */
				transpile_ast(i.first, config, i.first->closure_decompilation);
				i.first->tanspiled = true;

				/* Add to complete. */
				comleted.emplace_back(i.first);
				linear.erase(i.first);
			
			}
			else {
				linear.erase(i.first);
			}
		
	}

	return main_ast->closure_decompilation;
}