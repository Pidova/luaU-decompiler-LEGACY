#include <algorithm>
#include "ast_dec.hpp"
#include "post_ast.hpp"
#include "../emitter/emitter.hpp"

#define node_nonmutable(node) node->has_expr(ast_dec::expr_type::condition_nonmutable)

#define routine_inc(node, routine) routine += node->count_expr <ast_dec::expr_type::concat_routine_start>() + node->count_expr <ast_dec::expr_type::call_routine_start>() + node->count_expr <ast_dec::expr_type::table_start>() + node->count_expr <ast_dec::expr_type::condition_concat_start>()
#define routine_dec(node, routine) routine -= node->count_expr <ast_dec::expr_type::concat_routine_end>() + node->count_expr <ast_dec::expr_type::call_routine_end>() + node->count_expr <ast_dec::expr_type::table_end>() + node->count_expr <ast_dec::expr_type::condition_concat_end>()

/* Same thing as routines(inc/dec) but conditional concat routines gets ignored because they don't garunteed a locvar. */
#define routine_inc_lv(node, routine) routine += node->count_expr <ast_dec::expr_type::concat_routine_start>() + node->count_expr <ast_dec::expr_type::call_routine_start>() + node->count_expr <ast_dec::expr_type::table_start>() 
#define routine_dec_lv(node, routine) routine -= node->count_expr <ast_dec::expr_type::concat_routine_end>() + node->count_expr <ast_dec::expr_type::call_routine_end>() + node->count_expr <ast_dec::expr_type::table_end>() 


namespace ast_funcs {

	namespace regs {

		/* Sees if destination is logical in scope. */
		bool logical_dest_register(std::shared_ptr<ast_dec::ast>& ast, std::shared_ptr<ast_dec::node> start, const std::int16_t target) {

			/* Check for loop too see if they override any of the regs. */
			const auto next = ast->main_block->visit_rest_scope_addr(start->address);
			for (const auto& i : next) {

				/* See if register falls within loop vars. */
				if (i->has_expr(ast_dec::expr_type::for_start) || i->has_expr(ast_dec::expr_type::for_iv_start) || i->has_expr(ast_dec::expr_type::for_n_start)) {
					const auto for_target = i->loop_extra.end_node;
					if (for_target->loop_extra.start_reg <= target && for_target->loop_extra.end_reg >= target) {
						return false;
					}
				}

			}


			/* See if register gets used twice by dest or source with repecting scopes and either or reseting it. */
			std::uint32_t routine = 0u;
			std::int32_t scope = 0u;
			const auto target_1 = target;
			bool used_target_1_dest = true;
			bool used_target_1_source = false;
			bool no_locvar = true;
			std::shared_ptr<ast_dec::node> dest_node = start;
			std::shared_ptr<ast_dec::node> dest_node_nm = start; /* Dest node non mutable by sources. */

			/* Really just visiting future instructions too see if table source, dest, or idx gets set twice indicating end. */
			const auto rest_nodes = ast->main_block->visit_rest(start->address);
			for (const auto& s_node : rest_nodes) {

				/* Checks operands if targets get used or not but if it does get used just resets target. */
				bool used_source_twice = false; /* Used by source twice. */
				bool used_source_routine = false; /* Used by source in routine. */
				std::function<void(const std::shared_ptr<LuaU_dissassembler::operand>&, const lexer_dec::operand_types)> check_usage = [&](const std::shared_ptr<LuaU_dissassembler::operand>& operand, const lexer_dec::operand_types tt) mutable {

					const auto val = operand->reg;

					if (val == target_1) {

						/* Used twice and last dest node is current. */
						if (used_target_1_source && dest_node == start) {
							used_source_twice = true;
							return;
						}

						/* Used in routine and last dest node is start. */
						if (routine && dest_node_nm == start) {
							used_source_routine = true;
							return;
						}

						used_target_1_source = true;
						used_target_1_dest = false;
						dest_node = nullptr;
					}

					return;
				};


				/* Scope */
				scope += (s_node->count_expr <ast_dec::expr_type::repeat_>() +
					s_node->count_expr <ast_dec::expr_type::while_>() +
					s_node->count_expr <ast_dec::expr_type::for_start>() +
					s_node->count_expr <ast_dec::expr_type::for_iv_start>() +
					s_node->count_expr <ast_dec::expr_type::for_n_start>() +
					s_node->count_expr <ast_dec::expr_type::if_>());
				scope -= s_node->count_expr <ast_dec::expr_type::scope_end>();

				/* Out of scope */
				if (scope < 0) {
					break;
				}

				/* End of scope */
				if (!scope && s_node->lex->dissassembly->op == LuauOpcode::LOP_RETURN) {

					/* Just ended with it just using it as dest. */
					if (used_target_1_dest && !used_target_1_source) {
						no_locvar = false;
						break;
					}

				}
				

				/* Inc for concat start, call start, and table start. Dec for concat end, call end, and start end. */
				routine_inc_lv(s_node, routine);
				routine_dec_lv(s_node, routine);

				/* Check reg operands for usage. */
				s_node->lex->operand_expr_callback<lexer_dec::operand_types::source>(check_usage);
				s_node->lex->operand_expr_callback<lexer_dec::operand_types::compare>(check_usage);
				s_node->lex->operand_expr_callback<lexer_dec::operand_types::reg>(check_usage);

				/* Used by source twice. */
				if (used_source_twice) {
					no_locvar = false;
					break;
				}

				/* Used in source in routine. */
				if (used_source_routine) {
					no_locvar = false;
					break;
				}

				if (s_node->lex->has_operand_expr<lexer_dec::operand_types::dest>()) {

					const auto dest = s_node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;

					/* Skip loadb with jump. */
					if (s_node->lex->dissassembly->op == LuauOpcode::LOP_LOADB && s_node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp) {
						continue;
					}
					
					/* Check target usage. Abrubt end. */
					if (dest == target_1) {
						
						if (routine) { /* Used in routine not locvar. */
							break;
						}

						/* Set twice without used. Locvar */
						if (used_target_1_dest) {
							no_locvar = (dest_node != start);
							break;
						}

						used_target_1_dest = true;
						used_target_1_source = false;
						dest_node = s_node;
						dest_node_nm = s_node;
					}

				}

			}


			/* Not locvar by routine. */
			if (no_locvar) {
				return false;
			}
		
			return true;
		}

		/* Sets statements based on register stack. */
		void set_statements(std::shared_ptr<ast_dec::ast>& ast) {

			const auto all = ast->main_block->visit_all();
			for (const auto& i : all) {

				if (i->lex->has_operand_expr<lexer_dec::operand_types::dest>()) {

				}

			}

		}

	}

	namespace upvalues {

		void set(const std::shared_ptr<ast_dec::ast>& ast) {
			
			/* Captures follow newclosure. */
			const auto closures = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_NEWCLOSURE>(true));
			for (const auto& node : closures) {

					/* Incase it bugs out. */
					node->closure_extra.closure_idx = node->lex->dissassembly->operands.back()->k_idx /* Will work for proto. */;

					auto idx = 0u;
					auto next = ast->main_block->visit_addr(node->address + node->lex->dissassembly->len);

					while (next != nullptr && next->lex->dissassembly->op == LuauOpcode::LOP_CAPTURE) {
						
						if (next->lex->has_operand_expr<lexer_dec::operand_types::upvalue>()) {
							/* Pass upvalue */
							ast->protos[node->closure_extra.closure_idx]->upvalues.insert(std::make_pair(idx++, std::make_pair(ast->upvalues[next->lex->operand_expr<lexer_dec::operand_types::upvalue>().front()->upvalue].first, -1)));
						}
						else {

							/* Pass variable */

							const auto reg = next->lex->dissassembly->operands.back()->reg;

							std::string name = "";
							emitter::locvar_name(name, ast->transpiler_config->upvalue_prefix, std::stoi(std::to_string(ast->ast_id) + std::to_string(reg)) /* Str -> int for formatting */, ast->transpiler_config->upvalue_suffix_char);

							ast->protos[node->closure_extra.closure_idx]->upvalues.insert(std::make_pair(idx++, std::make_pair(name, reg)));

							/* Change arg name if reg. */
							for (auto& arg : ast->arg_regs)
								if (arg.first == reg) {
									arg.second = name;
									goto next_L;
								}

							/* See if function */
							if (node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg == reg) {
								ast->protos[node->closure_extra.closure_idx]->closure_name = name;
								goto next_L;
							}

							/* See if var/sub or iterator/sub */
							const auto back = ast->main_block->visit_rest_curr_flip(node->address);
							for (const auto& node_ : back) {
							
								if (node_->dest_loc.is_dest_loc && node_->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg == reg) {
									node_->dest_loc.name = name;
									node_->dest_loc.is_upvalue = true;
									break;
								}
								else if (node_->has_expr(ast_dec::expr_type::for_iv_start) || node_->has_expr(ast_dec::expr_type::for_start) || node_->has_expr(ast_dec::expr_type::for_n_start)) {
									
									/* Check for loops */
									const auto for_target = node_->loop_extra.end_node;
							
									/* Not a jumpback */
									if (for_target->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp > 0)
										continue;
									
									/* Iterator */
									if (for_target->loop_extra.start_reg <= reg && for_target->loop_extra.end_reg >= reg) {
										node_->loop_extra.iteration_names.insert(std::make_pair(reg, name));
										goto next_L;
									}
									

									/* Sub iterator movs for == of capture reg */
									if (node_->sub_node != nullptr && node_->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg == reg) {
										node_->loop_extra.iteration_names.insert(std::make_pair(node_->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg, name));
										goto next_L;											
									}
	
								} else if (node_->sub_node != nullptr && node_->get_sub_node()->dest_loc.is_dest_loc && node_->get_sub_node()->lex->has_operand_expr<lexer_dec::operand_types::dest>() && node_->get_sub_node()->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg == reg) {
									node_->get_sub_node()->dest_loc.name = name;
									node_->get_sub_node()->dest_loc.is_upvalue = true;
									goto next_L;
								}
								
							}

						}
					
						next_L:
							next = ast->main_block->visit_addr(next->address + next->lex->dissassembly->len);
					}
					
			}


