#include <algorithm>
#include <iostream>
#include "ast_dec.hpp"

#define node_nonmutable(node) node->has_expr(ast_dec::expr_type::condition_nonmutable)

namespace ast_funcs {

	namespace proto {

		void set_closure_info(const std::shared_ptr<ast_dec::ast>& current_proto, const std::size_t child_proto_id) {

			std::shared_ptr<ast_dec::node> closure_node = nullptr;

			/* Newclosure, Dupclosures node. */
			auto closures = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(current_proto->main_block->visit_inst<LuauOpcode::LOP_NEWCLOSURE>(true));
			const auto dupclosures = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(current_proto->main_block->visit_inst<LuauOpcode::LOP_DUPCLOSURE>(true));

			/* All closures. */
			closures.insert(closures.end(), dupclosures.begin(), dupclosures.end());

			/* Iterate through closures and get expression for it relative to proto given. */
			for (const auto& i : closures) {

				/* Expression has been set. */
				if (closure_node != nullptr)
					break;

				switch (i->lex->dissassembly->op) {

					case LuauOpcode::LOP_NEWCLOSURE: {

						/* Set newcclosure node. */
						if (i->lex->dissassembly->operands[1]->proto == child_proto_id)
							closure_node = i; /* Set node. */

						break;
					}

					case LuauOpcode::LOP_DUPCLOSURE: {

						/* Get proto from dupclosure kvalue. */
						if (current_proto->p->p[child_proto_id] == gco2cl(current_proto->p->k[i->lex->dissassembly->operands[1]->k_idx].value.gc)->l.p)
							closure_node = i; /* Set node. */

						break;
					}

					default: {
						throw std::exception("Unkown opcode for closure_type.");
					}

				}

			}

			/* Turn expression to closure type. */
			for (const auto& e : closure_node->expr) {

				auto& proto = current_proto->protos[child_proto_id];

				switch (e.first) {

					/* Comes with closure name. */
					case ast_dec::expr_type::closure_global: {
						proto->closure_type = ast_dec::closure_type::global;
						proto->closure_name = std::get<std::shared_ptr<ast_dec::node>>(current_proto->main_block->visit_next_inst<LuauOpcode::LOP_SETGLOBAL>(closure_node->address, false))->lex->dissassembly->operands[2]->k_value;
						break;
					}

					/* Closure name doesn't get compiled unless specified. */
					case ast_dec::expr_type::closure_local: {
						proto->closure_type = ast_dec::closure_type::local;
						break;
					}

					/* No closure name. */
					case ast_dec::expr_type::closure_newclosure: {
						proto->closure_type = ast_dec::closure_type::newclosure;
						break;
					}

					/* Shouldn't happen but incase it does. */
					default: {
						throw std::exception("Unkown expression for closure_type.");
					}

				}

			}

			return;
		}

	}

	namespace branches {
		
		template <ast_dec::expr_type tt, bool loadb = false /* loadb influences compare (conditional operations) */>
		void set(std::shared_ptr<ast_dec::ast>& ast, const std::uintptr_t begin, const std::uintptr_t end) {

			std::uintptr_t ignore_till_addr = 0u;

			const auto range = ast->main_block->visit_range(begin, end);
			const auto loadbs = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_LOADB>(true));

			for (const auto& node : range) {

				/* Used for loadb jump. */
				if (ignore_till_addr && node->address > ignore_till_addr)
					continue;

				if (node->lex->type == lexer_dec::inst_type::branch_condition || node->lex->type == lexer_dec::inst_type::branch) {

					const auto jmp = node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr;

				}

				/* Check loadb as end or data. */
				if (node->lex->dissassembly->op == LuauOpcode::LOP_LOADB) {

					const auto jmp = node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front();

					/* Jump. (Logical operation) */
					if (jmp->jmp) {

						ignore_till_addr = jmp->jmp_addr;

						const auto jmp_target = ast->main_block->visit_addr(ignore_till_addr);

						/* Next loadb exists so check to see if it has a jump. */
						if (ast->main_block->has_next_inst<LuauOpcode::LOP_LOADB>(jmp_target->address)) {

							bool next_bool_jump = false;

							for (const auto& lb : loadbs) {

								/* Fits in range and has jump. */
								if (lb->address > jmp_target->address && lb->address < end && lb->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp) {
									next_bool_jump = true;
									break;
								}

							}

							/* Next bools in the routines don't have jumps. */
							if (!next_bool_jump) {

								node->add_expr<tt>();

							}
							else { /* Next bools in the routine does have jump. */

								/* Conditional for the set jmp and dead for the jmp target. */
								node->add_expr<ast_dec::expr_type::conditional>();						

							}

							jmp_target->add_expr<ast_dec::expr_type::dead_instruction>();

						}
						else {

							/* Mark loadb so it gets analyzed. */
							ast->main_block->visit_addr(ignore_till_addr)->add_expr<ast_dec::expr_type::dead_instruction>();

						}

					}

				}

			}

