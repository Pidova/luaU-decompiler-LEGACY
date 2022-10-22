#pragma once
#include <unordered_map>
#include <variant>
#include <vector>
#include "../../dissassembler/Dissassembler.hpp"
#include "../lexer/lexer.hpp"

/*

	Standard ast. Nothing too special.

*/

namespace ast {

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

		while_, /* while () */
		do_, /* do */
		end_, /* end */
		break_, /* break */

		call_routine_start, /* Call routine start. */
		call_routine_end, /* Call routine end. */

		concat_routine_start, /* Concat routine start. */
		concat_routine_end, /* Concat routine end. */

		if_, /* if () */
		elseif_, /* elseif () */
		else_, /* else */
	    condition_and, /* if/elseif/nested(and) */
		condition_or, /* if/elseif/nested(or) */

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

		std::vector <expr_type> expr = { expr_type::lex }; /* Expression types. (Follows order) *All will get emmited(str). */
		std::size_t expr_count = 0u; /* Usally used for end, do, etc. */

		/* Convert destination to local? */
		struct dest_loc {
			bool is_dest_loc = false; /* Turns dest to local. */
			std::string name = "";
		};

		/* Extra information for branch. */
		struct branch_extra {
			bool opposite = false; /* Opposite compare from opcode. */
		};

		std::vector<std::shared_ptr<registers::reg>> regs; /* Pre-anlyzed register pointers. Everything else is covered by lexer. */
		std::shared_ptr<lexer::lexerme> lex; /* Node lexer data. Has all the detailed information. */

	};
	
	struct block {
		std::vector<std::shared_ptr<node>> nodes; /* Nodes in block. */
		std::vector<std::shared_ptr<block>> branches; /* 2 elements; first is branch taken second is not, 1 there is only a jump/loops (calls don't count), 0 no jumps.  */


		/* Visits first/all opcode value block. */
		template<LuauOpcode op>
		std::variant<std::vector<std::shared_ptr<node>>, std::shared_ptr<node>> visit_inst(const bool all /* All nodes with instruction. */) {

			std::vector<std::shared_ptr<node>> retn;
			std::vector<std::shared_ptr<block>> scopes = { std::shared_ptr<block>(this) };

			do {

				auto current_block = scopes[0];

				/* Iterate through block nodes and find given instruction. */
				for (const auto& i : current_block->nodes) {

					if (i->lex->dissassembly->op == op)
						if (all) /* Has all so emblace node. */
							retn.emplace_back(i);
						else /* Not all so return node. */
							return i;
				}

				/* Add nested blocks. */
				scopes.insert(scopes.end(), current_block->branches.begin(), current_block->branches.end());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

			} while (scopes.size ());

			return retn;
		}

		/* Visits next/all opcode from addr. (Ignores current.) */
		template<LuauOpcode op>
		std::variant<std::vector<std::shared_ptr<node>>, std::shared_ptr<node>> visit_next_inst(const std::uintptr_t addr, const bool all /* All nodes with instruction. */) {

			std::vector<std::shared_ptr<node>> retn;
			std::vector<std::shared_ptr<block>> scopes = { std::shared_ptr<block>(this) };

			do {

				auto current_block = scopes[0];

				/* Iterate through block nodes and find given instruction. */
				for (const auto& i : current_block->nodes) {

					if (i->address > addr && i->lex->dissassembly->op == op)
						if (all) /* Has all so emblace node. */
							retn.emplace_back(i);
						else /* Not all so return node. */
							return i;
				}

				/* Add nested blocks. */
				scopes.insert(scopes.end(), current_block->branches.begin(), current_block->branches.end());

				/* Remove current. */
				scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

			} while (scopes.size ());

			return retn;
		}


	};

	struct ast {

		Proto* p; /* Proto for ast. */
		std::unordered_map<std::uintptr_t, std::shared_ptr<LuaU_dissassembler::dissassembly>> dissassembly; /* Dissassembly of proto (Useful for some stuff). { PC, dissassembly } ex. dissassembly of pc=15 dissassembly[15]. */

		closure_type closure_type = closure_type::none;  /* Closure type. */
		std::string closure_name = ""; /* Closure name. */

		std::vector<std::int16_t> arg_regs; /* Register for arguments to be placed in. *-1 means: ... */
		std::shared_ptr <block> main_block; /* Main block. */

		std::vector<std::shared_ptr <ast>> protos; /* Any children protos. Relates to proto->p */

	};

	std::shared_ptr<ast> gen_ast(Proto* proto);
}