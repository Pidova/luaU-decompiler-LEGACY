#pragma once
#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <variant>
#include <vector>
#include <tuple>
#include "ast_dec.hpp"
#include "../../dissassembler/Dissassembler.hpp"
#include "../lexer/lexer_dec.hpp"
#include "../transpiler/transpiler_data.hpp"
#include "../debug.hpp"

/*

	Standard ast. Nothing too special.

*/

namespace ast_dec {

	/* Types of closures. */
	enum class closure_type : std::uint8_t {
		none,
		main, /* Main proto (Nothing). */
		local, /* local function test () */
		global, /* function test () */
		newclosure /* (function()  end)*/
	};

	/* Expression type. {desc, intended usage/set for mostly} */
	enum class expr_type : std::uint8_t {
		lex, /* Specific type refer to lexer. [PLACEHOLDER] */

		arith, /* r1 += r1 + r1 [AST] */
		arithK, /* r1 += r1 + 1 [AST] */

		for_iv_start, /* for i,v in pairs ({ 1 }) do [ALL] */
		for_start, /* for ?? in ?? do [ALL] */
		for_n_start, /* for ?? in ?? do (numeral) [ALL] */
		for_prep, /* For preperation instruction [AST] */

		repeat_, /* repeat [ALL] */
		while_, /* while () follows condition(not jumpback). [ALL] */
		until_, /* until () follows condition(typically jumpback). [ALL] */
		break_, /* break [ALL] */
		scope_end, /* scope end (generic) [ALL] */

		call_routine_start, /* Call routine start. [AST] */
		call_routine_end, /* Call routine end. [AST] */

		concat_routine_start, /* Concat routine start. [AST] */
		concat_routine_end, /* Concat routine end. [AST] */

		if_, /* if () [ALL] */
		elseif_, /* elseif () [ALL] */
		else_, /* else [ALL] */
		condition_nonmutable, /* Condition that cannot be converted into while/if/elseif etc. [AST] */
		condition_concat_start, /* Concat a condition(universal) (start). [AST] */
		condition_concat_end, /* Concat a condition(universal) (end will get written too compare flag). [AST] */
		condition_true, /* Sets compare flag too true garunteing that while true expressions get set as true (expr type). [ALL] */
		condition_parameter, /* Parameter register for condition (used so value cant be turned into variable, gets all values not just source/value). [ALL] */
		condition_flag, /* Writes result to flag. */
		condition_break, /* Conditon leads too break. */

		/* Will get emmited to condition flag post compare. */
		condition_and, /* if/elseif/nested(and) appends to if_statements (Can be applied to until or while) [ALL] */
		condition_or, /* if/elseif/nested(or)  appends to if_statements (Can be applied to until or while) [ALL] */
		condition_close, /* Conditon flag:  %s ) [ALL] */
		condition_open, /*  Conditon flag: ( %s   [ALL](PRE) */
		condition_open_post, /*  Conditon flag: ( %s   [ALL](POST) */


		/* These only apply to routines outside of other routines like, call, concat, table, etc. (Will account for call parameters) */
		condition_routine, /* Means instruction is apart of a conditional routine (will only account for valid data not known args or vars, some will get in like if instruction before branch is locvar) **Does not mean it can't be an arguement!!** [AST] */
		condition_routine_start, 
		condition_routine_end,



		table_start, /* Table { [ALL] */
		table_element, /* Element in table. (Not usable for setlist cause of concatation) [ALL] */
		table_end, /* Table } (Will get ignored and use SETLIST instruction integral operand amt if it hits SETLIST.) [ALL] */
		table_index, /* Extra expr used for certain things (Will be appended when everything is done). [ALL] */

		closure_local, /* local function test () **Can be mutated by lv set if it is a lv will be local else newclosure* [ALL] */
		closure_global, /* function test () [ALL] */
		closure_newclosure, /* (function()  end) [ALL] */

		bad_instruction, /* Instruction will never get executed no matter watch branch is taken or not. [AST] */
		dead_instruction, /* Instruction gets ignored. *Will run exprs but not instruction in transpiler. [TRANSPILER] */
		conditional /* Condition flag will get written too dest. (Used for branching opcodes including loadb +jmp **Will clear compare flag if conditional is not loadb) [ALL] */
	};

	enum class element {
		front,
		back
	};

	struct node {

		std::uintptr_t address = 0u; /* Address. */

		std::vector <std::pair <expr_type, std::size_t /* Count usally used for ends, repeat, etc. */>> expr = { {expr_type::lex, 0u} }; /* Expression types. (Follows order) *All will get emmited(str). */

		/* Convert destination to local? */
		struct dest_loc {
			bool set_prefix = false; /* Used in transpiler to set suffix to local variable name. */
			bool is_dest_loc = false; /* Turns dest to local. */
			bool is_upvalue = false; /* locvar is upvalue? (Will use name as the variable automatically overrides all conditions) */
			std::string name = ""; /* Locvar name (Suffix, actuall variable name if is_upvalue is true, closure names wont get set here)  */
		} dest_loc;

		/* Extra information for branch. */
		struct branch_extra {
			bool opposite = false; /* Opposite compare from opcode. */
		} branch_extra;

		/* Extra information for tables. */
		struct table_extra {
			std::uintptr_t end_table = 0u; /* End table node address(not scopped). */
		} table_extra;