			ast->main_block->visit_addr(end)->add_expr<tt>();

			return;
		}

	}

	namespace arguments {

		/* 
			*Note: This isn't perfect and only sets args useful to that proto Ie. print (arg1). Things that arent neccesarly useful like
				   an argument getting set right after or inside of a routine before getting used will most of the time just become a 
				   variable depending on some conditions like if it gets used after a condition but it can be set by that condition not
				   garunteed it will become an argument. All of this is put together this way instead of basing everything off of it's
				   arg stack become random args that don't even get used.
		*/
		void set_arguments(std::shared_ptr<ast_dec::ast>& ast) {

			std::vector <std::uint32_t> dests; /* Registers used in dest. **getting written too** */
			std::vector <std::uint32_t> source_no_dest; /* Registers used in source, value but not dest. **Not written too yet but been used** */


			/* Everything needs to go through based on control flow. */
			const auto all = ast->main_block->visit_all();
			for (const auto& node : all) {

				/* Loops usally overwrite some regs per part of there routine append them to dest. */
				if (node->lex->type == lexer_dec::inst_type::for_) {

					auto start_reg = 0u; /* for start */
					auto iteration = 0u; /* for regs being consumed for its operation */

					switch (node->lex->dissassembly->op) {

						case LuauOpcode::LOP_FORGLOOP: {
							start_reg = node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg;
							iteration = 4u;
							break;
						}
						case LuauOpcode::LOP_FORNLOOP: {
							start_reg = node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg;
							iteration = 2u;
							break;
						}

						/* Could be loop prep??? maybe */
						default: {
							continue;
						}

					}

					/* Add vars from those loops. */
					for (auto i = start_reg; i < (start_reg + iteration + 1u); ++i)
						if (std::find(dests.begin(), dests.end(), i) == dests.end()) /* Found dest */
							dests.emplace_back(i);

				}

				
				switch (node->lex->dissassembly->op) {
					
					case LuauOpcode::LOP_RETURN: {

						const auto dest = node->lex->operand_expr<lexer_dec::operand_types::reg>().front()->reg;
						auto amt = node->lex->operand_expr<lexer_dec::operand_types::integer>().front()->val;

						if (amt) {

							if (amt == -1)
								amt = (*(&node - 1u))->lex->dissassembly->operands.front()->reg;

							for (auto a = dest; a < (dest + amt); ++a)
								if (std::find(dests.begin(), dests.end(), a) == dests.end())
									dests.emplace_back(a);

						}

						break;
					}

					case LuauOpcode::LOP_GETVARARGS: {

						const auto dest = node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;
						auto amt = node->lex->operand_expr<lexer_dec::operand_types::integer>().front()->val;

						if (amt == -1)
							amt = (*(&node - 1u))->lex->dissassembly->operands.front()->reg;

						for (auto a = dest; a < (dest + amt); ++a)
							if (std::find(dests.begin(), dests.end(), a) == dests.end())
								dests.emplace_back(a);

						break;
					}

					case LuauOpcode::LOP_CALL: {

						auto args = node->lex->operand_expr<lexer_dec::operand_types::integer>().front()->val;
						const auto start = node->lex->dissassembly->operands.front()->reg;

						if (args == -1)
							args = ((*(&node - 1u))->lex->dissassembly->operands.front()->reg - start);

						/* Iterate through args and see if arg is not getting used in dest. */
						for (auto i = 0; i < args; ++i) {

							const auto arg = (i + start + 1u);

							if (std::find(dests.begin(), dests.end(), arg) == dests.end() &&
								std::find(source_no_dest.begin(), source_no_dest.end(), arg) == source_no_dest.end())
									source_no_dest.emplace_back(arg);

						}

						/* Add placement for call. */
						if (node->lex->has_operand_expr<lexer_dec::operand_types::dest>() && std::find(dests.begin(), dests.end(), start) == dests.end() &&
							std::find(source_no_dest.begin(), source_no_dest.end(), start) == source_no_dest.end())
								source_no_dest.emplace_back(start);

						break;
					}

					default: {

						/* Append source. */
						if (node->lex->has_operand_expr<lexer_dec::operand_types::source>()) {

							const auto regz = node->lex->operand_expr<lexer_dec::operand_types::source>();

							for (const auto& operand : regz) {

								/* Append unused reg. */
								if (std::find(dests.begin(), dests.end(), operand->reg) == dests.end() && std::find(source_no_dest.begin(), source_no_dest.end(), operand->reg) == source_no_dest.end())
									source_no_dest.emplace_back(operand->reg);

							}

						}


						/* Append reg. */
						if (node->lex->has_operand_expr<lexer_dec::operand_types::reg>()) {

							const auto regz = node->lex->operand_expr<lexer_dec::operand_types::reg>();

							for (const auto& operand : regz) {

								/* Append unused reg. */
								if (std::find(dests.begin(), dests.end(), operand->reg) == dests.end() && std::find(source_no_dest.begin(), source_no_dest.end(), operand->reg) == source_no_dest.end())
									source_no_dest.emplace_back(operand->reg);

							}

						}


						/* Append dest. */
						if (node->lex->has_operand_expr<lexer_dec::operand_types::dest>() && std::find(dests.begin(), dests.end(), node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg) == dests.end()) {

							const auto reg = dests.emplace_back(node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg);

							dests.emplace_back(reg);

							/* Append this. */
							if (node->lex->dissassembly->op == LuauOpcode::LOP_NAMECALL)
								dests.emplace_back(reg + 1u);
						}

						break;
					}

				}

			}
			
			return;
		}

	}

	namespace concats {

		/*
			Concat routines in luaU are a bit special. When a concat opcode is called the start register and end register operand assemble the concat too
			get placed as a dest. This can be used too find it's routine by getting the preceeding dest of the start and the end being the concat we can determine
			the routine and where it starts and end.
		*/
		void set_routines(std::shared_ptr<ast_dec::ast>& ast) {

			const auto concats = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_CONCAT>(true));

			/* Set concat info. */
			for (const auto& node : concats) {
				ast->main_block->visit_previous_dest_register(node->address, node->lex->dissassembly->operands.front()->reg)->add_expr<ast_dec::expr_type::concat_routine_start>(); /* Concat start. */
				node->add_expr<ast_dec::expr_type::concat_routine_end>(); /* Concat end. */
			}

			return;
		}

	}

	namespace calls {

		/* 
			Calls are a bit special in luaU because they rely on parent registers for both arguments and stack. Using this call routines usally get assembled right before
			a call opcode is executed. We get the beggining of this by judging by it's parent target register stack found in the call opcode in previous code. Note you can
			assemble the call and everything ahead of time and call later but that is very impracticle and very unoptimized which is generally not done.
		*/
		void set_routines(std::shared_ptr<ast_dec::ast>& ast) {

			const auto calls = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_CALL>(true));
			
			/* Set call info. */
			for (const auto& node : calls) {
					ast->main_block->visit_previous_dest_register(node->address, node->lex->dissassembly->operands.front()->reg)->add_expr<ast_dec::expr_type::call_routine_start>(); /* Call start. */
					node->add_expr<ast_dec::expr_type::call_routine_end>(); /* Call end. */
			}

			return;
		}

	}

	namespace loops {

		/* 
			For loops in luaU are generally easy to find based on it instruction jumpback range and where a for loop instruction is though this only finds
			start and end of the routine not where variables get assembled that is usally self handled by the transpiler and IGNORED by locvar huertistics
			though special cases may apply changing this.
		*/
		void set_for_routines(std::shared_ptr<ast_dec::ast>& ast) {

			const auto forgloops = std::get<std::vector<std::shared_ptr<ast_dec::node>>> (ast->main_block->visit_inst<LuauOpcode::LOP_FORGLOOP>(true));
			const auto fornloops = std::get<std::vector<std::shared_ptr<ast_dec::node>>> (ast->main_block->visit_inst<LuauOpcode::LOP_FORNLOOP>(true));
		
			/* Forgloops. */
			for (const auto& forloop : forgloops) {

				const auto jump_node = ast->main_block->visit_addr(forloop->lex->dissassembly->operands[1]->jmp_addr);
				const auto jump_inst = jump_node->lex->dissassembly->op;

				/* for i,v in ipairs/pairs */
				if (jump_inst == LuauOpcode::LOP_FORGPREP_INEXT || jump_inst == LuauOpcode::LOP_FORGPREP_NEXT)
					jump_node->add_expr<ast_dec::expr_type::for_iv_start>();
				else
					jump_node->add_expr<ast_dec::expr_type::for_start>();

				forloop->add_expr<ast_dec::expr_type::scope_end>();

			}

			/* Fornloops. */
			for (const auto& forloop : fornloops) {

				ast->main_block->visit_addr(forloop->lex->dissassembly->operands[1]->jmp_addr)->add_expr<ast_dec::expr_type::for_start>();
				forloop->add_expr<ast_dec::expr_type::scope_end>();

			}

			return;
		}

		/*
			While and repeat loops are "pretty tricky" to detect. 
				* Repeat: The compare will come at the end of the routine and is near a jumpback instruction.
				* While:  The compare will come at the beginging of the routine but its jump will go past jumpback instruction.
		
			This issue with this is not determing which one it is, it is determing which one is a valid while,until expression then a if/elseif.
			Though some hueristics can be used to mitigate this problem and determine if its apart of the loop or not:
				* If the jump exceeds the jumpback opcode or goes directly too it.
				* If a variable doesn't get written between the next branch if there proceeds a jump.
				* Opcodes that shouldn't be in between the next branch and proceeding jump (ie. return/nop/etc).
				* Use of any of the compares used as dest between the next branch and proceeding jump.

		*/
		void set_whilerep_routines(std::shared_ptr<ast_dec::ast>& ast) {

			std::vector<std::shared_ptr<ast_dec::node>> nodes;

			const auto jump_backs = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_JUMPBACK>(true));
			const auto jump_conds = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_next_type<lexer_dec::inst_type::branch_condition>(true));


			/* Check typical jumpbacks *Previous inst is condition its until else end for while. */
			for (const auto& jmp_back : jump_backs) {

				if (ast->main_block->visit_previous_addr(jmp_back->address)->lex->type == lexer_dec::inst_type::branch_condition) {
					jmp_back->add_expr<ast_dec::expr_type::until_>(); /* Until end. */
					nodes.emplace_back(jmp_back);
				}
				else {
					jmp_back->add_expr<ast_dec::expr_type::scope_end>(); /* While loop end. */
					nodes.emplace_back(jmp_back);
				}

			}

			/* See if jump memaddrs are negatives usally means there until. */
			for (const auto& jmp_back : jump_conds) 
				if (jmp_back->lex->dissassembly->operands[std::find(jmp_back->lex->operands.begin(), jmp_back->lex->operands.end(), lexer_dec::operand_types::memaddr) - jmp_back->lex->operands.begin()]->jmp < 0) { /* See if mem address of jump is negative (We need to get idx of memaddr operand). */
					jmp_back->add_expr<ast_dec::expr_type::until_>(); /* Until end. */
					nodes.emplace_back(jmp_back);
				}

			/* We need to get range of until/while rotuines conditions. */
			for (const auto& jumpback : nodes) {

				/* See if jumpback is a branch. */
				if (jumpback->lex->type != lexer_dec::inst_type::branch_condition && jumpback->lex->type != lexer_dec::inst_type::branch)
					throw std::exception("until/while branch jumpback isn't a branch.");

				const auto nodes_routine = ast->main_block->visit_range(jumpback->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr, jumpback->address);


				std::shared_ptr<ast_dec::node> marked_node = nullptr;
				

				if (jumpback->has_expr(ast_dec::expr_type::until_)) {

					/* End of routine is given with until. Find begging. */
					for (const auto& node : nodes_routine) {

						if (node->lex->type == lexer_dec::inst_type::branch_condition || node->lex->type == lexer_dec::inst_type::branch) {

							/* Break */
							if (node->lex->dissassembly->op == LuauOpcode::LOP_JUMP || node->lex->dissassembly->op == LuauOpcode::LOP_JUMPX) {

								if (node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr > jumpback->address)
									node->add_expr<ast_dec::expr_type::break_>();

								continue;
							} 

							/* Only has one compare or couldnt find anything else. */
							if ((node->address + node->lex->dissassembly->len) == jumpback->address) {

								if (node_nonmutable(node))
									throw std::exception("Until node cannot non-mutable.");

								node->add_expr<ast_dec::expr_type::condition_nonmutable>();
								break;
							}

							/* 
								Jumps too jumpback address.
									* Can mean justs ands in the compare condition.
									* If there is a compare after the jumpback it must exceed the jumpback if not then the loop is just a until(true/false);
							*/
							if (node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr == jumpback->address) {

								const auto previous = ast->main_block->visit_previous_addr(jumpback->address);

								/* Isn't a until(true/false) loop. */
								if (previous->lex->type == lexer_dec::inst_type::branch_condition || previous->lex->type == lexer_dec::inst_type::branch) {
									branches::set<ast_dec::expr_type::until_>(ast, node->address, jumpback->address);
									break;
								}
								else { /* Is a until(true/false) loop. */
									/* 	until(true/false) loop data will get handeled by transpiler if compareflag is 0. */
									break;
								}

							}

						}

					}

				}
				else {

					/* Begining of routine is given with until. Find end. */
					for (const auto& node : nodes_routine) {

						if (node->lex->type == lexer_dec::inst_type::branch_condition || node->lex->type == lexer_dec::inst_type::branch) {

							/* Break */
							if (node->lex->dissassembly->op == LuauOpcode::LOP_JUMP || node->lex->dissassembly->op == LuauOpcode::LOP_JUMPX) {

								if (node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr > jumpback->address)
									node->add_expr<ast_dec::expr_type::break_>();

								continue;
							}





						}


					}


				}

			}
			
			return;
		}
		
	}

	namespace tables {

		void set_routines(std::shared_ptr<ast_dec::ast>& ast) {

		    auto tables = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_NEWTABLE>(true));
			const auto duptables = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_DUPTABLE>(true));		
			tables.insert(tables.end(), duptables.begin(), duptables.end());

			std::vector<std::uintptr_t> ends; /* Used to avoid encapsulation with tables as this is for ends of already analyzed tables. */

			for (const auto& table : tables) {

				/* Check */
				bool valid = true;
				for (const auto i : ends)
					if (table->lex->dissassembly->addr <= i)
						valid = false;

				if (!valid)
					break;


				std::uintptr_t node_size = 0u;
				std::uintptr_t array_size = 0u; 
				std::uintptr_t predicted_size = 0u; /* Previous power of 2 for array_size, gives us more context on end. */


				/* Sizes for scopes of table. (Used for scopes of nested tables) */
				std::vector<std::uintptr_t> predicted_sizes;
				std::vector<std::uintptr_t> node_sizes;
				std::vector<std::uintptr_t> array_sizes;



				/* Cache data for scopes. */
				auto cache = [&](const bool start) mutable -> void {

					if (start) {
						predicted_sizes.emplace_back(predicted_size);
						node_sizes.emplace_back(node_size);
						array_sizes.emplace_back(array_size);
					}
					else {

						predicted_sizes.pop_back();
						node_sizes.pop_back();
						array_sizes.pop_back();

						/* Check size */
						if (predicted_sizes.size())
							predicted_size = predicted_sizes.back();

						if (node_sizes.size())
							node_size = node_sizes.back();

						if (array_sizes.size())
							array_size = array_sizes.back();

					}

				};

				/* Set size for node_size and array_size also sets table elements/start/end. */
				auto set_size = [&](const std::shared_ptr<ast_dec::node>& node) mutable -> void {

					switch (node->lex->dissassembly->op) {

						case LuauOpcode::LOP_NEWTABLE: {

							const auto operands = table->lex->operand_expr<lexer_dec::operand_types::integer>();
							auto x = operands.front()->table_size;
							node_size = x;
							array_size = operands.front()->val;

							--x;
							x = x | (x >> 1);
							x = x | (x >> 2);
							x = x | (x >> 4);
							x = x | (x >> 8);
							x = x | (x >> 16);
							predicted_size = x - (x >> 1);

							cache(true);
							node->add_expr<ast_dec::expr_type::table_start>();
							break;
						}

						case LuauOpcode::LOP_DUPTABLE: {

							auto x = std::stoi(node->lex->dissassembly->operands[1]->k_value.c_str());
							node_size = x;

							--x;
							x = x | (x >> 1);
							x = x | (x >> 2);
							x = x | (x >> 4);
							x = x | (x >> 8);
							x = x | (x >> 16);
							predicted_size = x - (x >> 1);
							cache(true);
							node->add_expr<ast_dec::expr_type::table_start>();
							break;
						}


						case LuauOpcode::LOP_SETTABLE:
						case LuauOpcode::LOP_SETTABLEKS:
						case LuauOpcode::LOP_SETTABLEN: {
							--node_size;
							node->add_expr<ast_dec::expr_type::table_element>();
							break;
						}

						case LuauOpcode::LOP_SETLIST: {

							array_size -= node->lex->dissassembly->operands[2]->val;
							node->add_expr<ast_dec::expr_type::table_end>();
							ends.emplace_back(table->address);
							cache(false);

							/* Still unused array members. */
							if (array_size)
								throw std::exception("Expected array size too be 0 for SETLIST instruction.");

							break;
						}

					}

					return;
				};
				

			
				/* Add first info */
				if (table->lex->dissassembly->op == LuauOpcode::LOP_NEWTABLE || table->lex->dissassembly->op == LuauOpcode::LOP_DUPTABLE) {
					
					/* Has table members. */
					set_size(table);
					if (array_size || node_size) {
						
						std::uint16_t reg = 0u; /* Previous register dest. */

						const auto nodes = ast->main_block->visit_rest(table->address);
						for (const auto& node : nodes) {

							set_size(node);
		
							/* Set previous dest register. */
							if (node->lex->has_operand_expr<lexer_dec::operand_types::dest>())
								reg = node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;


							/* Node_size equals predicted_size or is less then predicted_size that means that it exceeded predicted_size. */
							if (node_size && predicted_size >= node_size) {


								/* Table set so check if it's the end. */
								if (node->lex->type == lexer_dec::inst_type::table_set) {


									/* Target table. */
									const auto table_reg = node->lex->operand_expr<lexer_dec::operand_types::reg>().front()->reg;

									const auto target_2 = (node->lex->dissassembly->op == LuauOpcode::LOP_SETTABLE) ? node->lex->operand_expr<lexer_dec::operand_types::table_idx>().front()->reg : -1; /* Index for SETTABLE. */
									const auto target_1 = node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg; /* Source data can be idx. */
									bool used_target_1 = false;
									bool used_target_2 = false;

								
									const auto rest_nodes = ast->main_block->visit_rest(table->address);
									for (const auto& i : rest_nodes) {
										
										/* Checks operands if targets get used or not but if it does get used just resets target. */
										std::function<void(const std::shared_ptr<LuaU_dissassembler::operand>&, const lexer_dec::operand_types)> check_usage = [&](const std::shared_ptr<LuaU_dissassembler::operand>& operand, const lexer_dec::operand_types tt) mutable {
											
											const auto val = operand->reg;

											if (val == target_1)
												used_target_1 = false;

											if (target_2 != -1 && signed(val) == target_2)
												used_target_2 = false;

											return;
										};


										/* Check reg operands for usage. */
										i->lex->operand_expr_callback<lexer_dec::operand_types::source>(check_usage);
										i->lex->operand_expr_callback<lexer_dec::operand_types::compare>(check_usage);
										i->lex->operand_expr_callback<lexer_dec::operand_types::reg>(check_usage);

										
										if (i->lex->has_operand_expr<lexer_dec::operand_types::dest>()) {

										}
										else {
											// TODO check end for table end with settable not always end for after power of 2.
											/* No dest so node is final for table. */
											--node_size; 
											node->add_expr<ast_dec::expr_type::table_end>(); 
											cache(false);
											break;
										}

									}

								}

							}

							/* Found end, end anlysis. */
							if (!node_size && !array_size) {
								node->add_expr<ast_dec::expr_type::table_end>();
								ends.emplace_back(table->address);
								continue;
							}

						}

					}
					else {
						/* No table members. */
						table->add_expr<ast_dec::expr_type::table_start>();
						table->add_expr<ast_dec::expr_type::table_end>();
						ends.emplace_back(table->address);
					}

				}
				else {
					throw std::exception("Expected NEWTABLE or DUPTABLE instruction to init table.");
				}

			}

			return;
		}

	}

	namespace locvars {

		/*
		
			Sets locvars based on certain hueristics. More detailed info about it can be found in ast_config.hpp at control_flow_vars macro.
			Changing that will effect the behavior of this function but all the information you need can be found there but judge your discision
			on how you want the code to be in the decompilation. This isn't 100% perfect because where mostly judging on hueristics and nothing else.
			Depending on how the code is decompiled as of this version of luaU debug information contains stuff about variables (names, etc) but there isn't 
			a garunteed it will be present or not so this will try to recreat that.
		
		*/
		void set_lv(std::shared_ptr<ast_dec::ast>& ast, const std::uint16_t start_reg) {

			std::vector<std::uint16_t> registers = { start_reg }; /* Registers for scope. (Based on target register) */
			auto target = start_reg; /* Target register. */
			std::uintptr_t routine = 0u; /* Inside concat, call, table routine, inc for start, dec for end. */

			for (const auto& node : ast->main_block->visit_all()) {


				/* Turns node into variable. */
				auto node_var = [&](const std::shared_ptr<LuaU_dissassembler::operand>& operand) -> void {

					/* Not set yet. */
					if (operand->reg == target) {
						node->dest_loc.is_dest_loc = true;
						node->dest_loc.name = std::to_string(target);
						++target;
					}

					return;
				};


				/* End of scope. */
				for (auto i = 0u; i < node->count_expr <ast_dec::expr_type::scope_end>(); ++i)
					registers.pop_back();

				/* Log reg scope start. */
				if (node->lex->scope_start()) {
					target = registers.back();
					registers.emplace_back(target);
				}


				/* Inc for concat start, call start, and table start. Dec for concat end, call end, and start end. */
				routine += node->count_expr <ast_dec::expr_type::concat_routine_start>() + node->count_expr <ast_dec::expr_type::call_routine_start>() + node->count_expr <ast_dec::expr_type::table_start>();
				routine -= node->count_expr <ast_dec::expr_type::concat_routine_end>() + node->count_expr <ast_dec::expr_type::call_routine_end>() + node->count_expr <ast_dec::expr_type::table_end>();


				/* Not inside routine and dest. */
				if (!routine && node->lex->has_operand_expr<lexer_dec::operand_types::dest>()) {

					auto bad = false; /* Failed any checks. (Can also be used if node is alr set. */
					const auto dest = node->lex->operand_expr<lexer_dec::operand_types::dest>().front ();


					/* Capture with source garunteeds locvar so check there. */
					const auto captures = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_CAPTURE>(true));
					for (const auto& capture : captures)
						if (capture->lex->has_operand_expr<lexer_dec::operand_types::source>() && capture->lex->operand_expr<lexer_dec::operand_types::source>().front ()->capture_reg == target) {
							node_var(dest);
							bad = true;
							break;
						}
					if (bad)
						continue;
					 

					/* Check concat and call routines if the dest is used as a dest in them no locvar. */
					const auto calls = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_next_expr<ast_dec::expr_type::call_routine_start>(node->address, true));
					for (const auto& call : calls) {
						
						const auto node_end = ast->main_block->visit_relative_next_expr<ast_dec::expr_type::call_routine_end>(call->address, { ast_dec::expr_type::call_routine_start });
						
						/* Target dest reg used in call routine dest. */
						for (const auto& call_node : ast->main_block->visit_range(call->address, node_end->address))
							if (call_node->lex->has_operand_expr<lexer_dec::operand_types::dest>() && call_node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg == target)
								bad = true;
						
					}
					if (bad)
						continue;
					
					/* Concat */
					const auto concats = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_next_expr<ast_dec::expr_type::concat_routine_start>(node->address, true));
					for (const auto& concat : concats) {

						const auto node_end = ast->main_block->visit_relative_next_expr<ast_dec::expr_type::call_routine_start>(concat->address, { ast_dec::expr_type::concat_routine_start });

						/* Target dest reg used in call routine dest. */
						for (const auto& concat_node : ast->main_block->visit_range(concat->address, node_end->address))
							if (concat_node->lex->has_operand_expr<lexer_dec::operand_types::dest>() && concat_node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg == target)
								bad = true;

					}
					if (bad)
						continue;

					/* 
					
						After checking concat and call routines which can garunteed a variable this is where we mostly look at hueristics and decide if
						a register will become a variable or not. Either one it's set too will effect the preformance of the decompiled code and accuracy.
					
					*/


				}

			}

			return;
		}

	}

	void init_ast(std::shared_ptr<ast_dec::ast>& ast) {
		ast_funcs::arguments::set_arguments(ast);
		ast_funcs::calls::set_routines(ast);
		ast_funcs::concats::set_routines(ast);
		ast_funcs::loops::set_whilerep_routines(ast); /* Needed after concat can mess up if before or after. (expr_type::scope_end needed only for while end) */
		ast_funcs::loops::set_for_routines(ast);
		return;
	}

}

