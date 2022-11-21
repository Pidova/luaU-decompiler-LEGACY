#pragma once
#include <algorithm>
#include <unordered_map>
#include <variant>
#include <vector>
#include "../../dissassembler/Dissassembler.hpp"
#include "../lexer/lexer_dec.hpp"

/*

	Standard ast. Nothing too special.

*/

namespace ast_dec {

	namespace registers {

		enum class type : std::uint8_t {
			none,
			expression,
			vararg,
			integer,
			boolean,
			upvalue,
			kvalue,
			proto
		}; 

		struct reg {

			std::uint16_t id = 0u; /* Register id. */
			type tt = type::none;

			union values {
				bool boolean;
				std::intptr_t integer;
				std::uintptr_t upvalue;
				std::uintptr_t proto;
				std::uintptr_t kvalue;
			};

			std::string container = "";
			
		};

	}

	/* Types of closures. */
	enum class closure_type : std::uint8_t {
		none,
		main, /* Main proto (Nothing). */
		local, /* local function test () */
		global, /* function test () */
		newclosure /* (function()  end)*/
	};

	/* Expression type. */
	enum class expr_type : std::uint8_t {
		lex, /* Specific type refer to lexer. */

		arith, /* r1 += r1 + r1 */
		arithK, /* r1 += r1 + 1 */

		for_iv_start, /* for i,v in pairs ({ 1 }) do */
		for_start, /* for start */

		repeat_, /* repeat */
		while_, /* while () follows condition. */
		until_, /* until () follows condition. */
		break_, /* break */
		scope_end, /* scope end */

		call_routine_start, /* Call routine start. */
		call_routine_end, /* Call routine end. */

		concat_routine_start, /* Concat routine start. */
		concat_routine_end, /* Concat routine end. */

		if_, /* if () */
		elseif_, /* elseif () */
		else_, /* else */
	    condition_and, /* if/elseif/nested(and) appends to if_statements (Can be applied to until or while) */
		condition_or, /* if/elseif/nested(or)  appends to if_statements (Can be applied to until or while) */

		close, /*   %s ) */
		open, /* ( %s   */

		table_start, /* Table { */
		nested_element, /* Nested element in table. */
		table_end, /* Table } */

		closure_local, /* local function test () */
		closure_global, /* function test () */
		closure_newclosure /* (function()  end)*/
	};

	struct node {

		std::uintptr_t address = 0u; /* Address. */

		std::vector <std::pair <expr_type, std::size_t /* Count usally used for ends, repeat, etc. */>> expr = { {expr_type::lex, 0u} }; /* Expression types. (Follows order) *All will get emmited(str). */

		/* Convert destination to local? */
		struct dest_loc {
			bool is_dest_loc = false; /* Turns dest to local. */
			std::string name = ""; /* Locvar name (Suffix) */
		} dest_loc;

		/* Extra information for branch. */
		struct branch_extra {
			bool opposite = false; /* Opposite compare from opcode. */
		} branch_extra;

		std::shared_ptr<lexer_dec::lexerme> lex; /* Node lexer data. Has all the detailed information. */
		
		/* Node functions. */
		template <expr_type type>
		void add_expr(const std::size_t count) {

			/* Replace only lex with type. */
			if (this->expr.size() == 1u) {
				this->expr.front().first = type;
				this->expr.front().second = count;
			}
			else 
				this->expr.emplace_back(std::make_pair(type, count));
				
			return;
		}

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

	};
	
	struct block {

		std::uintptr_t node_start = 0u; /* PC start. */
		std::uintptr_t node_end = 0u; /* PC final instruction. */

		std::vector<std::shared_ptr<node>> nodes; /* Nodes in block. */
		std::vector<std::shared_ptr<block>> branches; /* 2 elements; first is branch taken second is not, 1 there is only a jump/loops (calls\for\jumpbacks(serves as end) don't count, jump backs will refer to other nodes(may get fragmented)), 0 no jumps.  */


		/* All visits may be unorganized by address you may need to sort if needed. */