		/* Extra information for loops. */
		struct loop_extra {
			std::shared_ptr<ast_dec::node> end_node = nullptr; /* Used for prologue and epilogue of loop. */
			std::uint16_t start_reg = 0u; /* Start register (Format) */
			std::uint16_t end_reg = 0u; /* End register (Format) */
			std::unordered_map<std::uint16_t /* Reg */, std::string /* Name */> iteration_names; /* Override iteration variable names. */
		} loop_extra;

		/* Extra information for closure. */
		struct closure_extra {
			std::size_t closure_idx = 0u; /* Index of relating closure too ast->proto. */
		} closure_extra;

		std::shared_ptr<lexer_dec::lexerme> lex; /* Node lexer data. Has all the detailed information. */
		std::shared_ptr<node> sub_node = nullptr; /* When a move instruction is hit this will be the source node(mutable by for loop scopes(preps or jumptoo if no prep)). */
		std::vector<std::shared_ptr<node>> dest_nodes; /* Where dests where intialy initialized(mutable by for loop scopes).  */


		/* Node functions */

		/* Appends expr */
		template <expr_type type>
		void add_expr(const std::size_t count = 1u, const element ele = element::back) {

			/* Replace only lex with type. */
			if (this->expr.size() && this->expr.front().first == ast_dec::expr_type::lex /* Used as place holder. */) {
				this->expr.front().first = type;
				this->expr.front().second = count;
			}
			else {
				
				const auto pair = std::make_pair(type, count);

				if (ele == element::back) {
					this->expr.emplace_back(pair);
				}
				else {
					this->expr.insert(this->expr.begin(), pair);
				}

			}
				
			return;
		}

		/* Adds expression if type isn't a expr. */
		template <expr_type type>
		void add_existance(const std::size_t count = 1u, const element ele = element::back) {

			if (!this->has_expr(type))
				this->add_expr<type>(count, ele);

			return;
		}

		/* Replaces next target from 0 with type and count. */
		template <expr_type target, expr_type type>
		void replace_next(const std::size_t count = 1u) {

			for (auto& expr : this->expr)
				if (expr.first == target) {
					expr.first = type;
					expr.second = count;
					break;
				}

			return;
		}

		/* Expr exists? */
		bool has_expr(expr_type type) {
			for (const auto& i : this->expr)
				if (i.first == type)
					return true;
			return false;
		}

		/* Counts expr count total. */
		template <expr_type type>
		std::uintptr_t count_expr() {
			std::uintptr_t count = 0u;
			for (const auto& i : this->expr)
				if (i.first == type)
					count += i.second;
			return count;
		}

		/* Gets final sub node. */
		std::shared_ptr<node> get_sub_node() {

			auto sub = this->sub_node;

			while (sub != nullptr) 
				if (sub->sub_node != nullptr && sub->sub_node != sub) {
					sub = sub->sub_node;
				}
				else {
					break;
				}
			
			return sub;
		}

		/* Turns expr pair into a string. */
		std::string expr_str(const std::pair <expr_type, std::size_t>& p) {

			std::string retn = "";

			switch (p.first) {

				case expr_type::lex: { retn += "lex";  break; }

				case expr_type::arith: { retn += "arith";  break; }
				case expr_type::arithK: { retn += "arithK";  break; }

				case expr_type::for_iv_start: { retn += "for_iv_start";  break; }
				case expr_type::for_n_start: { retn += "for_n_start";  break; }
				case expr_type::for_start: { retn += "for_start";  break; }
				case expr_type::for_prep: { retn += "for_prep"; break; }

				case expr_type::repeat_: { retn += "repeat";  break; }
				case expr_type::while_: { retn += "while";  break; }
				case expr_type::until_: { retn += "until";  break; }
				case expr_type::break_: { retn += "break";  break; }
				case expr_type::scope_end: { retn += "scope_end";  break; }

				case expr_type::call_routine_start: { retn += "call_routine_start";  break; }
				case expr_type::call_routine_end: { retn += "call_routine_end";  break; }

				case expr_type::concat_routine_start: { retn += "concat_routine_start";  break; }
				case expr_type::concat_routine_end: { retn += "concat_routine_end";  break; }

				case expr_type::if_: { retn += "if";  break; }
				case expr_type::elseif_: { retn += "elseif";  break; }
				case expr_type::else_: { retn += "else";  break; }
				case expr_type::condition_and: { retn += "and";  break; }
				case expr_type::condition_or: { retn += "or";  break; }
				case expr_type::condition_nonmutable: { retn += "condition_nonmutable";  break; }
				case expr_type::condition_concat_start: { retn += "condition_concat_start";  break; }
				case expr_type::condition_concat_end: { retn += "condition_concat_end";  break; }
				case expr_type::condition_true: { retn += "condition_true"; break; }
				case expr_type::condition_parameter: { retn += "condition_parameter"; break; }
				case expr_type::condition_routine: { retn += "condition_routine"; break; }
				case expr_type::condition_routine_start: { retn += "condition_routine_start"; break; }
				case expr_type::condition_routine_end: { retn += "condition_routine_end"; break; }
				case expr_type::condition_flag: { retn += "condition_flag"; break; }
				case expr_type::condition_break: { retn += "condition_break"; break; }

				case expr_type::condition_close: { retn += "condition_close";  break; }
				case expr_type::condition_open: { retn += "condition_open";  break; }
				case expr_type::condition_open_post: { retn += "condition_open_post"; break;  }

				case expr_type::table_start: { retn += "table_start";  break; }
				case expr_type::table_element: { retn += "table_element";  break; }
				case expr_type::table_end: { retn += "table_end";  break; }
				case expr_type::table_index: { retn += "table_index"; break; }

				case expr_type::closure_local: { retn += "closure_local";  break; }
				case expr_type::closure_global: { retn += "closure_global";  break; }
				case expr_type::closure_newclosure: { retn += "closure_newclosure";  break; }

				case expr_type::bad_instruction: { retn += "bad_instruction";  break; }
				case expr_type::dead_instruction: { retn += "dead_instruction";  break; }
				case expr_type::conditional: { retn += "conditional";  break; }

				default: {
					throw std::runtime_error("Unkown expr for expr string.");
				}

			}

			retn = '[' + retn + "]: " + std::to_string(p.second);

			return retn;
		}
	