namespace blocks {

	/* Set singular block *Jumpbacks act as end. */
	std::tuple <std::vector<std::shared_ptr<ast_dec::node>>, std::uintptr_t /* Start next pc. */, std::uintptr_t /* Final instruction. */> init_current_block(std::shared_ptr<ast_dec::ast>& ast, std::uintptr_t pc, std::vector<std::uintptr_t>& branch_ends) {
	
		std::vector<std::shared_ptr<ast_dec::node>> retn;
	
		/* Init basic node data. */
		do {
			
			auto current_dissassembly = ast->dissassembly[pc];

			/* Pc is already at a branch ending. So just return empty vector. */
			if (std::binary_search(branch_ends.begin(), branch_ends.end(), pc))
				break;

			/* Set current node. */
			auto node = std::make_shared<ast_dec::node>();
			retn.emplace_back(node);

			/* Set node data. */
			node->address = pc;
			node->lex = lexer_dec::lexer(current_dissassembly);
			
			/* Branch/end so break. */
			if (node->lex->type == lexer_dec::inst_type::branch || node->lex->type == lexer_dec::inst_type::branch_condition || (pc + current_dissassembly->len) == ast->p->sizecode)
				break;		

			pc += current_dissassembly->len;

		} while (true /* Earlier code will exit if hit a branch or passed branch ends. */);
		
		return std::make_tuple(retn, pc + ast->dissassembly[pc]->len /* Skip current instruction. */, pc);
	}