		/* Visits first/all opcode value block. */
		template<LuauOpcode op>
		std::variant<std::vector<std::shared_ptr<node>>, std::shared_ptr<node>> visit_inst(const bool all /* All nodes with instruction. */) {

			std::vector<std::shared_ptr<node>> retn;
			std::vector<block*> scopes = { this };

			do {
				
				auto current_block = scopes.front ();

				/* Iterate through block nodes and find given instruction. */
				for (const auto& i : current_block->nodes) {
					
					if (i->lex->dissassembly->op == op)
						if (all) /* Has all so emblace node. */
							retn.emplace_back(i);
						else /* Not all so return node. */
							return i;
				}
		
				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());
			
				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

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

					if (i->address > addr && i->lex->dissassembly->op == op)
						if (all) /* Has all so emblace node. */
							retn.emplace_back(i);
						else /* Not all so return node. */
							return i;
				}

				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

			} while (scopes.size ());

			/* Nothing. */
			if (!retn.size())
				throw std::exception("Returning no data for visit_next_inst.");

			return retn;
		}

		/* See if next opcode from addr exists. (Ignores current.) */
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

			} while (scopes.size());

			throw std::exception("Returning no data for visit_addr.");
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

					if (i->has_expr(type))
						if (all)
							retn.emplace_back(i);
						else
							return i;

				}

				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

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

					if (i->address > address && i->has_expr(type))
						if (all)
							retn.emplace_back(i);
						else
							return i;

				}

				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

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

			} while (scopes.size());

			throw std::exception("Returning no data for visit_addr.");
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

					if (i->lex->type == inst)
						if (all) /* Has all so emblace node. */
							retn.emplace_back(i);
						else /* Not all so return node. */
							return i;
				}

				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

			} while (scopes.size());

			return retn;
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

			} while (scopes.size());

			/* Node is null. */
			if (retn == nullptr)
				throw std::exception("Couldn't find previous dest based on register.");

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

					if (i->address >= on_address)
						retn.emplace_back(i);

				}

				/* Add nested blocks. */
				for (const auto& i : current_block->branches)
					scopes.emplace_back(i.get());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

			} while (scopes.size());

			if (!retn.size ())
				throw std::exception("Returning no data for visit_addr.");

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

			} while (scopes.size());

			throw std::exception("Returning no data for visit_relative_inst.");
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

			} while (scopes.size());

			throw std::exception("Returning no data for visit_relative_inst.");
		}


		/* Visits next relative node to expr being target and args being addatives till op hits = dec and args = inc(singular) and its 0.  (Ignores current) */
		template<expr_type target>
		std::shared_ptr<node> visit_relative_next_expr(const std::uintptr_t on_address, const std::vector<expr_type> rel) {

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
					for (const auto r : rel)
						if (i->has_expr(r)) {
							++count; 
							break;
						}

					/* If found op dec if count isnt 0. If it is 0 then return node. */
					if (i->has_expr(target))
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

			} while (scopes.size());

			throw std::exception("Returning no data for visit_relative_next_expr.");
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

			} while (scopes.size());

			if (!retn.size ())
				throw std::exception("Returning no data for visit_all.");

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

			} while (scopes.size());

			/* Nothing. */
			if (!retn.size())
				throw std::exception("Returning no data for visit_range.");

			return retn;
		}

	};

	struct ast {

		Proto* p; /* Proto for ast. */
		std::unordered_map<std::uintptr_t, std::shared_ptr<LuaU_dissassembler::dissassembly>> dissassembly; /* Dissassembly of proto (Useful for some stuff). { PC, dissassembly } ex. dissassembly of pc=15 dissassembly[15]. */
		std::uintptr_t pc_end = 0u; /* Pc end */

		closure_type closure_type = closure_type::none;  /* Closure type. */
		std::string closure_name = ""; /* Closure name. (Suffix) */

		std::vector<std::int16_t> arg_regs; /* Register for arguments to be placed in. *-1 means: ... */
		std::shared_ptr <block> main_block; /* Main block. */
		
		std::unordered_map<std::uintptr_t /* Idx */, std::pair <std::string /* Value */, std::uint16_t /* Reg*/>> upvalues;

		std::vector<std::shared_ptr <ast>> protos; /* Any children protos. Relates to proto->p */

	};

	std::shared_ptr<ast> gen_ast(Proto* proto);

}