		/* Debug */

		#if debug_functions
		
			/* Prints dissassembly */
			void debug_print_dissassembly(const char* const state = " ") {
				std::printf("[Node-Debug(%s)] %llu %s\n", state, this->lex->dissassembly->addr, this->lex->dissassembly->data.c_str());
				return;
			}

			void debug_print_all(const char* const state = " ") {
				std::printf("[Node-Debug(%s)] %llu %s ", state, this->lex->dissassembly->addr, this->lex->dissassembly->data.c_str());
				for (const auto& i : this->expr)
					std::printf("%s", this->expr_str(i).c_str());
				std::printf("\n");
				return;
			}

			std::string debug_get_dissassembly() {
				return std::to_string (this->lex->dissassembly->addr) + " " + this->lex->dissassembly->data;
			}

		#endif

	};
	
	struct block {

		std::uintptr_t node_start = 0u; /* PC start. */
		std::uintptr_t node_end = 0u; /* PC final instruction. */

		std::vector<std::shared_ptr<node>> nodes; /* Nodes in block. */
		std::vector<std::shared_ptr<block>> branches; /* 2 elements; first is branch taken second is not, 1 there is only a jump/loops (calls\for\jumpbacks don't count, jump backs will refer to other nodes(may get fragmented), may be extras if dead instruction is next), 0 no jumps.  */


		/* All visits gets sorted automatically by address. */

		/* Visits first/all opcode value block. */
		template<LuauOpcode op>
		std::variant<std::vector<std::shared_ptr<node>>, std::shared_ptr<node>> visit_inst(const bool all /* All nodes with instruction. */) {

			std::vector<std::shared_ptr<node>> retn;
			std::vector<block*> scopes = { this };
			std::vector<block*> analyzed_scopes = { this };

			do {
				
				auto current_block = scopes.front ();

				/* Iterate through block nodes and find given instruction. */
				for (const auto& i : current_block->nodes) {
					
					if (i->lex->dissassembly->op == op) {

						if (all) { /* Has all so emblace node. */
							retn.emplace_back(i);
						}
						else {/* Not all so return node. */
							return i;
						}

					}

				}
		
				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());
				
				this->remove_dupes(scopes); /* Remove duplicates. */
				this->remove_dupes(retn); 
				this->sort_addr(retn); /* Sort retn by address. */

			} while (scopes.size());

			return retn;
		}


		/* Visits first/all types in a block. */
		template<lexer_dec::inst_type type>
		std::variant<std::vector<std::shared_ptr<node>>, std::shared_ptr<node>> visit_type(const bool all /* All nodes with instruction. */) {

			std::vector<std::shared_ptr<node>> retn;
			std::vector<block*> scopes = { this };
			std::vector<block*> analyzed_scopes = { this };

			do {

				auto current_block = scopes.front();

				/* Iterate through block nodes and find given instruction. */
				for (const auto& i : current_block->nodes) {
					
					if (i->lex->type == type) {

						if (all) { /* Has all so emblace node. */
							retn.emplace_back(i);
						}
						else {/* Not all so return node. */
							return i;
						}

					}

				}

				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

				this->remove_dupes(scopes); /* Remove duplicates. */
				this->remove_dupes(retn);
				this->sort_addr(retn); /* Sort retn by address. */

			} while (scopes.size());

			return retn;
		}

		/* Visits next/all opcode from addr. (Ignores current.) */
		template<LuauOpcode op>
		std::variant<std::vector<std::shared_ptr<node>>, std::shared_ptr<node>> visit_next_inst(const std::uintptr_t addr, const bool all /* All nodes with instruction. */) {

			std::vector<std::shared_ptr<node>> retn;
			std::vector<block*> scopes = { this };

			do {

				auto current_block = scopes.front ();

				/* Iterate through block nodes and find given instruction. */
				for (const auto& i : current_block->nodes) {

					if (i->address > addr && i->lex->dissassembly->op == op) {

						if (all) { /* Has all so emblace node. */
							retn.emplace_back(i);
						}
						else { /* Not all so return node. */
							return i;
						}

					}

				}

				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

				this->remove_dupes(scopes); /* Remove duplicates. */
				this->remove_dupes(retn);
				this->sort_addr(retn); /* Sort retn by address. */

			} while (scopes.size ());

			/* Nothing. */
			if (!retn.size())
				throw std::runtime_error("Returning no data for visit_next_inst.");

			return retn;
		}