			return;
		}

	}

	namespace proto {

		void set_closure_info(const std::shared_ptr<ast_dec::ast>& current_proto, const std::size_t child_proto_id) {

			std::shared_ptr<ast_dec::node> closure_node = nullptr;

			/* Newcloure, Dupclosures node. */
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
						if (i->lex->dissassembly->operands[1]->proto == child_proto_id) {

							closure_node = i; /* Set node. */

							/* Skip captures if any. */
							auto next = current_proto->main_block->visit_addr(i->address + i->lex->dissassembly->len);
							while (next != nullptr && next->lex->dissassembly->op == LuauOpcode::LOP_CAPTURE) {
								next = current_proto->main_block->visit_addr(next->address + next->lex->dissassembly->len);
							}

							/* Next is null */
							if (next == nullptr) {
								closure_node->add_expr<ast_dec::expr_type::closure_local>(1u, ast_dec::element::front); /* local function ?? (??) [MUTABLE] */
							}
							else {

								/* Next is setglobal and uses reg as source. */
								if (next->lex->dissassembly->op == LuauOpcode::LOP_SETGLOBAL && next->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg == i->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg) {
									closure_node->add_expr<ast_dec::expr_type::closure_global>(1u, ast_dec::element::front);
									closure_node->closure_extra.setglobal_node = next;
									closure_node = next;
									closure_node->add_expr<ast_dec::expr_type::closure_global>(1u, ast_dec::element::front); /*  function ?? (??) */
								}
								else {
									closure_node->add_expr<ast_dec::expr_type::closure_local>(1u, ast_dec::element::front); /* local function ?? (??) [MUTABLE] */
								}

							}

							closure_node->closure_extra.closure_idx = child_proto_id;

						}

						break;
					}

					case LuauOpcode::LOP_DUPCLOSURE: {

						/* Get proto from dupclosure kvalue. */
						closure_node = i; /* Set node. */

						const auto next = current_proto->main_block->visit_addr(i->address + i->lex->dissassembly->len);
						if (next->lex->dissassembly->op == LuauOpcode::LOP_SETGLOBAL && next->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg == i->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg) {
							closure_node->add_expr<ast_dec::expr_type::closure_global>(1u, ast_dec::element::front);
							closure_node->closure_extra.setglobal_node = next;
							closure_node = next;
							closure_node->add_expr<ast_dec::expr_type::closure_global>(1u, ast_dec::element::front); /*  function ?? (??) */
						}
						else {
							closure_node->add_expr<ast_dec::expr_type::closure_local>(1u, ast_dec::element::front); /* local function ?? (??) [MUTABLE] */
						}

						closure_node->closure_extra.closure_idx = child_proto_id;

						break;
					}

					default: {
						throw std::runtime_error("Unkown opcode for closure_type.");
					}

				}

			}
			
			/* Turn expression to closure type. */

			auto proto = current_proto->protos[child_proto_id];


			/* Comes with closure name. */
			if (closure_node->has_expr(ast_dec::expr_type::closure_global)) {

				proto->closure_type = ast_dec::closure_type::global;
				proto->closure_name = closure_node->lex->operand_expr<lexer_dec::operand_types::kvalue>().front()->k_value;
				closure_node->add_expr<ast_dec::expr_type::dead_instruction>(); /* Handled by before hand. */

			}
			else if (closure_node->has_expr(ast_dec::expr_type::closure_local)) { /* Closure name doesn't get compiled unless specified. */
				
				proto->closure_type = ast_dec::closure_type::local;

			}
			else if (closure_node->has_expr(ast_dec::expr_type::closure_newclosure)) {/* No closure name. */

				proto->closure_type = ast_dec::closure_type::newclosure;
			
			}
			else {/* Shouldn't happen but incase it does. */
				throw std::runtime_error("Unkown expression for closure_type.");
			}


			return;
		}

	}

	namespace branches {

		/* Set branches conditons in routine with given range. */
		void set(std::shared_ptr<ast_dec::ast>& ast, const std::uintptr_t begin, const std::uintptr_t end, const std::vector <std::uintptr_t> dead /* Always opposite and when hit. */, const std::int16_t logical_operation_target = -1 /* Used for ignoring ands/ors. */) {

			const auto conditions = ast->main_block->visit_range_type<lexer_dec::inst_type::branch_condition>(begin, end);
			const auto over_target = *std::max_element(dead.begin(), dead.end());
			const auto range = ast->main_block->visit_range_current(begin, end);
			const auto loadbs = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_LOADB>(true));

			std::vector <std::pair <std::uintptr_t /* Jump */, std::size_t /* Scopes */>> scopes;

			const auto has_val = [&scopes](const std::uintptr_t addr) {
				return std::find_if(scopes.begin(), scopes.end(), [&addr](const std::pair<std::uintptr_t, std::size_t>& ele) { return ele.first == addr; }) != scopes.end();
			};


			for (const auto& node : range) {


				if (node->lex->type == lexer_dec::inst_type::branch_condition || node->lex->type == lexer_dec::inst_type::branch) {
					
					const auto jmp = node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr;

					/* Hit dead? */
					if (std::find(dead.begin(), dead.end(), jmp) != dead.end()) {
						
						/* Close any open scopes. */
						if (!scopes.empty() && scopes.back().second) {
							node->add_expr<ast_dec::expr_type::condition_close>(scopes.back().second);
						}

						/* Not the last branch. */
						if (conditions.back() != node) {
							node->add_expr<ast_dec::expr_type::condition_and>();
						}

						/* Always opposite */
						node->branch_extra.opposite = true;
					
					}
					else {

						/* Add some scope if there is none just too garunteed a scope. */
						if (!scopes.size()) {
							scopes.push_back(std::make_pair(jmp, 0u));
						}


						if (node->has_expr(ast_dec::expr_type::condition_emit_next)) {

							/* Condition emits compare to loadb. If there is no logical condition for variable exit thought if there is check writing register with dest register of loadb. */
							if (logical_operation_target == -1) {
								continue;
							}
							else if (logical_operation_target != -1 && ast->main_block->visit_addr(node->address + node->lex->dissassembly->len)->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg != logical_operation_target) {
								continue;
							}

						}

						/* Jmp exceeds dead values. */
						if (jmp > over_target) {

							if (has_val(node->address + node->lex->dissassembly->len)) {

							} 

							/* Last condition isnt current so or. */
							if (conditions.back() != node) {
								node->add_expr<ast_dec::expr_type::condition_or>();
							}
							else {
							
								/* Close any open scopes. */
								if (scopes.back().second) {
									node->add_expr<ast_dec::expr_type::condition_close>(scopes.back().second);
								}

							}
							
						}
						else {

							/* Next hit end scope. */
							if (scopes.back().first == (node->address + node->lex->dissassembly->len)) {

								if (scopes.back().second) {
									node->add_expr<ast_dec::expr_type::condition_close>(scopes.back().second);
								}

								scopes.pop_back();

								/* Add next and/or */
								if (!scopes.empty()) {
									
									if (scopes.back().first == jmp) {

									}

								}

							}
							else {

								/* New and sub scope */
								if (scopes.back().first != jmp && scopes.back().first > jmp) {
									node->add_expr<ast_dec::expr_type::condition_and>();
									node->add_expr<ast_dec::expr_type::condition_open_post>();
									scopes.push_back(std::make_pair(jmp, 1u /* Starts off with one could inc. */));
									node->branch_extra.opposite = true;
								}
								else {
									node->add_expr<ast_dec::expr_type::condition_or>();
								}

							}

						}

					}

				}

			}

			return;
		}

		/* Sets valid branch routines for a range with expr: "condition_routine" */
		void set_valid_branch_routine(std::shared_ptr<ast_dec::ast>& ast) {

			std::size_t idx = 0u;
			std::size_t routine = 0u;

			const auto conditions = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_type<lexer_dec::inst_type::branch_condition>(true));

			/* Nothing to analyze */
			if (conditions.empty()) {
				return;
			}

			auto condition = conditions[idx++]->lex->operand_expr<lexer_dec::operand_types::compare>();
			auto target_1 = condition.front()->reg;
			auto target_2 = (condition.size () > 1u) ? condition.back()->reg : -1;
			bool used_target_1 = false;
			bool used_target_2 = false;


			std::shared_ptr<ast_dec::node> start_node = nullptr;

			/* Checks operands if targets get used or not but if it does get used just resets target. */
			std::function<void(const std::shared_ptr<LuaU_dissassembler::operand>&, const lexer_dec::operand_types)> check_usage = [&](const std::shared_ptr<LuaU_dissassembler::operand>& operand, const lexer_dec::operand_types tt) mutable {

				const auto val = operand->reg;

				if (val == target_1) {
					used_target_1 = false;
					start_node = nullptr;
				}

				if (target_2 != -1 && signed(val) == target_2) {
					used_target_2 = false;
					start_node = nullptr;
				}

				return;
			};


			auto all = ast->main_block->visit_all();
			for (auto& node : all) {

				node->lex->operand_expr_callback<lexer_dec::operand_types::source>(check_usage);
				node->lex->operand_expr_callback<lexer_dec::operand_types::reg>(check_usage);


				/* Inc for concat start, call start, and table start. Dec for concat end, call end, and start end. */
				routine_inc(node, routine);
				routine_dec(node, routine);


				if (!routine && node->lex->has_operand_expr<lexer_dec::operand_types::dest>()) {

					const auto dest  = node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;	
				
					/* Check target usage. Abrubt end. */
					if (dest == target_1) {

						/* Set start node for buffer. */
						if (start_node == nullptr) {
							start_node = node;
						}

						/* Set twice without used. Abrubt end. */
						if (used_target_1) {
							used_target_1 = false;
							used_target_2 = false;
							start_node = nullptr;
							continue;
						}

						used_target_1 = true;
					}

					if (target_2 != -1 && signed(dest) == target_2) {

						/* Set start node for buffer. */
						if (start_node == nullptr) {
							start_node = node;
						}

						/* Set twice without used. Abrubt end. */
						if (used_target_2) {
							used_target_1 = false;
							used_target_2 = false;
							start_node = nullptr;
							continue;
						}

						used_target_2 = true;
					}

				}

				if (node->lex->type == lexer_dec::inst_type::branch_condition) {

					/* Start node is null so just use current node. */
					if (start_node == nullptr) {
						start_node = node;
					}
					
					/* Append condition routine expr. */
					start_node->add_expr<ast_dec::expr_type::condition_routine_start>();

					/* Check loadb */
					const auto next = ast->main_block->visit_addr(node->address + node->lex->dissassembly->len);
					if (next != nullptr && next->lex->dissassembly->op == LuauOpcode::LOP_LOADB) {

						const auto mem = next->lex->operand_expr<lexer_dec::operand_types::memaddr>().front();
						if (mem->jmp) {

							node->add_expr<ast_dec::expr_type::condition_emit_next>();
							node = ast->main_block->visit_addr(next->address + next->lex->dissassembly->len);	

							next->add_expr<ast_dec::expr_type::dead_instruction>(); /* Ignore instruction. */
							node->add_expr<ast_dec::expr_type::dead_instruction>(); /* Ignore instruction. */
						}

					}
					
					node->add_expr<ast_dec::expr_type::condition_routine_end>();
					
				

					const auto range_br = ast->main_block->visit_range(start_node->address, node->address);
					for (const auto& i : range_br) {
						i->add_expr<ast_dec::expr_type::condition_routine>();
					}

					/* Set next */
					if (conditions.size() != idx) {
						condition = conditions[idx++]->lex->operand_expr<lexer_dec::operand_types::compare>();
						target_1 = condition.front()->reg;
						target_2 = (condition.size() > 1u) ? condition.back()->reg : -1;
						used_target_1 = false;
						used_target_2 = false;
						start_node = nullptr;
					}

				}

			}

			return;
		}

		/* 
		
			* Sets ifs/elseifs/elses (Logical routines loops etc must be set first). 
			
			 Logical routines does all of the dirty work already if you want too know if it's if see if end is a compare.
		*/
		void set_branch_statements(std::shared_ptr<ast_dec::ast>& ast) {

			/* Unsafe too make compares as definite concat if those are really variables will get handled later. */

			const auto routines = std::get<std::vector<std::pair <std::shared_ptr<ast_dec::node>, std::shared_ptr<ast_dec::node>>>>(ast->main_block->visit_expr_routine<ast_dec::expr_type::condition_logical_start, ast_dec::expr_type::condition_logical_end>(true));
			for (const auto& i : routines) {

				if (i.second->lex->type == lexer_dec::inst_type::branch_condition) {

					/* If or elseif/else */
					const auto jmp_addr = i.second->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr;

					const auto concat_start = i.first;
					const auto concat_end = i.second;
					const auto routine_end = ast->main_block->visit_addr(jmp_addr);

					/* See if previous has jump if so else/elseif. */
					const auto prev = ast->main_block->visit_previous_addr(jmp_addr);
					if (prev->lex->dissassembly->op == LuauOpcode::LOP_JUMP /* Jump forward else */) {
					
						/* elseif/else */

					}
					else {

						/* If statement */

						concat_start->add_expr<ast_dec::expr_type::condition_concat_start>();
						concat_end->add_expr<ast_dec::expr_type::condition_concat_end>();
						concat_end->add_expr<ast_dec::expr_type::if_>();
						routine_end->add_expr<ast_dec::expr_type::scope_end>(1u, (concat_end->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr > 1u) ? ast_dec::element::front  : ast_dec::element::back /* Empty expression? */);

						/* See if jump is greater then 1. */
						ast_funcs::branches::set(ast, concat_start->address, concat_end->address, { jmp_addr });
						
					}

				}

			}

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

				   **Will also set sub nodes and dest nodes with sources**
		*/
		void set(std::shared_ptr<ast_dec::ast>& ast) {

			auto set_args = [&](const std::shared_ptr<ast_dec::ast>& proto, const bool dont_set = false /* Used for main moves. */) mutable {

				std::vector <std::vector <std::uint32_t>> dests; /* (Scoped) Registers used in dest. **getting written too** */
				std::vector <std::vector <std::shared_ptr<ast_dec::node>>> dests_nodes; /* Dest nodes relative to dests (Can't be pair as too this is used for something entirely different from arguments) */
				std::vector <std::uint32_t> source_no_dest; /* Registers used in source, value but not dest. **Not written too yet but been used** */
				std::vector <std::uintptr_t> addr_scopes; /* Scopes */

				/* Emblace first */
				dests.emplace_back(std::vector <std::uint32_t>({ }));
				dests_nodes.emplace_back(std::vector <std::shared_ptr<ast_dec::node>>({ }));

				/* Already been analyzed. */
				if (!proto->arg_regs.empty()) {
					return;
				}

				const auto all = proto->main_block->visit_all();
				for (const auto& node : all) {
					
					/* Fix scope */
					if (std::find(addr_scopes.begin(), addr_scopes.end(), node->address) != addr_scopes.end()) {

						for (auto i = 0u; i < std::count(addr_scopes.begin(), addr_scopes.end(), node->address); ++i) {
							dests.pop_back();
							dests_nodes.pop_back();
							addr_scopes.pop_back();
						}

					}
				
					/* Loops usally overwrite some regs per part of there routine append them to dest. */
					const auto for_node = node->loop_extra.end_node;
					if (for_node != nullptr) {
					
						/* Loop jumpback */
						addr_scopes.emplace_back(for_node->address);
						dests.emplace_back(dests.back());
						dests_nodes.emplace_back(dests_nodes.back());
						

						auto start_reg = 0u; /* for start */
						auto iteration = 0u; /* for regs being consumed for its operation */

						switch (for_node->lex->dissassembly->op) {

							case LuauOpcode::LOP_FORGLOOP: {
								start_reg = for_node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg;
								iteration = 4u;
								break;
							}
							case LuauOpcode::LOP_FORNLOOP: {
								start_reg = for_node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg;
								iteration = 2u;
								break;
							}

							 /* Could be loop prep??? maybe */
							default: {
								continue;
							}

						}

						/* See if theres prep for for loop then add if so. */
						auto for_dest = node;
						if (!for_dest->has_expr(ast_dec::expr_type::for_prep)) {

							const auto prev = ast->main_block->visit_previous_addr(for_dest->address);
							if (prev->has_expr(ast_dec::expr_type::for_prep))
								for_dest = prev;

						}
						
						/* Add vars from those loops. */
						for (auto i = start_reg; i < (start_reg + iteration + 1u); ++i)
							if (std::find(dests.back().begin(), dests.back().end(), i) == dests.back().end()) { /* Didnt find dest */
								dests.back().emplace_back(i);
								dests_nodes.back().emplace_back(for_dest);
							}
							else { /* Found dest, mutate dest node. */
								dests_nodes.back()[std::find(dests.back().begin(), dests.back().end(), i) - dests.back().begin()] = for_dest;
							}
						
					}

					switch (node->lex->dissassembly->op) {

						case LuauOpcode::LOP_RETURN: {

							const auto dest = node->lex->operand_expr<lexer_dec::operand_types::reg>().front()->reg;
							auto amt = node->lex->operand_expr<lexer_dec::operand_types::integer>().front()->val;

							if (amt) {

								if (amt == -1)
									amt = (*(&node - 1u))->lex->dissassembly->operands.front()->reg;

								for (auto a = dest; a < (dest + amt); ++a)
									if (std::find(dests.back().begin(), dests.back().end(), a) == dests.back().end()) {
										dests.back().emplace_back(a);
										dests_nodes.back().emplace_back(node);
									}
									else {
										node->dest_nodes.emplace_back(dests_nodes.back()[std::find(dests.back().begin(), dests.back().end(), a) - dests.back().begin()]);
									}

							}

							break;
						}

						case LuauOpcode::LOP_GETVARARGS: {

							const auto dest = node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;
							auto amt = node->lex->operand_expr<lexer_dec::operand_types::integer>().front()->val;

							if (amt == -1)
								amt = (*(&node - 1u))->lex->dissassembly->operands.front()->reg;

							if (amt == 0)
								amt = 1u;

							for (auto a = dest; a < (dest + amt); ++a)
								if (std::find(dests.back().begin(), dests.back().end(), a) == dests.back().end()) {
									dests.back().emplace_back(a);
									dests_nodes.back().emplace_back(node);
								}
								else {
									node->dest_nodes.emplace_back(dests_nodes.back()[std::find(dests.back().begin(), dests.back().end(), a) - dests.back().begin()]);
								}

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

								if (std::find(dests.back().begin(), dests.back().end(), arg) == dests.back().end() &&
									std::find(source_no_dest.begin(), source_no_dest.end(), arg) == source_no_dest.end()) {
									source_no_dest.emplace_back(arg);
								}

							}

							/* Add placement for call. */
							if (node->lex->has_operand_expr<lexer_dec::operand_types::dest>() && std::find(dests.back().begin(), dests.back().end(), start) == dests.back().end() &&
								std::find(source_no_dest.begin(), source_no_dest.end(), start) == source_no_dest.end()) {
								source_no_dest.emplace_back(start);
							}

							break;
						}

						default: {

							/* Append source. */
							if (node->lex->has_operand_expr<lexer_dec::operand_types::source>()) {

								const auto regz = node->lex->operand_expr<lexer_dec::operand_types::source>();

								for (const auto& operand : regz) {

									/* Append unused reg. */
									if (std::find(dests.back().begin(), dests.back().end(), operand->reg) == dests.back().end() && std::find(source_no_dest.begin(), source_no_dest.end(), operand->reg) == source_no_dest.end()) {
										source_no_dest.emplace_back(operand->reg);
									}
									else if (std::find(dests.back().begin(), dests.back().end(), operand->reg) != dests.back().end()) {
										node->source_nodes.emplace_back(dests_nodes.back()[std::find(dests.back().begin(), dests.back().end(), operand->reg) - dests.back().begin()]);
									}

								}

							}


							/* Append reg. */
							if (node->lex->has_operand_expr<lexer_dec::operand_types::reg>()) {

								const auto regz = node->lex->operand_expr<lexer_dec::operand_types::reg>();

								for (const auto& operand : regz) {

									/* Append unused reg. */
									if (std::find(dests.back().begin(), dests.back().end(), operand->reg) == dests.back().end() && std::find(source_no_dest.begin(), source_no_dest.end(), operand->reg) == source_no_dest.end()) {
										source_no_dest.emplace_back(operand->reg);
										
									}
									else if (std::find(dests.back().begin(), dests.back().end(), operand->reg) != dests.back().end()) {
										node->source_nodes.emplace_back(dests_nodes.back()[std::find(dests.back().begin(), dests.back().end(), operand->reg) - dests.back().begin()]);
									}


								}

							}


							/* Append dest. */
							if (node->lex->has_operand_expr<lexer_dec::operand_types::dest>()) {

								const auto reg = node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;

								if (std::find(dests.back().begin(), dests.back().end(), reg) == dests.back().end()) {

									dests.back().emplace_back(reg);
									dests_nodes.back().emplace_back(node);

									/* Append this. */
									if (node->lex->dissassembly->op == LuauOpcode::LOP_NAMECALL) {
										dests.back().emplace_back(reg + 1u);
										dests_nodes.back().emplace_back(node);
									}

								}
								else {
									node->dest_nodes.emplace_back(dests_nodes.back()[std::find(dests.back().begin(), dests.back().end(), reg) - dests.back().begin()]);
								}

							}

							/* Add for move */
							if (node->lex->dissassembly->op == LuauOpcode::LOP_MOVE) {

								const auto reg = node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg;
								if (std::find(dests.back().begin(), dests.back().end(), reg) != dests.back().end()) {
									node->sub_node = dests_nodes.back()[std::find(dests.back().begin(), dests.back().end(), reg) - dests.back().begin()];
								}

							}

							break;
						}

					}
				
					/* Append scope */
					if (node->lex->type == lexer_dec::inst_type::branch_condition) {

						/* Scope isnt jumpback? */
						const auto jmp_addr = node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr;
						if (jmp_addr > node->address) {
							addr_scopes.emplace_back(jmp_addr);
							dests.emplace_back(dests.back());
							dests_nodes.emplace_back(dests_nodes.back());
						}

					}
				
				}
			
				/* Check source_no_dest based off of call/table/concat routines least dest. */
			    std::int16_t reg = -1;
				for (const auto& node : all) {
					
					/* Nothing has been set in child proto so check manually with instruction. */
					switch (node->lex->dissassembly->op) {

						case LuauOpcode::LOP_CALL: {

							const auto dest = node->lex->dissassembly->operands.front()->reg;
							if (reg == -1 || dest < reg) {
								reg = dest;
							}
							
							break;
						}

						case LuauOpcode::LOP_CONCAT: {

							const auto dest = node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg;
							if (reg == -1 || dest < reg) {
								reg = dest;
							}
							
							break;
						}


						default: {
							break;
						}

					}
				
				}
		
				/* Append reg if found. */
				if (reg > 0 && std::find(source_no_dest.begin(), source_no_dest.end(), reg - 1) == source_no_dest.end()) {	
					source_no_dest.emplace_back(reg - 1);
				}
			
				/* No dests */
				if (source_no_dest.empty()) {
					return;
				}
			
				/* Remove dupes */
				std::sort(source_no_dest.begin(), source_no_dest.end());
				source_no_dest.erase(std::unique(source_no_dest.begin(), source_no_dest.end()), source_no_dest.end());

				/* Used for main skips arguments set. */
				if (dont_set) {
					return;
				}
		

				const auto max = *std::max_element(source_no_dest.begin(), source_no_dest.end());
				const auto min = 0;
				
				/* Set args (fill stack) */
				for (auto reg = min; reg <= max; reg++)
					proto->arg_regs.emplace_back(std::make_pair(reg, emitter::create::locvar_name(ast->transpiler_config->arg_prefix, reg, ast->transpiler_config->arg_suffix_char)));
				

				if (ast->main_block->has_next_inst<LuauOpcode::LOP_GETVARARGS>(0u)) {
					proto->arg_regs.emplace_back(std::make_pair(-1, "..."));
				}

				return;
			};


			/* Set moves and sources for main. */
			if (ast->closure_type == ast_dec::closure_type::main) {
				set_args(ast, true);
			}
			
			/* Everything needs to go through based on control flow. */
			for (const auto& proto : ast->protos) {
				set_args(proto);
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
				ast->main_block->visit_previous_dest_register(node->address, node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg)->add_expr<ast_dec::expr_type::concat_routine_start>(); /* Concat start. */
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

				const auto prev = ast->main_block->visit_previous_dest_register(node->address, node->lex->dissassembly->operands.front()->reg);

				/* Namecall gets special treatment. */
				if (prev->lex->dissassembly->op == LuauOpcode::LOP_NAMECALL) {
					// lexer_dec::operand_types::source

					const auto data = prev->lex->operand_expr< lexer_dec::operand_types::source>().front()->reg;
					const auto data_1 = prev->lex->operand_expr< lexer_dec::operand_types::integer>().front()->reg;
					
					/* Call first operand will be the same as previous. */
					if (data == data_1) {
						ast->main_block->visit_previous_dest_register(prev->address, node->lex->dissassembly->operands.front()->reg)->add_expr<ast_dec::expr_type::call_routine_start>(); /* Call start */
					}
					else {

						const auto args = node->lex->operand_expr< lexer_dec::operand_types::integer>().front()->val - 1u;

						if (args) {
							/* Set previous as call register + 2(1 is reserved, other is slot) */
							ast->main_block->visit_previous_dest_register(node->address, node->lex->dissassembly->operands.front()->reg + 2u)->add_expr<ast_dec::expr_type::call_routine_start>(); /* Call start */
						}
						else {
							/* No args namecall is end. */
							prev->add_expr<ast_dec::expr_type::call_routine_start>(); /* Call start */
						}

					}

				}
				else {
					prev->add_expr<ast_dec::expr_type::call_routine_start>(); /* Call start */
				}

				node->add_expr<ast_dec::expr_type::call_routine_end>(); /* Call end. */
			}

			return;
		}

	}

	namespace loops {

		/* Sorts loops based on ends. */
		void sort_loops(std::shared_ptr<ast_dec::ast>& ast) {

			auto labels = ast->main_block->visit_all_goto();
			std::sort(labels.begin(), labels.end(), [](std::pair<std::uintptr_t, std::vector<std::shared_ptr<ast_dec::node>>>& a, std::pair<std::uintptr_t, std::vector<std::shared_ptr<ast_dec::node>>>& b) { return a.first < b.first; });

			const auto all = ast->main_block->visit_all();
			for (const auto& node : all) {

				auto node_found = std::find_if(labels.begin(), labels.end(), [&](const std::pair<std::uintptr_t, std::vector<std::shared_ptr<ast_dec::node>>>& pair) { return pair.first == node->address; });

				/* Has goto */
				if (labels.size() && node_found != labels.end()) {

					
					std::vector <std::pair <ast_dec::expr_type, std::size_t>> exprs;

					if (labels.size() == 1u)
						continue;

					/* Collaprse exprs */
					node->collapse_expr();

					/* Reverse */
					std::reverse(node_found->second.begin(), node_found->second.end());

					for (const auto& label : node_found->second) {

						if (label->has_expr(ast_dec::expr_type::while_end)) {

							exprs.emplace_back(node->get_expr<ast_dec::expr_type::while_>());
							node->remove_expr<ast_dec::expr_type::while_>();

						} else if (label->has_expr(ast_dec::expr_type::until_)) {
					
							exprs.emplace_back(node->get_expr<ast_dec::expr_type::repeat_>());
							node->remove_expr<ast_dec::expr_type::repeat_>();

						} else if (label->has_expr(ast_dec::expr_type::for_end)) {

							exprs.emplace_back(node->get_expr<ast_dec::expr_type::for_start>());
							node->remove_expr<ast_dec::expr_type::for_start>();

						} else if (label->has_expr(ast_dec::expr_type::for_n_end)) {

							exprs.emplace_back(node->get_expr<ast_dec::expr_type::for_n_start>());
							node->remove_expr<ast_dec::expr_type::for_n_start>();

						} else if (label->has_expr(ast_dec::expr_type::for_iv_end)) {

							exprs.emplace_back(node->get_expr<ast_dec::expr_type::for_iv_start>());
							node->remove_expr<ast_dec::expr_type::for_iv_start>();

						} 


					}

					/* Append sorted exprs */
					for (const auto& expr : exprs)
						node->add_expr_tt(expr.first, expr.second);

				}

			}

			return;
		}

		/* Sets forprep instructions exprs. */
		void set_for_prep_exprs(std::shared_ptr<ast_dec::ast>& ast) {

			const auto all = ast->main_block->visit_all();
			for (const auto& i : all) {

				switch (i->lex->dissassembly->op) {

					case LuauOpcode::LOP_FORNPREP: {
						i->add_expr<ast_dec::expr_type::for_prep>();
						break;
					}

					case LuauOpcode::LOP_FORGPREP: {
						i->add_expr<ast_dec::expr_type::for_prep>();
						break;
					}

					case LuauOpcode::LOP_FORGPREP_NEXT: {
						i->add_expr<ast_dec::expr_type::for_prep>();
						break;
					}

					case LuauOpcode::LOP_FORGPREP_INEXT: {
						i->add_expr<ast_dec::expr_type::for_prep>();
						break;
					}

					default: {
						break;
					}

				}

			}

			return;
		}

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
				if (jump_inst == LuauOpcode::LOP_FORGPREP_INEXT || jump_inst == LuauOpcode::LOP_FORGPREP_NEXT) {
					jump_node->add_expr<ast_dec::expr_type::for_iv_start>();
					forloop->add_expr<ast_dec::expr_type::for_iv_end>();
				}
				else {
					jump_node->add_expr<ast_dec::expr_type::for_start>();
					forloop->add_expr<ast_dec::expr_type::for_end>();
				}

				forloop->add_expr<ast_dec::expr_type::scope_end>();

				jump_node->loop_extra.end_node = forloop;
				forloop->loop_extra.end_node = forloop;

				/* Set start/end registers */
				forloop->loop_extra.start_reg = forloop->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg;
				forloop->loop_extra.end_reg = forloop->loop_extra.start_reg + 4u;

			}

			/* Fornloops. */
			for (const auto& forloop : fornloops) {

				const auto loop = ast->main_block->visit_addr(forloop->lex->dissassembly->operands[1]->jmp_addr);
				loop->add_expr<ast_dec::expr_type::for_n_start>();
				forloop->add_expr<ast_dec::expr_type::scope_end>();
				forloop->add_expr<ast_dec::expr_type::for_n_end>();

				loop->loop_extra.end_node = forloop;
				forloop->loop_extra.end_node = forloop;

				/* Set start/end registers */
				forloop->loop_extra.start_reg = forloop->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg;
				forloop->loop_extra.end_reg = forloop->loop_extra.start_reg + 2u;

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
					jmp_back->add_expr<ast_dec::expr_type::while_end>(); /* While loop end. */
					nodes.emplace_back(jmp_back);
				}

			}

			/* See if jump memaddrs are negatives usally means there until. */
			for (const auto& jmp_back : jump_conds) 
				if (jmp_back->lex->dissassembly->operands[std::find(jmp_back->lex->operands.begin(), jmp_back->lex->operands.end(), lexer_dec::operand_types::memaddr) - jmp_back->lex->operands.begin()]->jmp < 0) { /* See if mem address of jump is negative (We need to get idx of memaddr operand). */
					jmp_back->add_expr<ast_dec::expr_type::until_>(); /* Until end. */
					nodes.emplace_back(jmp_back);
				}


			/* We need to get range of until/while routines conditions. */
			for (const auto& jumpback : nodes) {

				std::shared_ptr<ast_dec::node> marked_node = nullptr;

				/* See if jumpback is a branch. */
				if (jumpback->lex->type != lexer_dec::inst_type::branch_condition && jumpback->lex->type != lexer_dec::inst_type::branch)
					throw std::runtime_error("until/while branch jumpback isn't a branch.");

				const auto jmp_addr = jumpback->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr;
				const auto jmp_node = ast->main_block->visit_addr(jmp_addr);
				const auto nodes_routine = ast->main_block->visit_range_current(jmp_addr, jumpback->address);
			    auto condition = ast->main_block->visit_expr_routine_range_touching<ast_dec::expr_type::condition_routine_start, ast_dec::expr_type::condition_routine_end>(jmp_addr, jumpback->address);

				if (jumpback->has_expr(ast_dec::expr_type::until_)) {
					
					jmp_node->add_expr<ast_dec::expr_type::repeat_>();

					if (condition.empty()) {
						/* No conditions in it (Garunteed repeat (true) do) */
						jumpback->add_expr<ast_dec::expr_type::condition_true>(1u, ast_dec::element::front);
					}
					else {

						const auto cond = condition.back();

						if (cond.second->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr > jumpback->address) {

							/* Not analyzed */
							if (!cond.first->has_expr(ast_dec::expr_type::condition_concat_start)) {

								cond.first->add_expr<ast_dec::expr_type::condition_concat_start>();
								cond.second->add_expr<ast_dec::expr_type::condition_concat_end>();

								branches::set(ast, cond.first->address, cond.second->address, { jumpback->address });
							}

						}
						else {
							/* Last conditon doesn't jump out possible repeat until(true)*/
							jumpback->add_expr<ast_dec::expr_type::condition_true>(1u, ast_dec::element::front);
						}

					}				

				}
				else { /* While loop */

					/* No conditions in it (Garunteed repeat (true) do) */
					if (!condition.size()) {
						jmp_node->add_expr<ast_dec::expr_type::repeat_>();
						jumpback->add_expr<ast_dec::expr_type::condition_true>(1u, ast_dec::element::front);
					}
					else {

						const auto cond = condition.front();

						if (cond.second->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr > jumpback->address) {
							
							bool dont_set = false;
							std::shared_ptr<ast_dec::node> begin_node = cond.first;

							/* Go through second and first range and see regs logicals. */
							const auto range = ast->main_block->visit_range_current(cond.first->address, cond.second->address);
							for (const auto& node : range) {
								
								auto nearest_compare = std::get<std::shared_ptr<ast_dec::node>>(ast->main_block->visit_next_type_addr<lexer_dec::inst_type::branch_condition>(node->address, false));
								
								/* Check dest for compare value. */
								if (node->lex->has_operand_expr<lexer_dec::operand_types::dest>()) {

									const auto dest = node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;
									const auto compare = nearest_compare->lex->operand_expr<lexer_dec::operand_types::compare>();
										
									/* Hit reg set of compare. */
									if ((compare.size() == 1u && compare.front()->reg == dest) || (compare.size() == 2u && (compare.front()->reg == dest || compare.back()->reg == dest))) {
										
										const auto logical = ast_funcs::regs::logical_dest_register(ast, node, dest);
										
										if (!logical && begin_node == nullptr) {
											begin_node = node;
										}
										else if (logical) {
											
											/* Last codition and last is nearest with it being logical so while (true) */
											if (cond.second == nearest_compare) {
												jmp_node->add_expr<ast_dec::expr_type::while_>();
												jmp_node->add_expr<ast_dec::expr_type::condition_true>(1u, ast_dec::element::front);
												dont_set = true;
												break;
											}
											
											begin_node = nullptr;
										}
										

									}

								}

							}

							/* Write condition routine. */
							if (!dont_set) {
								begin_node->add_expr<ast_dec::expr_type::condition_concat_start>();
								cond.second->add_expr<ast_dec::expr_type::condition_concat_end>();
								cond.second->add_expr<ast_dec::expr_type::while_>();
								branches::set(ast, begin_node->address, cond.second->address, { jumpback->address, (jumpback->address + jumpback->lex->dissassembly->len)});
							}
							
						}
						else {
							/* Last conditon doesn't jump out possible repeat until(true)*/
							jmp_node->add_expr<ast_dec::expr_type::repeat_>();
							jumpback->add_expr<ast_dec::expr_type::condition_true>(1u, ast_dec::element::front);
						}

					}

				}

				/* Add breaks by getting second condition that doesn't have concat and marking. */
				for (const auto& c : condition) {

					if (!c.second->has_expr(ast_dec::expr_type::condition_concat_end) && c.second->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr > jumpback->address) {
						c.second->add_expr<ast_dec::expr_type::condition_concat_start>();
						c.second->add_expr<ast_dec::expr_type::condition_break>();
					}

				}

				/* End of routine is given with until. Find begging. */
				for (const auto& node : nodes_routine) {

					/* Break **Not definite jump to end of until routine or end of while can mean break of any conditional** */
					if (node->lex->dissassembly->op == LuauOpcode::LOP_JUMP || node->lex->dissassembly->op == LuauOpcode::LOP_JUMPX) {

						if (node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr > jumpback->address) {
							node->add_expr<ast_dec::expr_type::break_>();
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
					continue;


				std::uint16_t reg = 0u; /* Previous register dest. */
				std::uint32_t nested_count = 0u; /* Used for node analysis. */
				std::uintptr_t node_size = 0u;
				std::uintptr_t array_size = 0u; 
				std::uintptr_t predicted_size = 0u; /* Previous power of 2 for array_size, gives us more context on end. */


				/* Sizes for scopes of table. (Used for scopes of nested tables) */
				std::vector<std::uintptr_t> predicted_sizes;
				std::vector<std::uintptr_t> node_sizes;
				std::vector<std::uintptr_t> array_sizes;



				/* Cache data for scopes. */
				auto cache = [&](const bool start /* Start new cache or set old cache? */) mutable -> void {

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
						if (!predicted_sizes.empty())
							predicted_size = predicted_sizes.back();

						if (!node_sizes.empty())
							node_size = node_sizes.back();

						if (!array_sizes.empty())
							array_size = array_sizes.back();

					}

				};

				/* Set size for node_size and array_size also sets table elements/start/end. */
				auto set_size = [&](const std::shared_ptr<ast_dec::node>& node) mutable -> void {

					switch (node->lex->dissassembly->op) {

						case LuauOpcode::LOP_NEWTABLE: {
							
							const auto operands = node->lex->operand_expr<lexer_dec::operand_types::integer>();
							auto x = operands.front()->table_size;

							if (!x && !operands.back()->val) {
								node->add_expr<ast_dec::expr_type::table_start>();
								node->add_expr<ast_dec::expr_type::table_end>();
								return;
							}


							node_size = x;
							array_size = operands.back()->val;
				

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

							auto amt = node->lex->dissassembly->operands[2]->val;
							if (amt == -1)
								amt = reg;
							
							array_size -= std::uintptr_t (amt);
							
							if (predicted_sizes.size () > 1u && !node_size && !array_size) {		
								node->add_expr<ast_dec::expr_type::table_end>();
								ends.emplace_back(node->address);
							}

							cache(false);

							break;
						}

						default: {
							break;
						}

					}

					return;
				};
				

		
				/* Add first info */
				if (table->lex->dissassembly->op == LuauOpcode::LOP_NEWTABLE || table->lex->dissassembly->op == LuauOpcode::LOP_DUPTABLE) {
					
					/* Has table members? */
					set_size(table);
					
					if (array_size || node_size) {

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


									/* Really just visiting future instructions too see if table source, dest, or idx gets set twice indicating end. */
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

										/* New table instruction inc for nested. */
										if (i->lex->dissassembly->op == LuauOpcode::LOP_DUPTABLE || i->lex->dissassembly->op == LuauOpcode::LOP_NEWTABLE)
											++nested_count;

										if (i->lex->has_operand_expr<lexer_dec::operand_types::dest>()) {

											const auto dest = i->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;

											/* Check target usage. Abrubt end. */
											if (dest == target_1) {

												/* Set twice without used. Abrubt end. */
												if (used_target_1) {
													goto node_end;
												}

												used_target_1 = true;
											}

											if (target_2 != -1 && signed(dest) == target_2) {

												/* Set twice without used. Abrubt end. */
												if (used_target_2) {
													goto node_end;
												}

												used_target_2 = true;
											}

										}
										else {
											
											if (i->lex->type == lexer_dec::inst_type::table_set) {

												/* Table has same table source as current table source valid element.*/
												if (table_reg == i->lex->operand_expr<lexer_dec::operand_types::reg>().front()->reg) {
													--node_size;
												}
												else { /* New table */

													--node_size; /* Inc for node. */
													node->add_expr<ast_dec::expr_type::table_end>(); /* End of table. */
													cache(false); /* Revert back cache. */

													/* End of a nested table. */
													if (nested_count) {
														--nested_count;
													}
													else {  /* Nested table is 0 and we entered a new table. */
														break;
													}

													/* Target mets then its okay. */
													if (used_target_1 && ((target_2 != -1 && used_target_2) || target_2 == -1)) {
														break;
													}
	
													break;
												}

											}
											else {/* check for new table */
												goto node_end;
											}
											
										}

									}

								}

							}

							/* Found end, end anlysis. */
							if (!node_size && !array_size) {
							node_end:
								node->add_expr<ast_dec::expr_type::table_end>();
								ends.emplace_back(node->address);
								break;
							}

						}

					}
					else {
						/* No table members. */
						ends.emplace_back(table->address);
					}

				}
				else {
					throw std::runtime_error("Expected NEWTABLE or DUPTABLE instruction to init table.");
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
			std::uintptr_t routine = 0u; /* Inside concat, call, table routine, inc for start, dec for end. */

			for (const auto& node : ast->main_block->visit_all()) {


				/* Turns node into variable. */
				auto node_var = [&](const std::shared_ptr<LuaU_dissassembler::operand>& operand) -> void {
				
					/* Not set yet. */
					if (operand->reg == registers.back()) {
						node->dest_loc.is_dest_loc = true;
						node->dest_loc.name = emitter::create::locvar_name(ast->transpiler_config->variable_prefix, registers.back(), ast->transpiler_config->var_suffix_char) ;
						++registers.back();
					}

					return;
				};
	

				/* End of scope. */
				for (auto i = 0u; i < node->count_expr <ast_dec::expr_type::scope_end>(); ++i) {
					registers.pop_back();
				}

				/* Log reg scope start. */
				if (node->has_expr(ast_dec::expr_type::for_start) || node->has_expr(ast_dec::expr_type::for_n_start) || node->has_expr(ast_dec::expr_type::for_iv_start)) {
					registers.emplace_back(node->loop_extra.end_node->loop_extra.end_reg + 1u);
				}
				else if (node->has_expr(ast_dec::expr_type::if_) || node->has_expr(ast_dec::expr_type::repeat_) || node->has_expr(ast_dec::expr_type::while_)) {
					registers.emplace_back(registers.back());
				}
				else if (node->has_expr(ast_dec::expr_type::elseif_) || node->has_expr(ast_dec::expr_type::else_)) {
					registers.back() = (*(registers.end() - 1));
				}
					 

				/* Inc for concat start, call start, and table start. Dec for concat end, call end, and start end. */
				routine_inc(node, routine);
				routine_dec(node, routine);


				/* Mutate local closure to newclosure. */
				if (node->has_expr(ast_dec::expr_type::closure_local) && routine) {
					node->replace_next<ast_dec::expr_type::closure_local, ast_dec::expr_type::closure_newclosure>();
					ast->protos[node->closure_extra.closure_idx]->closure_type = ast_dec::closure_type::newclosure;
				}
				

				/* Not inside routine and dest. */
				if (!routine && node->lex->has_operand_expr<lexer_dec::operand_types::dest>()) {
				
					auto bad = false; /* Failed any checks. (Can also be used if node is already set. */
					const auto dest = node->lex->operand_expr<lexer_dec::operand_types::dest>().front ();

					/* Fix name */
					if (node->has_expr(ast_dec::expr_type::closure_local)) {
						emitter::override::locvar_name(ast->protos[node->closure_extra.closure_idx]->closure_name, ast->transpiler_config->function_prefix, registers.back(), ast->transpiler_config->function_suffix_char);
					}
				
					/* Capture with source garunteeds locvar so check there. */
					const auto captures = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst_scope<LuauOpcode::LOP_CAPTURE>(node->address, true));
					for (const auto& capture : captures)
						if (capture->lex->has_operand_expr<lexer_dec::operand_types::source>() && capture->lex->operand_expr<lexer_dec::operand_types::source>().front ()->capture_reg == registers.back()) {
							node_var(dest);
							bad = true;
							break;
						}
					if (bad)
						continue;
					 
					
					/* Check concat and call routines if the dest is used as a dest in them no locvar. */
					const auto calls = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_next_expr_scope<ast_dec::expr_type::call_routine_start>(node->address, true));
					for (const auto& call : calls) {

						const auto node_end = ast->main_block->visit_relative_next_expr_scope<ast_dec::expr_type::call_routine_end>(call->address, { ast_dec::expr_type::call_routine_start });
					
						/* Target dest reg used in call routine dest. */
						for (const auto& call_node : ast->main_block->visit_range(call->address, node_end->address))
							if (call_node->lex->has_operand_expr<lexer_dec::operand_types::dest>() && call_node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg == registers.back())
								bad = true;
					
					}
					if (bad)
						continue;
				
					/* Concat */
					const auto concats = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_next_expr_scope<ast_dec::expr_type::concat_routine_start>(node->address, true));
					for (const auto& concat : concats) {
						
						const auto node_end = ast->main_block->visit_relative_next_expr_scope<ast_dec::expr_type::concat_routine_end>(concat->address, { ast_dec::expr_type::concat_routine_start });
					
						/* Target dest reg used in call routine dest. */
						for (const auto& concat_node : ast->main_block->visit_range(concat->address, node_end->address))
							if (concat_node->lex->has_operand_expr<lexer_dec::operand_types::dest>() && concat_node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg == registers.back())
								bad = true;

					}
					if (bad)
						continue;
					
					/* 
					
						After checking concat and call routines which can garunteed a variable this is where we mostly look at hueristics and decide if
						a register will become a variable or not. Either one it's set too will effect the preformance of the decompiled code and accuracy.
					
					*/


					/* Garunteed */
					if (node->has_expr(ast_dec::expr_type::table_end)) {
						node_var(node->lex->dissassembly->operands.front());
						continue;
					}
				
					/* Mutate local closure to newclosure. */
					if (node->has_expr(ast_dec::expr_type::closure_local)) {
						
						if (dest->reg != registers.back()) {
							node->replace_next<ast_dec::expr_type::closure_local, ast_dec::expr_type::closure_newclosure>();
							ast->protos[node->closure_extra.closure_idx]->closure_type = ast_dec::closure_type::newclosure;
						}
						else {
							emitter::override::locvar_name(ast->protos[node->closure_extra.closure_idx]->closure_name, ast->transpiler_config->function_prefix, registers.back()++, ast->transpiler_config->function_suffix_char);
						}


					}
					else {
					
						if (dest->reg == registers.back()) {
						
		
							/* See if dest register is logical. */
							if (ast_funcs::regs::logical_dest_register(ast, node, registers.back())) {
								
								/* Mutate global */
								if (node->has_expr(ast_dec::expr_type::closure_global)) {

									node->replace_next<ast_dec::expr_type::closure_global, ast_dec::expr_type::closure_local>();
									node->remove_all_expr <ast_dec::expr_type::closure_global>();

									ast->protos[node->closure_extra.closure_idx]->closure_type = ast_dec::closure_type::local;
									
									node->closure_extra.setglobal_node->remove_all_expr <ast_dec::expr_type::dead_instruction>();
									emitter::override::locvar_name(ast->protos[node->closure_extra.closure_idx]->closure_name, ast->transpiler_config->function_prefix, registers.back()++, ast->transpiler_config->function_suffix_char);
								
								}
								else {
									node_var(node->lex->dissassembly->operands.front());
								}

							}

							continue;
						}

					}

				}
				else if (!routine && node->has_expr(ast_dec::expr_type::table_end)) { /* No routine and end of table garunteed locvar. */
					node_var(node->lex->dissassembly->operands.front());
				}

			}

			return;
		}

		/*
		
			Can be checked by 2 hueristics:

				1. Get biggest jump inside branch jump and so on if it leads to a loadb do rule 2 vise versa.

				2:
					* Only applys to compare with 2 source registers else check rule 1.
					* If jump, jumps too loadb with previous instruction from jump being loadb but not with a jump:
						* If previous loadb and current have the same register close expression
						* Find loadb that has a jump and cache register and from out jump taken see if it gets used without written too at the end.
						  If it does not get used first then close expression.
						* Current loadb register must follow next loab with having compare in it.
						
					* If jump, jumps too loadb with previous instruction from jump being loadb but with a jump:
						* Check if register end(tooken jump) is logical if it is variable else something else repeat above.
		
		*/
		/* Sets logical operations. (Can be used for if/elseif statements). */
		void set_logical_operations(std::shared_ptr<ast_dec::ast>& ast) {

			/* Max jump out of jump compare with not being break(pre). */
			auto jump_out = [&](const std::uintptr_t start, const std::uintptr_t end) mutable -> std::shared_ptr<ast_dec::node>  {

				/* See if jump inside jump jumps out. */
				std::shared_ptr<ast_dec::node> jump_out = nullptr;
				const auto range = ast->main_block->visit_range(start, end);
				for (const auto& node : range) {
					
					if (node->lex->type == lexer_dec::inst_type::branch_condition) {

						const auto jmp = node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr;

						if (jmp > end && (jump_out == nullptr || jmp > jump_out->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr)) {

							/* Check too see if no break */
							if (!node->has_expr(ast_dec::expr_type::break_)) {
								jump_out = node;
							}

						}

					}

				}
			
				return jump_out;
			};
			

			auto all = ast->main_block->visit_all();
			for (auto& i : all) {

				/* Loadb or either branch condition. */
				if (i->lex->dissassembly->op == LuauOpcode::LOP_LOADB || i->lex->type == lexer_dec::inst_type::branch_condition) {

					const auto cached_init = i;
					std::vector<std::uint16_t> compares; /* Singular loadb compare(jmp 1+; loadb r1 +1; loadb r1 0; ???) jumps log registers and see if it gets used in compare first. */
					std::vector<std::uint16_t> compares_double; /* Double compare(jmp 3+ loadb r1,+1; loadb r1 0; loadb r2, 0; ???) */

					/* Make sure it hasnt already been analyzed. */
					if (!i->has_expr(ast_dec::expr_type::condition_logical_start) && !i->has_expr(ast_dec::expr_type::condition_logical) && !i->has_expr(ast_dec::expr_type::condition_logical_end)) {

						do {
							
							/* Get next branch jump. */
							if (i->lex->dissassembly->op == LuauOpcode::LOP_LOADB) {
								i = ast->main_block->visit_addr(i->address + i->lex->dissassembly->len);
							}

							/* No compare routine */
							if (!i->has_expr(ast_dec::expr_type::condition_routine_start) && !i->has_expr(ast_dec::expr_type::condition_routine_end) && !i->has_expr(ast_dec::expr_type::condition_routine)) {
								break;
							}
						
							/* Get next compare jump if current isnt one. */
							if (i->lex->type != lexer_dec::inst_type::branch_condition) {
								i = std::get<std::shared_ptr<ast_dec::node>>(ast->main_block->visit_next_type_addr<lexer_dec::inst_type::branch_condition>(i->address, false));
							}
						
							/* Not a compare branch something went wrong. */
							if (i == nullptr || i->lex->type != lexer_dec::inst_type::branch_condition) {
								break;
							}

							auto current_jmp = i->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr;

							/* Get any jumps that jumps out of current jump and isnt a break. */
							std::shared_ptr<ast_dec::node> jump_out_n = nullptr;
							std::shared_ptr<ast_dec::node> jump_out_temp = nullptr;
							do {
								jump_out_n = jump_out_temp;
								jump_out_temp = jump_out(
									(jump_out_n == nullptr) ? i->address : jump_out_n->address,
									(jump_out_n == nullptr) ? current_jmp : jump_out_n->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr
								);
							} while (jump_out_temp != nullptr);
						
				
							/* Found hueristic 1 check passthrough again. */
							if (jump_out_n != nullptr) {
								i = jump_out_n;
							}
							else {
							
								/* Found hueristic 2 or just end of normal branch */
								
								/* Check for compares with current branch. */
								const auto compare_count = i->lex->count_operand_expr<lexer_dec::operand_types::compare>();
								if (!compares.empty() || !compares_double.empty()) {

									const auto compare_operands = i->lex->operand_expr<lexer_dec::operand_types::compare>();

									if (compare_count == 1u && compares.size() == 1u) {

										/* Singular */
										if (!compares.empty() && compare_operands.front()->reg == compares.front()) {
											compares.clear(); /* Hit */
										}

										/* Double */
										if (!compare_operands.empty() && compare_operands.front()->reg == compares_double.front()) {
											compares_double.clear(); /* Hit */
										}

									}
									else if (compares.size() == 2u) { /* Max compare is 2 */

										std::vector<std::uint16_t> t_vect = { compare_operands.front()->reg, compare_operands.back()->reg };
									
										std::sort(t_vect.begin(), t_vect.end());
										std::sort(compares.begin(), compares.end());
										std::sort(compares_double.begin(), compares_double.end());

										/* Check for compares when sorted. */

										/* Singular */
										if (!compares.empty() && compares.front() == t_vect.front() && compares.back() == t_vect.back()) {
											compares.clear(); /* Hit */
										}

										/* Double */
										if (!compares_double.empty () && compares_double.front() == t_vect.front() && compares_double.back() == t_vect.back()) {
											compares_double.clear(); /* Hit */
										}

									}



								}

								/* Make sure compare has 2 compares. */
								if (compare_count != 2u) {
								
									const auto next = ast->main_block->visit_addr(i->address + i->lex->dissassembly->len);

									/* Nothing */
									if (next == nullptr) {
										break;
									}

									if (next->lex->dissassembly->op != LuauOpcode::LOP_LOADB) {

										/* Doesn't lead too loab handle it differently. */

									}
									else {					
										continue;
									}

								}

								const auto next = ast->main_block->visit_addr(i->address + i->lex->dissassembly->len);
								
								/* Nothing*/
								if (next == nullptr) {
									break;
								}
								
								/* Next has loadb with jump. */
								if (next->lex->dissassembly->op == LuauOpcode::LOP_LOADB && next->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp) {

									/* Singular */
									if (current_jmp == (next->address + next->lex->dissassembly->len)) {
										
										compares.push_back(next->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg);

										/* Exceeded max of 2 routine is something else. */
										if (compares.size() > 2u) {
											break;
										}

										/* Set next and contiue */
										i = ast->main_block->visit_addr(current_jmp + ast->main_block->visit_addr(current_jmp)->lex->dissassembly->len);

										continue;
									}

								}

								
								/* Previous from jump must be loadb. */
								const auto prev = ast->main_block->visit_previous_addr(current_jmp);
								if (prev->lex->dissassembly->op == LuauOpcode::LOP_LOADB) {

									/* Doesnt have jump check previous. */
									if (!prev->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp) {

										const auto p_prev = ast->main_block->visit_previous_addr(prev->address);

										/* Has previous loadb jump.  */
										if (p_prev->lex->dissassembly->op == LuauOpcode::LOP_LOADB && p_prev->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp) {
											
											compares_double.emplace_back(p_prev->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg);

											/* Exceeded max of 2 routine is something else. */
											if (compares_double.size() > 2u) {
												break;
											}

											/* Visit jump */
											i = ast->main_block->visit_addr(current_jmp);
										
											continue;
										}
										else {
											break;
										}

									}
									else {

										
									}

								}
								else {

									/* Analyze range of start current and jump. */
							
									auto next_inst = ast->main_block->visit_addr(i->address + i->lex->dissassembly->len);
									const auto next_jmp = std::get<std::shared_ptr<ast_dec::node>>(ast->main_block->visit_next_type_addr<lexer_dec::inst_type::branch_condition>(i->address, false));
									
									/* No next end. */
									if (next_jmp == nullptr || next_inst == nullptr) {
										break;
									}
									

									/* Jump data */
									const auto next_jmp_target = next_jmp->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr;
									const auto next_jmp_target_node = ast->main_block->visit_addr(next_jmp_target);
									

									/* Jump too same address possibly or? */
									if (current_jmp == next_jmp->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr) {

										/* No compare routine */
										if (!next_inst->has_expr(ast_dec::expr_type::condition_routine_start) && !next_inst->has_expr(ast_dec::expr_type::condition_routine_end) && !next_inst->has_expr(ast_dec::expr_type::condition_routine)) {
											break;
										}

										/* Has compare routine following next. */
										i = next_jmp;
										continue;
									}

									
									/* Next is loadb check also next. */
									if (next_inst->lex->dissassembly->op == LuauOpcode::LOP_LOADB) {
										next_inst = ast->main_block->visit_addr(next_inst->address + next_inst->lex->dissassembly->len);
									}
								
									
									/* Next leads too routine */
									if (next_inst->has_expr(ast_dec::expr_type::condition_routine_start) || next_inst->has_expr(ast_dec::expr_type::condition_routine_end) || next_inst->has_expr(ast_dec::expr_type::condition_routine)) {
										
										/* Check loadbs */

										const auto next = ast->main_block->visit_addr(i->address + i->lex->dissassembly->len);

										/* Nothing*/
										if (next == nullptr) {
											break;
										}

										/* Next has loadb with jump. */
										if (next->lex->dissassembly->op == LuauOpcode::LOP_LOADB && next->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp) {

											/* Singular */
											if (next_jmp_target == (next->address + next->lex->dissassembly->len)) {									
												/* Set next and contiue */
												i = next_jmp;
												continue;
											}

										}

										/* Take jump and check previouses */
										const auto prev = ast->main_block->visit_previous_addr(next_jmp_target);
										if (prev->lex->dissassembly->op == LuauOpcode::LOP_LOADB) {
										
											/* Doesnt have jump check previous. */
											if (!prev->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp) {

												const auto p_prev = ast->main_block->visit_previous_addr(prev->address);

												/* Has previous loadb jump.  */
												if (p_prev->lex->dissassembly->op == LuauOpcode::LOP_LOADB && p_prev->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp) {
													
													/* Check logical could be locvar. */
													if (ast_funcs::regs::logical_dest_register(ast, p_prev, p_prev->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg)) {
														break;
													}

													i = next_jmp;
													continue;
												}
												else {
													break;
												}

											}
											else {
									
												/* Target goes to loadb, same regs and jump. */
												if (next_jmp_target_node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg == prev->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg) {
													/* Set next and contiue */
													
													/* Check logical could be locvar. */
													if (ast_funcs::regs::logical_dest_register(ast, next_jmp_target_node, next_jmp_target_node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg)) {
														break;
													}

													i = next_jmp;
													continue;
												}

											}

										}

									}

									break; 
								}
								
							}

			
						} while (true);


						cached_init->add_expr<ast_dec::expr_type::condition_logical_start>();

						/* Members */
						const auto range = ast->main_block->visit_range(cached_init->address, i->address);
						for (const auto& i : range)
							i->add_expr<ast_dec::expr_type::condition_logical>();

						/* End */
						i->add_expr<ast_dec::expr_type::condition_logical_end>();	

					}

				}

			}

			return;
		}

	}

	void init_ast(std::shared_ptr<ast_dec::ast>& ast) {

		#if display_analysis
				std::printf("[AST] For prep exprs.\n");
		#endif
		ast_funcs::loops::set_for_prep_exprs(ast);

		#if display_analysis
				std::printf("[AST] Setting call routines.\n");
		#endif
		ast_funcs::calls::set_routines(ast);
		
		#if display_analysis
				std::printf("[AST] Setting concat routines.\n");
		#endif
		ast_funcs::concats::set_routines(ast);

		#if display_analysis
				std::printf("[AST] Setting table routines.\n");
		#endif
		ast_funcs::tables::set_routines(ast);

		#if display_analysis
				std::printf("[AST] Setting valid branch routines.\n");
		#endif
		ast_funcs::branches::set_valid_branch_routine(ast);

		#if display_analysis
				std::printf("[AST] Setting while/repeat routines.\n");
		#endif
		ast_funcs::loops::set_whilerep_routines(ast); /* Needed after concat can mess up if before or after. (expr_type::scope_end needed only for while end) */

		#if display_analysis
				std::printf("[AST] Setting for routines.\n");
		#endif
		ast_funcs::loops::set_for_routines(ast);

		#if display_analysis
				std::printf("[AST] Setting logical routines.\n");
		#endif
		ast_funcs::locvars::set_logical_operations(ast);

		#if display_analysis
				std::printf("[AST] Setting if/elseif/else routines.\n");
		#endif
		ast_funcs::branches::set_branch_statements(ast);

		#if display_analysis
				std::printf("[AST] Setting locvars.\n");
		#endif
		ast_funcs::locvars::set_lv(ast, ast->arg_regs.size());

		#if display_analysis
				std::printf("[AST] Setting upvalues.\n");
		#endif
		ast_funcs::upvalues::set(ast);

		#if display_analysis
				std::printf("[AST] Setting arguments for children proto.\n");
		#endif
		ast_funcs::arguments::set(ast);

		#if display_analysis
				std::printf("[AST] Sorting loops.\n");
		#endif
		ast_funcs::loops::sort_loops(ast);

		return;
	}

	void post_ast(std::shared_ptr<ast_dec::ast>& ast) {

		#if display_analysis
				std::printf("[POST-AST] Setting table node ends.\n");
		#endif
		ast_post::table::set_node_end(ast);

		#if display_analysis
				std::printf("[POST-AST] Setting table indexes exprs.\n");
		#endif
		ast_post::table::set_indexs(ast);

		#if display_analysis
				std::printf("[POST-AST] Setting arith exprs.\n");
		#endif
		ast_post::arith::set_arith_exprs(ast);

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
			if (std::binary_search(branch_ends.begin(), branch_ends.end(), pc)) {
				branch_ends.erase(std::remove(branch_ends.begin(), branch_ends.end(), pc), branch_ends.end());
				return std::make_tuple(retn, pc, pc);
			}

			/* Set current node. */
			auto node = std::make_shared<ast_dec::node>();
			retn.emplace_back(node);
			
			/* Set node data. */
			node->address = pc;
			node->lex = lexer_dec::lexer(current_dissassembly);
			
			#if display_dissassembly
				std::printf("[AST-dissassembly] %" PRIuPTR " %s ", pc, current_dissassembly->data.c_str());
				if (node->lex->type == lexer_dec::inst_type::branch || node->lex->type == lexer_dec::inst_type::branch_condition) {
					std::printf(" - %"  PRIuPTR "\n", node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr);
				}
				else {
					std::printf("\n");
				}
			#endif	
			
			/* Branch/end so break. */
			if (node->lex->type == lexer_dec::inst_type::branch || node->lex->type == lexer_dec::inst_type::branch_condition || (pc + current_dissassembly->len) == ast->p->sizecode)
				break;		

			pc += current_dissassembly->len;

		} while (true /* Earlier code will exit if hit a branch or passed branch ends. */);
		
		return std::make_tuple(retn, pc + ast->dissassembly[pc]->len, pc);
	}


	/* Set blocks for current ast. */
	void set_blocks(std::shared_ptr<ast_dec::ast>& ast) {

		std::uintptr_t pc = 0u;
		std::vector<std::uintptr_t> branch_ends;
		std::vector<std::uintptr_t> branch_ends_clone; /* Same as branch ends but a clone used for certain things. */
		std::unordered_map <std::uintptr_t /* PC(start) */, std::tuple <std::uintptr_t  /* PC(end) */, std::uintptr_t  /* PC(end(end + curr->len)) */, std::vector<std::shared_ptr<ast_dec::node>> /* Nodes */>> linear_blocks; /* Block data for scopes. */


		/* Set each end as every branch taken pc. */
		for (auto& dism : ast->dissassembly) {
			
			const auto temp_lex = lexer_dec::lexer(dism.second);

			/* Don't log if jumpback. */
			if (temp_lex->dissassembly->op == LuauOpcode::LOP_JUMPBACK) {
				continue;
			}

			if (temp_lex->type == lexer_dec::inst_type::branch || temp_lex->type == lexer_dec::inst_type::branch_condition) {

				/* Jump with no memaddr operand idk how this has happened. */
				if (!temp_lex->has_operand_expr<lexer_dec::operand_types::memaddr>()) {
					throw std::runtime_error("Jump with no memaddr operand in lexer at set_blocks.");
				}

				/* Jump is negative don't take. */
				if (temp_lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp < 0) {
					continue;
				}

				branch_ends.emplace_back(temp_lex->operand_expr<lexer_dec::operand_types::memaddr>().front ()->jmp_addr);
				
			}
		
		}

		/* Clone branch ends. */
		branch_ends_clone.reserve(branch_ends.size());
		std::copy(branch_ends.begin(), branch_ends.end(), branch_ends_clone.begin());


		/* Set main block. */
		ast->main_block = std::make_shared<ast_dec::block>();


		/* Append blocks */
		do {

			#if display_analysis_blocks
				std::printf("[AST] Begin block.\n");
			#endif

			const auto block = init_current_block(ast, pc, branch_ends_clone);
			linear_blocks.insert(std::make_pair(pc, std::make_tuple(std::get<2>(block), std::get<1>(block), std::get<0>(block))));
			pc = std::get<1>(block);

			#if display_analysis_blocks
				std::printf("[AST] End block.\n");
				std::printf("[AST] Next pc: %" PRIuPTR " size: %" PRIi32 "\n", pc, ast->p->sizecode);
			#endif

		} while (pc < ast->p->sizecode);

		
		/* Reset PC. */
		pc = 0u;

		/* Init main (Block search is dependent on main so needs to be seperate from everything). */
		ast->main_block->node_start = pc;
		ast->main_block->node_end = std::get<0>(linear_blocks[pc]);
		ast->main_block->nodes = std::get<2>(linear_blocks[pc]);
		
		std::vector<std::uintptr_t> analyzed_scopes; /* Used to prevent infinite loops when doing visits. */

		/* Assemble blocks */
		while (pc < ast->p->sizecode /* Pc didn't exceed sizecode. */) {

			const auto node_block = linear_blocks[pc];
			const auto nodes = std::get<2>(node_block);

	        /* Adbrupt end */
			if (!nodes.size()) {
				break;
			}
			
			analyzed_scopes.emplace_back(pc);
			
			/* End */
			const auto jump_node = nodes.back();
			if (jump_node->lex->type == lexer_dec::inst_type::branch || jump_node->lex->type == lexer_dec::inst_type::branch_condition) {
			
				const auto jmp = jump_node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr;
				const auto next_node_block = linear_blocks[jmp];
			
				auto current = ast->find_block(pc); /* Block too place all info in. */
				auto jump_block = ast->find_block(jmp); /* Jump taken block */
				auto nojump_block = ast->find_block(jump_node->address + jump_node->lex->dissassembly->len); /* Branch not taken jump. */
			
				/* Current is always available something bad happened. */
				if (current == nullptr) {
					
					/* Either something bad has happen or dead instruction. (Will never get executed no matter what branch is taken or not) */
					#if display_warnings
						std::printf("[WARNING] Dead instruction: %" PRIuPTR " %s\n", ast->dissassembly[pc]->addr, ast->dissassembly[pc]->data.c_str());
					#endif

					/* Get previous from pc */
					std::uintptr_t key = 0u;
					for (const auto& i : ast->dissassembly) {
						if (i.first < pc && key < i.first) {
							key = i.first;
						}
					}

					#if display_warnings
						std::printf("[WARNING] Appending dead instruction to block with: pc = %" PRIuPTR " at instruction : %s\n", key, ast->dissassembly[key]->data.c_str());
					#endif

					current = std::make_shared<ast_dec::block>();
					current->node_start = pc;
					current->node_end = pc;
					current->nodes = std::get<2>(linear_blocks[pc]);

					ast->find_block_addr(key)->branches.emplace_back(current);

					/* Propegate with bad instruction expr. */
					for (const auto& node : current->nodes) {
						node->add_expr<ast_dec::expr_type::bad_instruction>();
					}

				}


				switch (jump_node->lex->type) {

					case lexer_dec::inst_type::branch: {

						/* Construct branch taken. */
						if (jump_block == nullptr) {
							jump_block = std::make_shared<ast_dec::block>();
							jump_block->node_start = jmp;
							jump_block->node_end = std::get<0>(linear_blocks[jmp]);
							jump_block->nodes = std::get<2>(linear_blocks[jmp]);
						}

						/* Add jump taken. */
						if (jump_node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp > 0 || std::find(analyzed_scopes.begin(), analyzed_scopes.end(), jmp) == analyzed_scopes.end()) {
							current->branches.emplace_back(jump_block);
						}

						break;
					}

					case lexer_dec::inst_type::branch_condition: {

						/* Construct branch taken. */
						if (jump_block == nullptr) {
							jump_block = std::make_shared<ast_dec::block>();
							jump_block->node_start = jmp;
							jump_block->node_end = std::get<0>(linear_blocks[jmp]);
							jump_block->nodes = std::get<2>(linear_blocks[jmp]);
						}

						/* Construct no branch taken. */
						if (nojump_block == nullptr) {
							nojump_block = std::make_shared<ast_dec::block>();
							nojump_block->node_start = jump_node->address + jump_node->lex->dissassembly->len;
							nojump_block->node_end = std::get<0>(linear_blocks[jump_node->address + jump_node->lex->dissassembly->len]);
							nojump_block->nodes = std::get<2>(linear_blocks[jump_node->address + jump_node->lex->dissassembly->len]);
						}


						/* Add jump taken. */
						if (jump_node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp > 0 || std::find(analyzed_scopes.begin(), analyzed_scopes.end(), jmp) == analyzed_scopes.end()) {
							current->branches.emplace_back(jump_block);
						}
						current->branches.emplace_back(nojump_block);

						break;
					}

					default: {
						throw std::runtime_error("Unexpected jump inst type.");
					}

				}

			}

			/* Set pc */
			pc = std::get<1>(node_block);

		};

		return;
	}

}