	/* Set blocks for current ast. */
	void set_blocks(std::shared_ptr<ast_dec::ast>& ast) {

		std::uintptr_t pc = 0u;
		std::vector<std::uintptr_t> branch_ends;
		std::unordered_map <std::uintptr_t /* PC(start) */, std::tuple <std::uintptr_t  /* PC(end) */, std::uintptr_t  /* PC(end(end + curr->len)) */, std::vector<std::shared_ptr<ast_dec::node>> /* Nodes */>> linear_blocks; /* Block data for scopes. */


		/* Set each end as every branch taken pc. */
		for (auto& dism : ast->dissassembly) {

			const auto temp_lex = lexer_dec::lexer(dism.second);

			/* Don't log if jumpback. */
			if (temp_lex->dissassembly->op == LuauOpcode::LOP_JUMPBACK)
				continue;

			if (temp_lex->type == lexer_dec::inst_type::branch || temp_lex->type == lexer_dec::inst_type::branch_condition) {

				/* Jump with no memaddr operand idk how this has happened. */
				if (!temp_lex->has_operand_expr<lexer_dec::operand_types::memaddr>())
					throw std::exception("Jump with no memaddr operand in lexer at set_blocks.");

				branch_ends.emplace_back(temp_lex->operand_expr<lexer_dec::operand_types::memaddr>().front ()->jmp_addr);
				
			}
		
		}


		/* Set main block. */
		ast->main_block = std::make_shared<ast_dec::block>();


		/* Append blocks */
		do {
			
			const auto block = init_current_block(ast, pc, branch_ends);
			linear_blocks.insert(std::make_pair(pc, std::make_tuple(std::get<2>(block), std::get<1>(block), std::get<0>(block))));

			pc = std::get<1>(block);

		} while (pc < ast->p->sizecode /* Pc didn't exceed sizecode. */);


		/* Reset PC. */
		pc = 0u;
	

		/* Init main. (Block search is dependent on main so needs to be seperate from everything). */
		ast->main_block->node_start = pc;
		ast->main_block->node_end = std::get<0>(linear_blocks[pc]);
		ast->main_block->nodes = std::get<2>(linear_blocks[pc]);


		/* Set pc. */
		pc = std::get<1>(linear_blocks[pc]);;


		/* Assemble blocks. */
		while (pc < ast->p->sizecode /* Pc didn't exceed sizecode. */) {

			const auto& block = linear_blocks[pc];

		};
		
		return;
	}

}