		/* See if next opcode from addr exists. (Ignores current) */
		template<LuauOpcode op>
		bool has_next_inst(const std::uintptr_t addr) {

			std::vector<block*> scopes = { this };

			do {

				auto current_block = scopes.front();

				/* Iterate through block nodes and find given instruction. */
				for (const auto& i : current_block->nodes) {

					if (i->address > addr && i->lex->dissassembly->op == op)
						return true;
				}

				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

				this->remove_dupes(scopes); /* Remove duplicates. */

			} while (scopes.size());

			return false;
		}


		/* Visit node with address. */
		std::shared_ptr<node> visit_addr(const std::uintptr_t addr) {

			std::vector<block*> scopes = { this };

			do {

				auto current_block = scopes.front();

				/* Iterate through block nodes and find given instruction. */
				for (const auto& i : current_block->nodes) {

					if (i->address == addr)
							return i;

				}

				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

				this->remove_dupes(scopes); /* Remove duplicates. */

			} while (scopes.size());

			#if display_warnings 
				std::printf("[WARNING] Returning no data for visit_addr.\n");
			#endif

			return nullptr;
		}


		/* Visit node with expression. */
		template<expr_type type>
		std::variant<std::vector<std::shared_ptr<node>>, std::shared_ptr<node>> visit_expr(const bool all) {

			std::vector<std::shared_ptr<node>> retn;
			std::vector<block*> scopes = { this };

			do {

				auto current_block = scopes.front();

				/* Iterate through block nodes and find given instruction. */
				for (const auto& i : current_block->nodes) {

					if (i->has_expr(type)) {

						if (all) {
							retn.emplace_back(i);
						}
						else {
							return i;
						}

					}

				}

				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

				this->remove_dupes(scopes); /* Remove duplicates. */
				this->remove_dupes(retn);
				this->sort_addr(retn); /* Sort retn by address. */

			} while (scopes.size());

			return retn;
		}

		/* Visit next node with expression. (Ignores current address) */
		template<expr_type type>
		std::variant<std::vector<std::shared_ptr<node>>, std::shared_ptr<node>> visit_next_expr(const std::uintptr_t address, const bool all) {

			std::vector<std::shared_ptr<node>> retn;
			std::vector<block*> scopes = { this };

			do {

				auto current_block = scopes.front();

				/* Iterate through block nodes and find given instruction. */
				for (const auto& i : current_block->nodes) {

					if (i->address > address && i->has_expr(type)) {

						if (all) {
							retn.emplace_back(i);
						}
						else {
							return i;
						}

					}

				}

				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

				this->remove_dupes(scopes); /* Remove duplicates. */
				this->remove_dupes(retn);
				this->sort_addr(retn); /* Sort retn by address. */

			} while (scopes.size());

			return retn;
		}


		/* Visit node with previous address. */
		std::shared_ptr<node> visit_previous_addr(const std::uintptr_t addr) {

			std::vector<block*> scopes = { this };

			do {

				auto current_block = scopes.front();

				/* Iterate through block nodes and find given instruction. */
				for (const auto& i : current_block->nodes) {

					if ((i->address + i->lex->dissassembly->len) == addr)
						return i;

				}

				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

				this->remove_dupes(scopes); /* Remove duplicates. */

			} while (scopes.size());

			throw std::runtime_error("Returning no data for visit_addr.");
		}


		/* Visits next/all inst type. */
		template<lexer_dec::inst_type inst>
		std::variant<std::vector<std::shared_ptr<node>>, std::shared_ptr<node>> visit_next_type(const bool all /* All nodes with instruction. */) {

			std::vector<std::shared_ptr<node>> retn;
			std::vector<block*> scopes = { this };

			do {

				auto current_block = scopes.front();

				/* Iterate through block nodes and find given instruction. */
				for (const auto& i : current_block->nodes) {

					if (i->lex->type == inst) {

						if (all) { /* Has all so emblace node. */
							retn.emplace_back(i);
						}
						else { /* Not all so return node. */
							return i;
						}

					}

				}

				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

				this->remove_dupes(scopes); /* Remove duplicates. */
				this->remove_dupes(retn);
				this->sort_addr(retn); /* Sort retn by address. */

			} while (scopes.size());

			return retn;
		}
		

		/* Visits all inst type in range. (Includes being, end) */
		template<lexer_dec::inst_type inst>
		std::vector<std::shared_ptr<node>> visit_range_type(const std::uintptr_t begin, const std::uintptr_t end) {

			std::vector<std::shared_ptr<node>> retn;
			std::vector<block*> scopes = { this };

			do {

				auto current_block = scopes.front();

				/* Iterate through block nodes and find given instruction. */
				for (const auto& i : current_block->nodes) {

					if (i->address >= begin && i->address <= end && i->lex->type == inst) {
						retn.emplace_back(i);
					}

				}

				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

				this->remove_dupes(scopes); /* Remove duplicates. */
				this->remove_dupes(retn);
				this->sort_addr(retn); /* Sort retn by address. */

			} while (scopes.size());

			return retn;
		}