std::shared_ptr<ast_dec::ast> ast_dec::gen_ast(Proto* proto, const std::shared_ptr<transpiler_data::transpiler_config>& config) {

	std::uintptr_t pc = 0u;

	/* Current ast. */
	auto retn = std::make_shared<ast>();
	std::vector<std::shared_ptr<ast>> protos_ast = { retn }; /* All protos including nested protos. */

	/* Set closure and proto. */
	retn->closure_type = closure_type::main;
	retn->p = proto;
	retn->transpiler_config = config;

	#if display_analysis
		std::printf("[AST] Initing main dissassembly.\n");
	#endif
	/* Set current proto dissasembly. */
	for (auto i = 0u; i < unsigned(proto->sizecode);) {
		auto dism = std::make_shared<LuaU_dissassembler::dissassembly>();
		LuaU_dissassembler::dissassemble(pc, proto, dism);
		retn->dissassembly.insert(std::make_pair(pc, dism));
		pc += dism->len;
		i += dism->len;
	}

	/* Set current proto blocks. */
	#if display_analysis
		std::printf("[AST] Initing main blocks.\n");
	#endif
	blocks::set_blocks(retn);


	do {

		auto current_proto = protos_ast.front();
		auto ast_id = current_proto->ast_id;

		#if display_analysis
				std::printf("[AST] Initing proto [%" PRIuPTR "].\n", reinterpret_cast<std::uintptr_t>(current_proto.get()));
		#endif


		/* Gen children proto to analyze. */
		#if display_analysis
				std::printf("[AST] Setting child proto information.\n");
		#endif
		for (auto i = 0u; i < unsigned (current_proto->p->sizep); ++i) {
			
			/* Make child ast and add proto and get type. */
			auto child_ast = std::make_shared<ast>();
			child_ast->p = current_proto->p->p[i];
			child_ast->transpiler_config = config;
			
			current_proto->protos.emplace_back(child_ast);
			ast_funcs::proto::set_closure_info(current_proto, i);

			/* Add child to get analyzed. */
			protos_ast.emplace_back(child_ast);

			child_ast->ast_id = ++ast_id;
		}

		/* Set dism of child proto. */
		#if display_analysis
				std::printf("[AST] Initing child protos.\n");
		#endif
		for (auto& child : current_proto->protos) {

			#if display_analysis
					std::printf("[AST] Initing child proto [%" PRIuPTR "].\n", reinterpret_cast<std::uintptr_t>(child.get()));
			#endif

			if (child->dissassembly.empty()) {

				#if display_analysis
						std::printf("[AST] Setting child dissasembly.\n");
				#endif

				pc = 0u;

				/* Set current proto dissasembly. */
				for (auto i = 0u; i < unsigned(child->p->sizecode);) {
					auto dism = std::make_shared<LuaU_dissassembler::dissassembly>();
					LuaU_dissassembler::dissassemble(pc, child->p, dism);
					child->dissassembly.insert(std::make_pair(pc, dism));
					pc += dism->len;
					i += dism->len;
				}

			}

			/* Set child proto blocks. */
			if (child->main_block == nullptr || child->main_block->nodes.empty()) {
				#if display_analysis
						std::printf("[AST] Initing child proto main blocks.\n");
				#endif
				blocks::set_blocks(child);
			}

		}

		/* Init ast */
		#if display_analysis
				std::printf("[AST] Initing ast.\n");
		#endif
		ast_funcs::init_ast(current_proto); 

		/* Post ast */
		#if display_analysis
				std::printf("[AST] Post processing ast.\n");
		#endif
		ast_funcs::post_ast(current_proto);

		/* Set end pc and display tree. */
		#if display_analysis
				std::printf("[AST] Setting end pc.\n");
		#endif
		current_proto->pc_end = current_proto->main_block->visit_all().back()->address;
		

		#if display_analysis
				std::printf("[AST] Finished with current ast.\n");
		#endif

		#if display_data 
				std::cout << current_proto->proto_information() << std::endl;
				std::cout << "Tree:\n" << current_proto->tree_str() << std::endl;
		#endif 

		/* Remove current. */
		protos_ast.erase(std::remove(protos_ast.begin(), protos_ast.end(), current_proto), protos_ast.end());

	} while (protos_ast.size());

	return retn;
}