std::shared_ptr<ast_dec::ast> ast_dec::gen_ast(Proto* proto) {

	/* Current ast. */
	auto retn = std::make_shared<ast>();
	std::vector<std::shared_ptr<ast>> protos_ast = { retn }; /* All protos including nested protos. */

	/* Set closure and proto. */
	retn->closure_type = closure_type::main;
	retn->p = proto;
	
	do {


		std::uintptr_t pc = 0u;
		auto current_proto = protos_ast[0];

		/* Set current proto dissasembly. */
		for (auto i = 0u; i < unsigned (proto->sizecode);) {
			auto dism = std::make_shared<LuaU_dissassembler::dissassembly>();
			LuaU_dissassembler::dissassemble(pc, current_proto->p, dism);
			current_proto->dissassembly.insert(std::make_pair(pc, dism));
			pc += dism->len;
			i += dism->len;
		}

		/* Set current proto blocks. */
		blocks::set_blocks(current_proto);

		/* Gen children proto to analyze. */
		for (auto i = 0u; i < unsigned (current_proto->p->sizep); ++i) {
			
			/* Make child ast and add proto and get type. */
			auto child_ast = std::make_shared<ast>();
			child_ast->p = current_proto->p->p[i];
			ast_funcs::proto::set_closure_info(current_proto, i);

			/* Add child to get analyzed. */
			protos_ast.emplace_back(child_ast);
		}

		/* Init ast. */
		ast_funcs::init_ast(current_proto);

		/* Remove current. */
		protos_ast.erase(std::remove(protos_ast.begin(), protos_ast.end(), current_proto), protos_ast.end());


	} while (protos_ast.size());

	return retn;
}