		/* Visits next inst type in range. (Includes being, end) */
		template<lexer_dec::inst_type inst>
		std::shared_ptr<node> visit_range_type_next(const std::uintptr_t begin, const std::uintptr_t end) {

			std::vector<block*> scopes = { this };

			do {

				auto current_block = scopes.front();

				/* Iterate through block nodes and find given instruction. */
				for (const auto& i : current_block->nodes) {

					if (i->address >= begin && i->address <= end && i->lex->type == inst) {
						return i;
					}

				}

				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

				this->remove_dupes(scopes); /* Remove duplicates. */

			} while (scopes.size());

			#if display_warnings 
				std::printf("[WARNING] Nothing will be returned for visit_range_type_next.\n");
			#endif

			return nullptr;
		}

		/* Visits node with previous node with given register as dest. */
		std::shared_ptr<node> visit_previous_dest_register(const std::uintptr_t on_address, const std::uint16_t target_reg) {

			std::shared_ptr<ast_dec::node> retn = nullptr;
			std::vector<block*> scopes = { this };

			do {

				auto current_block = scopes.front();

				/* Iterate through block nodes and find given dest register thats before on_address. */
				for (const auto& i : current_block->nodes) {

					/* yeah */
					if (i->address < on_address && i->lex->operands.size() && i->lex->operands.front() == lexer_dec::operand_types::dest && i->lex->dissassembly->operands.front()->reg == target_reg) {
						
						if (retn == nullptr) /* First */
							retn = i;
						else if (retn->address < i->address /* Nearest */)
							retn = i;

					}

				}

				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

				this->remove_dupes(scopes); /* Remove duplicates. */

			} while (scopes.size());

			/* Node is null. */
			if (retn == nullptr)
				throw std::runtime_error("Couldn't find previous dest based on register.");

			return retn;
		}


		/* Visits rest of nodes for all blocks. (Ignores current) */
	    std::vector<std::shared_ptr<node>> visit_rest(const std::uintptr_t on_address) {

			std::vector<std::shared_ptr<node>> retn;
			std::vector<block*> scopes = { this };

			do {

				auto current_block = scopes.front();

				/* Iterate through block nodes and find given instruction. */
				for (const auto& i : current_block->nodes) {

					if (i->address > on_address)
						retn.emplace_back(i);

				}

				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

				this->remove_dupes(scopes); /* Remove duplicates. */
				this->remove_dupes(retn);
				this->sort_addr(retn); /* Sort retn by address. */

			} while (scopes.size());

			if (!retn.size ())
				throw std::runtime_error("Returning no data for visit_addr.");

			return retn;
		}


		/* Visits relative node to op being target and args being addatives till op hits = dec and args = inc(singular) and its 0.  */
		template<LuauOpcode op>
		std::shared_ptr<node> visit_relative_inst(const std::vector<LuauOpcode> rel) {

			auto count = 0u;
			std::vector<block*> scopes = { this };

			do {

				auto current_block = scopes.front();

				/* Iterate through block nodes and find given instruction. */
				for (const auto& i : current_block->nodes) {

					/* Inc for relative. */
					if (std::find(rel.begin(), rel, i->lex->dissassembly->op) != rel.end())
						++count;
					
					/* If found op dec if count isnt 0. If it is 0 then return node. */
					if (i->lex->dissassembly->op == op)
						if (!count)
							return i;
						else
							--count;
				}

				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

				/* Remove duplicates. */
				this->remove_dupes(scopes);

			} while (scopes.size());

			throw std::runtime_error("Returning no data for visit_relative_inst.");
		}


		/* Visits next relative node to op being target and args being addatives till op hits = dec and args = inc(singular) and its 0.  (Ignores current) */
		template<LuauOpcode op>
		std::shared_ptr<node> visit_relative_next_inst(const std::uintptr_t on_address, const std::vector<LuauOpcode> rel) {

			auto count = 0u;
			std::vector<block*> scopes = { this };

			do {

				auto current_block = scopes.front();

				/* Iterate through block nodes and find given instruction. */
				for (const auto& i : current_block->nodes) {

					/* If current node address isnt bigger repeat till it is.*/
					if (i->address <= on_address)
						continue;

					/* Inc for relative. */
					if (std::find(rel.begin(), rel.end (), i->lex->dissassembly->op) != rel.end())
						++count;

					/* If found op dec if count isnt 0. If it is 0 then return node. */
					if (i->lex->dissassembly->op == op)
						if (!count)
							return i;
						else
							--count;
				}

				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

				this->remove_dupes(scopes); /* Remove duplicates. */

			} while (scopes.size());

			throw std::runtime_error("Returning no data for visit_relative_inst.");
		}


		/* Visits next relative node to expr being target and args being addatives till exprs(rel arg) hits = dec and exper_target(template target) = inc(singular) and its 0.  (Ignores current) */
		template<expr_type target>
		std::shared_ptr<node> visit_relative_next_expr(const std::uintptr_t on_address, const std::vector<expr_type> rel) {

			auto count = 0u;
			std::vector<block*> scopes = { this };

			do {

				auto current_block = scopes.front();

				/* Iterate through block nodes and find given instruction. */
				for (const auto& i : current_block->nodes) {

					/* If current node address isnt bigger repeat till it is.*/
					if (i->address <= on_address) {
						continue;
					}

					/* Inc for relative. */
					for (const auto r : rel)
						if (i->has_expr(r)) {
							++count; 
							break;
						}

					/* If found op dec if count isnt 0. If it is 0 then return node. */
					if (i->has_expr(target)) {

						if (!count) {
							return i;
						}
						else {
							--count;
						}

					}

				}

				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

				this->remove_dupes(scopes); /* Remove duplicates. */

			} while (scopes.size());

			throw std::runtime_error("Returning no data for visit_relative_next_expr.");
		}

		/* Visits expr routines (doesn't count for target). */
		template<expr_type target, expr_type close>
		std::variant<std::vector<std::pair <std::shared_ptr<node> /* Begin */, std::shared_ptr<node> /* End */>>, std::pair <std::shared_ptr<node> /* Begin */, std::shared_ptr<node> /* End */>>  visit_expr_routine(const bool all /* All nodes with instruction. */) {

			bool inside = false;
			std::vector <std::pair <std::shared_ptr<node> /* Begin */, std::shared_ptr<node> /* End */>> retn;
			std::shared_ptr<node> begin = nullptr;
			std::vector<block*> scopes = { this };

			do {

				auto current_block = scopes.front();

				/* Iterate through block nodes and find given instruction. */
				for (const auto& i : current_block->nodes) {
			
					if (i->has_expr(target)) {
						inside = true;
						begin = i;
					}

					/* If found op dec if count isnt 0. If it is 0 then return node. */
					if (inside && i->has_expr(close)) {

						if (all) { /* Has all so emblace node. */
							retn.emplace_back(std::make_pair(begin, i));
						}
						else { /* Not all so return node. */
							return std::make_pair(begin, i);
						}

						inside = false;
					}

				}

				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

				this->remove_dupes(scopes); /* Remove duplicates. */
				this->remove_dupes(retn);
				this->sort_addr(retn);

			} while (scopes.size());

			#if display_warnings
				if (!retn.size()) { 
					std::printf("[WARNING] Returning no data for visit_expr_routine.\n");				
				}
			#endif
			
			return retn;
		}


		/* Visits expr routines in range (includes start and end). */
		template<expr_type target, expr_type close>
		std::vector<std::pair <std::shared_ptr<node> /* Begin */, std::shared_ptr<node> /* End */>> visit_expr_routine_range(const std::uintptr_t start, const std::uintptr_t end) {

			std::vector <std::pair <std::shared_ptr<node>, std::shared_ptr<node>>> retn;

			const auto routines = std::get<std::vector<std::pair <std::shared_ptr<node>, std::shared_ptr<node>>>>(this->visit_expr_routine<target, close>(true));
			for (const auto& i : routines) {
				
				if (i.first->address >= start && i.second->address <= end)
					retn.emplace_back(i);

			}

			this->remove_dupes(retn);
			this->sort_addr(retn);

			return retn;
		}

		/* Visits expr routines in range that are touching and concats them to one object. (includes start and end).*/
		template<expr_type target, expr_type close>
		std::vector<std::pair<std::shared_ptr<node> /* Begin */, std::shared_ptr<node> /* End*/>> visit_expr_routine_range_touching(const std::uintptr_t start, const std::uintptr_t end) {

			std::shared_ptr<node> begin = nullptr;
			std::shared_ptr<node> prev_last = nullptr;
			std::vector<std::pair<std::shared_ptr<node>, std::shared_ptr<node>>> retn;

			const auto routines = this->visit_expr_routine_range<target, close>(start, end);
			for (const auto& i : routines) {
				
				/* Add missing */
				if (begin == nullptr) {
					begin = i.first;
				}

				if (prev_last != nullptr) {

					if ((prev_last->address + prev_last->lex->dissassembly->len) != i.first->address) {
						retn.emplace_back(std::make_pair(begin, i.second));
						begin = nullptr;
						prev_last = nullptr;
					}

				}

				prev_last = i.second;

			}

			if (begin != nullptr) {
				retn.emplace_back(std::make_pair(begin, prev_last));
			}

			this->sort_addr(retn);

			return retn;
		}

		/* Visits all blocks in ast.  */
		std::vector <std::shared_ptr<node>> visit_all() {

			std::vector <std::shared_ptr<node>> retn;
			std::vector<block*> scopes = { this };

			do {

				auto current_block = scopes.front();

				retn.insert(retn.end(), current_block->nodes.begin(), current_block->nodes.end());

				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

				this->remove_dupes(scopes); /* Remove duplicates. */
				this->remove_dupes(retn);
				this->sort_addr(retn); /* Sort retn by address. */

			} while (scopes.size());

			if (!retn.size ())
				throw std::runtime_error("Returning no data for visit_all.");

			return retn;
		}

		/* Visits all nodes between addresses (Ignores start, end) */
		std::vector<std::shared_ptr<node>> visit_range(const std::uintptr_t start, const std::uintptr_t end) {

			std::vector<std::shared_ptr<node>> retn;
			std::vector<block*> scopes = { this };

			do {

				auto current_block = scopes.front();

				/* Iterate through block nodes. */
				for (const auto& i : current_block->nodes) {

					/* Between addresses. */
					if (i->address > start && i->address < end)
						retn.emplace_back(i);

				}

				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

				this->remove_dupes(scopes); /* Remove duplicates. */
				this->remove_dupes(retn);
				this->sort_addr(retn); /* Sort retn by address. */

			} while (scopes.size());

			/* Nothing. */
			#if display_warnings 
				if (!retn.size()) {
						std::printf("[WARNING] No will be returned for visit_range.\n");
				}
			#endif

			return retn;
		}

		/* Visits all nodes between addresses (Includes curren, end) */
		std::vector<std::shared_ptr<node>> visit_range_current(const std::uintptr_t start, const std::uintptr_t end) {

			std::vector<std::shared_ptr<node>> retn;
			std::vector<block*> scopes = { this };

			do {

				auto current_block = scopes.front();

				/* Iterate through block nodes. */
				for (const auto& i : current_block->nodes) {

					/* Between addresses. */
					if (i->address >= start && i->address <= end)
						retn.emplace_back(i);

				}

				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

				this->remove_dupes(scopes); /* Remove duplicates. */
				this->remove_dupes(retn);
				this->sort_addr(retn); /* Sort retn by address. */

			} while (scopes.size());

			/* Nothing. */
			if (!retn.size())
				throw std::runtime_error("Returning no data for visit_range.");

			return retn;
		}

		/* Visits nodes that jump too address given with type (Must have memaddr operand) */
		template<lexer_dec::inst_type type>
		std::vector<std::shared_ptr<node>> visit_type_goto(const std::uintptr_t addr) {

			std::vector<std::shared_ptr<node>> retn;
			const auto all = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(this->visit_type<type>(true));

			for (const std::shared_ptr<ast_dec::node>& i : all) {

				const auto mems = i->lex->operand_expr<lexer_dec::operand_types::memaddr>();
				for (const auto& m : mems) 
					if (m->jmp_addr == addr) {
						retn.emplace_back(i);
						break;
					}
				
			}

			return retn;
		}

		private:

			/* Removes scope dupes. */
			void remove_dupes(std::vector<block*>& scopes) {
				std::sort(scopes.begin(), scopes.end());
				scopes.erase(std::unique(scopes.begin(), scopes.end()), scopes.end());
				return;
			}

			/* Removes node dupes from pair. (removes by address) */
			void remove_dupes(std::vector<std::pair <std::shared_ptr<node> /* Begin */, std::shared_ptr<node> /* End */>>& nodes) {
				std::sort(nodes.begin(), nodes.end());
				nodes.erase(std::unique(nodes.begin(), nodes.end()), nodes.end());
				return;
			}

			/* Removes node dupes. (removes by address) */
			void remove_dupes(std::vector<std::shared_ptr<node>>& nodes) {
				std::sort(nodes.begin(), nodes.end());
				nodes.erase(std::unique(nodes.begin(), nodes.end()), nodes.end());
				return;
			}

			/* Sorts nodes by addr. */
			void sort_addr(std::vector<std::shared_ptr<node>>& nodes) {
				if (nodes.size ())
					std::sort(nodes.begin(), nodes.end(), [](const std::shared_ptr<node>& a, const std::shared_ptr<node>& b) -> bool { return a->address < b->address; });
				return;
			}

			/* Sorts nodes by addr. */
			void sort_addr(std::vector<std::pair <std::shared_ptr<node> /* Begin */, std::shared_ptr<node> /* End */>>& nodes) {
				if (nodes.size())
					std::sort(nodes.begin(), nodes.end(), [](const std::pair <std::shared_ptr<node>, std::shared_ptr<node>>& a, const std::pair <std::shared_ptr<node>, std::shared_ptr<node>>& b) -> bool { return a.first->address < b.first->address; });
				return;
			}
	};

	struct ast {

		std::size_t ast_id = 0u; /* ID */


		Proto* p; /* Proto for ast. */
		std::unordered_map<std::uintptr_t, std::shared_ptr<LuaU_dissassembler::dissassembly>> dissassembly; /* Dissassembly of proto (Useful for some stuff). { PC, dissassembly } ex. dissassembly of pc=15 dissassembly[15]. */
		std::uintptr_t pc_end = 0u; /* Pc end */


		closure_type closure_type = closure_type::none;  /* Closure type. */
		std::string closure_name = ""; /* Closure name. (Suffix) */


		std::string closure_decompilation = ""; /* Handled by transpiler not ast used in transpiler for parent protos. */
		bool tanspiled = false;


		std::vector<std::pair <std::int16_t /* Reg */, std::string /* Name */>> arg_regs; /* Register for arguments to be placed in. *-1 means: ... */
		/* No node can refrence the same address all nodes are unique but branches can reference the same jump. */
		std::shared_ptr <block> main_block; /* Main block. */
		


		std::unordered_map<std::uintptr_t /* Idx */, std::pair <std::string /* Value */, std::int16_t /* Reg (-1 for upv) */>> upvalues;

		std::vector<std::shared_ptr <ast>> protos; /* Any children protos. Relates to proto->p */

		std::shared_ptr<transpiler_data::transpiler_config> transpiler_config; /* Linked transpiler config. */



		/* Finds block by start address. */
		std::shared_ptr <block> find_block(const std::uintptr_t addr) {

			/* Append all blocks. */
			std::vector<std::shared_ptr <block>> scopes = { this->main_block };

			do {

				auto current_block = scopes.front();

				if (current_block->node_start == addr)
					return current_block;

				/* Add nested blocks. */
				for (const auto& i : current_block->branches) 
					scopes.emplace_back(i);
				
				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

				/* Remove duplicates. */
				std::sort(scopes.begin(), scopes.end());
				scopes.erase(std::unique(scopes.begin(), scopes.end()), scopes.end());

			} while (scopes.size());

			return nullptr;
		}

		/* Finds block by containing address. */
		std::shared_ptr <block> find_block_addr(const std::uintptr_t addr) {

			/* Append all blocks. */
			std::vector<std::shared_ptr <block>> scopes = { this->main_block };

			do {

				auto current_block = scopes.front();

				if (current_block->node_start <= addr && current_block->node_end >= addr)
					return current_block;

				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i);

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

				/* Remove duplicates. */
				std::sort(scopes.begin(), scopes.end());
				scopes.erase(std::unique(scopes.begin(), scopes.end()), scopes.end());

			} while (scopes.size());

			return nullptr;
		}

		/* Turns ast into tree string. */
		std::string tree_str() {

			std::unordered_map <std::uintptr_t /* addr */, std::size_t /* amt */> indent_multiplier;
			std::string retn = "";
			std::uintptr_t pc = 0u;
			std::string indenting = "";
			std::vector<std::uintptr_t> labels;

			const auto branch = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(this->main_block->visit_next_type<lexer_dec::inst_type::branch>(true));
			const auto contional = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(this->main_block->visit_next_type<lexer_dec::inst_type::branch_condition>(true));

			/* Append jump backs. */
			for (const auto& node : branch) {
				labels.emplace_back(node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr);
			}

			for (const auto& node : contional) {
				if (node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp < 0) {
					labels.emplace_back(node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr);
				}
			}

			/* Append first */
			indent_multiplier.insert(std::make_pair(pc, 0u));

			do {

				const auto block = this->find_block(pc);
				const auto mult = indent_multiplier[pc];
				

				/* Compile indent */
				for (auto i = 0u; i < mult; ++i)
					indenting += "	";


				/* Compile nodes str */
				for (const auto& node : block->nodes) {

					auto dism = indenting + std::to_string (node->address) + " " + node->lex->dissassembly->data;

					/* Add label */
					if (std::find(labels.begin(), labels.end(), node->address) != labels.end()) {
						retn += "label_" + std::to_string(node->address) + ":\n";
					}

					/* Goto */
					if (node->lex->type == lexer_dec::inst_type::branch || (node->lex->type == lexer_dec::inst_type::branch_condition && (node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp < 0 || std::find(labels.begin (), labels.end(), node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr) != labels.end()))) {
						dism += " goto label_" + std::to_string (node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr) + ";";
					}
					
					dism += " ( ";
					for (const auto& p : node->expr)
						dism += node->expr_str(p) + " ";
					dism += ")\n";

					retn += dism;
				}


				/* Set mults */
				if (block->branches.size() ==  2u) {

					const auto branch_taken = block->branches.front()->node_start;
					const auto branch_not_taken = block->branches.back()->node_start;
					
					/* Most greater relative too branch_taken. */
					auto greater = 0u;
					for (const auto& i : indent_multiplier)
						if (i.first > greater && i.first < branch_taken) {
							greater = i.first;
						}

					/* Add indent to greater. */
					if (greater) {

						if (indent_multiplier.find(branch_taken) == indent_multiplier.end()) {
							indent_multiplier.insert(std::make_pair(branch_taken, indent_multiplier[greater]));
							labels.emplace_back(branch_taken);
						}

					}

					if (indent_multiplier.find(branch_taken) == indent_multiplier.end()) {
						indent_multiplier.insert(std::make_pair(branch_taken, mult));
					}

					if (indent_multiplier.find(branch_not_taken) == indent_multiplier.end()) {
						indent_multiplier.insert(std::make_pair(branch_not_taken, mult + 1u));
					}

				}	


				/* Set pc */
				pc = block->node_end + block->visit_addr(block->node_end)->lex->dissassembly->len;
				indenting.clear();

			} while (pc < this->pc_end);


			return retn;
		}

		std::string proto_information() {

			std::string retn = "";

			retn += "Proto:\n";
			retn += "	* name: " + closure_name + "\n";
			retn += "	* type: ";


			switch (this->closure_type) {

				case ast_dec::closure_type::main: {
					retn += "main";
					break;
				}

				case ast_dec::closure_type::local : {
					retn += "local";
					break;
				}

				case ast_dec::closure_type::newclosure: {
					retn += "newclosure";
					break;
				}

				case ast_dec::closure_type::global: {
					retn += "global";
					break;
				}

				default: {
					retn += "none";
					break;
				}

			}
			retn += "\n";

			retn += "	* arg count: " + std::to_string(this->arg_regs.size()) + "\n";
			retn += "	* args: ";

			for (const auto& i : this->arg_regs)
				retn += "r" + std::to_string(i.first) + "(" + i.second + ") ";

			retn += "\n";
			retn += "	* upvalue count: " + std::to_string(this->upvalues.size()) + "\n";
			retn += "	* upvalues: ";

			for (const auto& i : this->upvalues)
				retn += "r" + std::to_string(i.second.second) + "(" + i.second.first + ") ";

			return retn;
		}

	};

	std::shared_ptr<ast> gen_ast(Proto* proto, const std::shared_ptr<transpiler_data::transpiler_config>& config);

}