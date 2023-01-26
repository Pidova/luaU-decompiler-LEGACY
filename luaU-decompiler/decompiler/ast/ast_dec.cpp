#include <algorithm>
#include "ast_macros.hpp"
#include "ast_dec.hpp"
#include "post_ast.hpp"
#include "../emitter/emitter.hpp"
#include "../generic/generic.hpp"




namespace global_cache {

	namespace scopes {

		std::unordered_map<std::shared_ptr<ast_dec::node> /* Branch */, std::shared_ptr<ast_dec::node> /* End */> cached_ends;

	}

	void clear() {

		global_cache::scopes::cached_ends.clear();

		return;
	}
}



namespace ast_funcs {

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

				auto prev = ast->main_block->visit_previous_dest_register(node->address, node->lex->dissassembly->operands.front()->reg);

				/* Namecall gets special treatment. */
				if (prev->lex->dissassembly->op == LuauOpcode::LOP_NAMECALL) {

					const auto data = prev->lex->operand_expr< lexer_dec::operand_types::dest>().front()->reg;
					const auto data_1 = prev->lex->operand_expr< lexer_dec::operand_types::source>().front()->reg;

					/* Call first operand will be the same as previous. */
					if (data == data_1) {
						ast->main_block->visit_previous_dest_register(prev->address, node->lex->dissassembly->operands.front()->reg)->add_expr<ast_dec::expr_type::call_routine_start>(); /* Call start */
					}
					else {

						const auto args = node->lex->operand_expr<lexer_dec::operand_types::integer>().front()->val - 1u;

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

					/* Next is end and has args fix prev. */
					if (ast->main_block->visit_next(prev) == node && node->lex->operand_expr<lexer_dec::operand_types::integer>().front()->val) {

						prev = ast->main_block->visit_previous_dest_register(prev->address, node->lex->dissassembly->operands.front()->reg + 1);

					}

					prev->add_expr<ast_dec::expr_type::call_routine_start>(); /* Call start */
				}

				node->add_expr<ast_dec::expr_type::call_routine_end>(); /* Call end. */
			}

			return;
		}


		/* Sets multret call routines **Only applies to variables/args initing variables will get handled by transpiler automatically** */
		void set_multret_routines(std::shared_ptr<ast_dec::ast>& ast) {

			std::uint32_t routine = 0u;

			/* Checking for regs and vector reg. */
			bool first = false;
			std::vector<std::uint16_t> check_regs;

			const auto all = ast->main_block->visit_all();
			for (const auto& node : all) {

				/* Inc for concat start, call start, and table start. Dec for concat end, call end, and start end. */
				routine_inc_lv(node, routine);
				routine_dec_lv(node, routine);

				/* Call? */
				if (!routine && node->lex->type == lexer_dec::inst_type::call && !node->has_expr(ast_dec::expr_type::locvar)) {

					auto retn = node->lex->operand_expr<lexer_dec::operand_types::integer>().back()->val;
					const auto start = node->lex->dissassembly->operands.front()->reg;

					/* Fix for multret. */
					if (retn == LUA_MULTRET) {
						retn = generic::fix_mulret(ast, node->address);
					}

					if (retn > 1) {

						/* Add multrets. */
						for (auto i = start; i < (start + retn); ++i) {
							check_regs.emplace_back(i);
						}

						first = true;

					}

				}

				/* Looks for regs. */
				if (check_regs.size() && node->lex->has_operand_expr<lexer_dec::operand_types::source>()) {

					const auto sources = node->lex->operand_expr<lexer_dec::operand_types::source>();
					for (const auto& operand : sources) {

						const auto reg = operand->reg;
						if (std::find(check_regs.begin(), check_regs.end(), reg) != check_regs.end()) {

							/* First */
							if (first) {
								node->add_expr<ast_dec::expr_type::call_mulret_start>();
								first = false;
							}
							else {

								if (check_regs.size() == 1u) {
									node->add_expr<ast_dec::expr_type::call_mulret_end>();
								}
								else {
									node->add_expr<ast_dec::expr_type::call_mulret_member>();
								}

							}

							check_regs.erase(std::remove(check_regs.begin(), check_regs.end(), reg), check_regs.end());

						}

					}

				}

			}

			return;
		}

		/* Register gets used in call? */
		bool reg_arg(std::shared_ptr<ast_dec::ast>& ast, const std::shared_ptr<ast_dec::node>& call, const std::uint16_t target) {

			/* Call */
			if (call->lex->type == lexer_dec::inst_type::call && call->lex->has_operand_expr<lexer_dec::operand_types::dest>()) {

				const auto dest = call->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;
				auto arg = call->lex->operand_expr<lexer_dec::operand_types::integer>().front()->val;

				if (arg == LUA_MULTRET) {
					arg = generic::fix_mulret(ast, call->address);
				}

				if (arg) {
					return ((1u + dest) <= target && (1u + dest + arg) >= target);
				}

			}

			return false;
		}

	}

	namespace scopes {

		/* Gets end of scope node. (Doesn't count for current, looks for ends/jumps/returns) */
		std::shared_ptr<ast_dec::node> end_of_scope(std::shared_ptr<ast_dec::ast>& ast, const std::shared_ptr<ast_dec::node>& start) {
			

			/* Check cached */
			if (global_cache::scopes::cached_ends.find(start) != global_cache::scopes::cached_ends.end()) {
				return global_cache::scopes::cached_ends[start];  /* Append */
			}

			std::vector <std::uintptr_t> addr_scopes; /* Scopes */

			std::intptr_t scope = 0;

			const auto all = ast->main_block->visit_rest(start->address);
			for (const auto& node : all) {

				/* End of scope. */
				if (!scope && (node->has_expr(ast_dec::expr_type::return_) || node->lex->type == lexer_dec::inst_type::branch)) {
					global_cache::scopes::cached_ends.insert(std::make_pair(start, node)); /* Append */
					return node;
				}

				/* Track scopes. */
				if (std::find(addr_scopes.begin(), addr_scopes.end(), node->address) != addr_scopes.end()) {
					scope -= std::count(addr_scopes.begin(), addr_scopes.end(), node->address);
				}

				/* Way out of scope.*/
				if (scope < 0) {
					global_cache::scopes::cached_ends.insert(std::make_pair(start, node));
					return node;
				}

				/* Add for loop. */
				const auto for_node = node->loop_extra.end_node;
				if (for_node != nullptr && node->loop_extra.start_node == node) {
					addr_scopes.emplace_back(for_node->address);
				}

				/* Append scope */
				if (node->lex->type == lexer_dec::inst_type::branch_condition) {

					/* Scope isnt jumpback? */
					const auto jmp_addr = node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr;
					if (jmp_addr > node->address) {
						const auto end = ast_funcs::scopes::end_of_scope(ast, node);
						addr_scopes.emplace_back((end != nullptr) ? end->address : jmp_addr);
						++scope;
					}

				}

			}

			global_cache::scopes::cached_ends.insert(std::make_pair(start, nullptr)); /* Append */
			return nullptr;
		}

	}

	namespace instructions {

		/* Set return exprs for return opcode(s). */
		void set_return_exprs(std::shared_ptr<ast_dec::ast>& ast) {

			const auto all = ast->main_block->visit_all();
			for (const auto& i : all) {

				switch (i->lex->dissassembly->op) {

					case LuauOpcode::LOP_RETURN: {
						debug_success("Setting return on: %s", i->str().c_str());
						i->add_expr<ast_dec::expr_type::return_>();
						break;
					}

					default: {
						break;
					}
				}

			}

			return;
		}

	}

	namespace regs {

		/* Sees if destination is logical in scope. */
		bool logical_dest_register(std::shared_ptr<ast_dec::ast>& ast, std::shared_ptr<ast_dec::node> start, const std::int16_t target) {

			/* Check for loop too see if they override any of the regs. */
			const auto next_scope = ast->main_block->visit_rest_scope_addr(start->address);
			for (const auto& i : next_scope) {

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
			std::size_t iter_count = 0u; /* Counts times it was iterated. */
			const auto target_1 = target;
			bool used_target_1_dest = true;
			bool used_target_1_source = false;
			bool no_locvar = true;
			std::int8_t used_next = -1; /* Set dest next used source repeat always. */
			std::shared_ptr<ast_dec::node> dest_node = start;
			std::shared_ptr<ast_dec::node> dest_node_nm = start; /* Dest node non mutable by sources. */
			std::int32_t dest_scope = 0u; /* Scope where dest was set. (Can be reset by source) */
			std::int32_t set_scope = 0u; /* Scope where dest was set. (Cannot be reset by source) */

			
			debug_success("Starting with: %s", start->str().c_str());


			/* Really just visiting future instructions too see if table source, dest, or idx gets set twice indicating end. */
			const auto rest_nodes = ast->main_block->visit_rest(start->address);
			for (const auto& s_node : rest_nodes) {
			
				++iter_count;

				/* Checks operands if targets get used or not but if it does get used just resets target. */
				bool used_source_twice = false; /* Used by source twice. */
				bool used_source_routine = false; /* Used by source in routine. */
				std::function<void(const std::shared_ptr<LuaU_dissassembler::operand>&, const lexer_dec::operand_types)> check_usage = [&](const std::shared_ptr<LuaU_dissassembler::operand>& operand, const lexer_dec::operand_types tt) mutable {

					const auto val = operand->reg;

					if (val == target_1) {
						
						debug_line("Source was hit on %s", s_node->str().c_str());

						/* Used twice and last dest node is current. */
						if (used_target_1_source && dest_node_nm == start) {
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
					
						/* See if used next constantly. */
						used_next = (used_next == -1) ? ((dest_node->address + dest_node->lex->dissassembly->len) == s_node->address) : -2;

						dest_scope = 0;
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
	
				/* Set but now used outside of scope. */
				if (dest_scope > scope && scope > 0) {
					debug_success("Used outside of scope valid register on %s", s_node->str().c_str());
					no_locvar = false;
					break;
				}

				/* Out of scope or last scope and hit return. */
				if (scope < 0 || (!scope && s_node->has_expr(ast_dec::expr_type::return_))) {
					debug_line("Register use out of scope or hit return without scoped.");
					break;
				}
			
				/* End of scope */
				if (!scope && s_node->has_expr(ast_dec::expr_type::return_)) {
					
					/* Just ended with it just using it as dest. */
					if (used_target_1_dest && !used_target_1_source) {
						debug_success("Return used register as source on %s", s_node->str().c_str());
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
				s_node->lex->operand_expr_callback<lexer_dec::operand_types::table_reg>(check_usage);

				/* Call */
				if (ast_funcs::calls::reg_arg(ast, s_node, target)) {

					debug_line("Source argument was hit on %s", s_node->str().c_str());

					used_target_1_source = true;
					used_target_1_dest = false;
					dest_scope = 0;
					dest_node = nullptr;

				}

				/* Used by source twice. */
				if (used_source_twice) {
					debug_success("Used register twice as a source on %s", s_node->str().c_str());
					no_locvar = false;
					break;
				}
				

				/* Used in source in routine. */
				if (used_source_routine) {
					debug_success("Used register as source in routine on %s", s_node->str().c_str());
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
						
						debug_line("Dest was hit on %s", s_node->str().c_str());

						if (routine) { /* Used in routine not locvar. */
							debug_warning("Register used as dest inside routine on %s", s_node->str().c_str());
							no_locvar = true;
							break;
						}

						/* Set twice without used. Locvar */
						if (used_target_1_dest && set_scope == scope) {
							debug_warning("Register was set twice without being used on %s", s_node->str().c_str());
							no_locvar = (dest_node != start);
							break;
						}

						used_target_1_dest = true;
						used_target_1_source = false;
						dest_node = s_node;
						dest_node_nm = s_node;
						set_scope = scope;
						dest_scope = scope;

					}

				}

			}

			/* Not used at all. */
			if (!used_target_1_source && dest_node == start) {
				debug_success("Not used at all.");
				return true;
			}

			/* Not locvar by routine. */
			if (no_locvar || (no_locvar && used_next == 1)) {
				debug_warning("Not a locvar.");
				return false;
			}
		
			/* Ignore var next to return. */
			const auto next = ast->main_block->visit_next(start);
			if (next != nullptr && next->has_expr(ast_dec::expr_type::return_) && iter_count == 1u) {
				debug_warning("Register created right before a return instruction not a locvar.");
				return false;
			}
		
			debug_success("It is a locvar.");
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
			
			/* Captures follow newclosure or dupclosure(RARLEY). */
			auto closures = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_NEWCLOSURE>(true));
			const auto dupclosures = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_DUPCLOSURE>(true));

			/* All closures. */
			closures.insert(closures.end(), dupclosures.begin(), dupclosures.end());

			for (const auto& node : closures) {
					
					debug_line("Closure at: %s", node->str().c_str());

					/* Incase it bugs out. */
					node->closure_extra.closure_idx = node->lex->dissassembly->operands.back()->k_idx /* Will work for proto. */;

					auto idx = 0u;
					auto next = ast->main_block->visit_next(node);

					while (next != nullptr && next->lex->dissassembly->op == LuauOpcode::LOP_CAPTURE) {
						
						debug_line("Iterating through capture at: %s", next->str().c_str());

						if (next->lex->has_operand_expr<lexer_dec::operand_types::upvalue>()) {
							/* Pass upvalue */
							debug_success("Next is upvalue.");
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
									debug_success("Upvalue is a arguement.");
									arg.second = name;
									goto next_L;
								}

							/* See if function */
							if (node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg == reg) {
								debug_success("Upvalue is a function.");
								ast->protos[node->closure_extra.closure_idx]->closure_name = name;
								goto next_L;
							}

							/* See if var/sub or iterator/sub */
							const auto back = ast->main_block->visit_rest_curr_flip(node->address);
							for (const auto& node_ : back) {

								/* Variable/local function */
								if (node_->lex->has_operand_expr<lexer_dec::operand_types::dest>() && (node_->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg == reg || ((node_->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg + node_->dest_loc.multret_amount) >= reg && node_->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg <= reg))) {

									if (node_->has_expr(ast_dec::expr_type::closure_local)) {
										/* local function ?? */
										debug_success("Upvalue is a local function.");

										/* Set name of upvalue closure. */
										ast->protos[(node->lex->has_operand_expr<lexer_dec::operand_types::kvalue>()) ? node_->lex->operand_expr<lexer_dec::operand_types::kvalue>().front()->k_idx : node_->lex->operand_expr<lexer_dec::operand_types::proto>().front()->proto]->closure_name = name;
										break;
									}
									else if (node_->has_expr(ast_dec::expr_type::locvar)) {
										/* Variable */
										debug_success("Upvalue is a local variable.");

										/* Set multret names if is. */
										if (!node_->dest_loc.multret_amount) {
											node_->dest_loc.name = name;
										}
										else {
											node_->dest_loc.multret_names[reg - node_->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg] = name;
										}

										node_->add_existance<ast_dec::expr_type::locvar_upvalue>();
										break;
									}
							
								}
								else if (node_->has_expr(ast_dec::expr_type::for_iv_start) || node_->has_expr(ast_dec::expr_type::for_start) || node_->has_expr(ast_dec::expr_type::for_n_start)) {
									
									/* Check for loops */
									const auto for_target = node_->loop_extra.end_node;
							
									/* Not a jumpback */
									if (for_target->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp > 0)
										continue;
									
									/* Iterator */
									if (for_target->loop_extra.start_reg <= reg && for_target->loop_extra.end_reg >= reg) {
										debug_success("Upvalue is a for loop variable.");
										node_->loop_extra.iteration_names.insert(std::make_pair(reg, name));
										goto next_L;
									}
									

									/* Sub iterator movs for == of capture reg */
									if (node_->sub_node != nullptr && node_->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg == reg) {
										debug_success("Upvalue is a loop variable in relation with subnode.");
										node_->loop_extra.iteration_names.insert(std::make_pair(node_->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg, name));
										goto next_L;											
									}
	
								} 
								else if (node_->sub_node != nullptr && node_->get_sub_node()->has_expr(ast_dec::expr_type::locvar) && node_->get_sub_node()->lex->has_operand_expr<lexer_dec::operand_types::dest>() && (node_->get_sub_node()->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg == reg || ((node_->get_sub_node()->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg + node_->dest_loc.multret_amount) >= reg && node_->get_sub_node()->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg <= reg))) {
									
									debug_success("Upvalue is apart of a call multret.");

									/* Set multret names if is. */
									if (!node_->dest_loc.multret_amount) {
										node_->get_sub_node()->dest_loc.name = name;
									}
									else {
										node_->get_sub_node()->dest_loc.multret_names[reg - node_->get_sub_node()->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg] = name;
									}
									
									node_->get_sub_node()->add_existance<ast_dec::expr_type::locvar_upvalue>();
									goto next_L;
								}
								
							}

						}
					
						next_L:
							next = ast->main_block->visit_next(next);
					}
					
			}


			return;
		}

	}

	namespace proto {

		void set_closure_info(const std::shared_ptr<ast_dec::ast>& current_proto, const std::size_t child_proto_id) {

			std::shared_ptr<ast_dec::node> end_capture = nullptr;
			std::shared_ptr<ast_dec::node> closure_node = nullptr;

			/* Newcloure, Dupclosures node. */
			auto closures = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(current_proto->main_block->visit_inst<LuauOpcode::LOP_NEWCLOSURE>(true));
			const auto dupclosures = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(current_proto->main_block->visit_inst<LuauOpcode::LOP_DUPCLOSURE>(true));
	
			/* All closures. */
			closures.insert(closures.end(), dupclosures.begin(), dupclosures.end());
		
			/* Iterate through closures and get expression for it relative to proto given. */
			for (const auto& i : closures) {

				/* Already analyzed. */
				if (i->has_expr(ast_dec::expr_type::closure_global) || i->has_expr(ast_dec::expr_type::closure_local) || i->has_expr(ast_dec::expr_type::closure_newclosure))
					continue;

				/* Expression has been set. */
				if (closure_node != nullptr)
					break;

				/* Set newcclosure node. */
				if (i->lex->dissassembly->operands[1]->proto == child_proto_id) {

					closure_node = i; /* Set node. */

					/* Skip captures if any. */
					auto next = current_proto->main_block->visit_next(i);
					while (next != nullptr && next->lex->dissassembly->op == LuauOpcode::LOP_CAPTURE) {
						next = current_proto->main_block->visit_next(next);
					}
					
					const auto next_prev = current_proto->main_block->visit_previous_addr(next->address);
					end_capture = (next != nullptr && next->lex->dissassembly->op == LuauOpcode::LOP_CAPTURE) ? next : (next_prev != nullptr && next_prev->lex->dissassembly->op == LuauOpcode::LOP_CAPTURE) ? next_prev : closure_node;

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

			}
			
			/* Turn expression to closure type. */

			auto proto = current_proto->protos[child_proto_id];

			/* Check for getimport followed by potential gettable. */
			std::int16_t target = -1;
			std::shared_ptr<ast_dec::node> next = end_capture;
			std::string name = "";
			do {

				next = current_proto->main_block->visit_next(next);

				if (next != nullptr) {
	
					if (next->lex->dissassembly->op == LuauOpcode::LOP_GETIMPORT) {

						/* Can't use getimport twice. */
						if (target != -1) {
							next = nullptr;
							break;
						}

						target = next->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;
						name += next->lex->operand_expr<lexer_dec::operand_types::kvalue>().front()->k_value;

					}
					else if (next->lex->type == lexer_dec::inst_type::table_get && next->lex->has_operand_expr<lexer_dec::operand_types::kvalue>()) {

						const auto dest = next->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;
						const auto source = next->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg;

						if (target == source) {
							target = dest;
						}
						else {
							next = nullptr;
							break;
						}

						name += '.' + next->lex->operand_expr<lexer_dec::operand_types::kvalue>().front()->k_value;

					}
					else if (next->lex->type == lexer_dec::inst_type::table_set) {

						const auto source = next->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg;
						const auto reg = next->lex->operand_expr<lexer_dec::operand_types::reg>().front()->reg;

						if (source == closure_node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg && reg == target) {
							name += '.' + next->lex->operand_expr<lexer_dec::operand_types::kvalue>().front()->k_value;
							break;
						}

						next = nullptr;
						break;
					}
					else {
						/* Only getimport or table get/set. */
						next = nullptr;
						break;
					}

				}

			} while (next != nullptr);


			/* Change too global function with set name. */
			if (next != nullptr) {

				/* Erase , ", ' */
				if (name.find('\"') != std::string::npos) {
					name.erase(std::remove(name.begin(), name.end(), '\"'), name.end());
				}

				if (name.find('\'') != std::string::npos) {
					name.erase(std::remove(name.begin(), name.end(), '\''), name.end());
				}


				proto->closure_type = ast_dec::closure_type::global;
				proto->closure_name = name;
		

				/* Replace next */
				if (closure_node->has_expr(ast_dec::expr_type::closure_local)) {
					closure_node->replace_next<ast_dec::expr_type::closure_local, ast_dec::expr_type::closure_global>();
				}
				else if (closure_node->has_expr(ast_dec::expr_type::closure_newclosure)) {
					closure_node->replace_next<ast_dec::expr_type::closure_newclosure, ast_dec::expr_type::closure_global>();
				}


				/* Fill with dead instructions. */
				for (const auto& i : current_proto->main_block->visit_range_current((closure_node->address + closure_node->lex->dissassembly->len), next->address))
					i->add_expr<ast_dec::expr_type::dead_instruction>(); /* Handled by before hand. */

				closure_node->closure_extra.idx_nodes = std::make_pair(end_capture, next);
			}
			else {

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

			}

			return;
		}

	}

	namespace branches {

		/* Set logical operations in routine with given range. */
		void set(std::shared_ptr<ast_dec::ast>& ast, const std::uintptr_t begin, const std::uintptr_t end, const std::vector <std::uintptr_t> dead /* Always opposite and when hit. */, const std::int16_t logical_operation_target = -1 /* Used for ignoring ands/ors. */, const bool check_loops = false /* Checks jumps too see if they lead too loop by expr and preform opposite. */, const bool last_include = false /* Includes last compare as or. */) {

			const auto conditions = ast->main_block->visit_range_type<lexer_dec::inst_type::branch_condition>(begin, end);
			const auto over_target = *std::max_element(dead.begin(), dead.end());
			const auto range = ast->main_block->visit_range_current(begin, end);
			const auto loadbs = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_LOADB>(true));

			std::vector <std::pair <std::uintptr_t /* Jump */, std::size_t /* Scopes */>> scopes;
			std::unordered_map<std::uintptr_t /* Jmp loc */, std::size_t /* Count */> jmp_hit;

			const auto has_val = [&scopes](const std::uintptr_t addr) {
				return std::find_if(scopes.begin(), scopes.end(), [&addr](const std::pair<std::uintptr_t, std::size_t>& ele) { return ele.first == addr; }) != scopes.end();
			};


			for (const auto& node : range) {

				if (node->lex->type == lexer_dec::inst_type::branch_condition || node->lex->type == lexer_dec::inst_type::branch) {
					
					const auto jmp = node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr;

					/* Add hit */
					if (jmp_hit.find(jmp) == jmp_hit.end()) {
						jmp_hit.insert(std::make_pair(jmp, 0u));
					}
					else {
						++jmp_hit[jmp];
					}

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

						/* Include last */
						if (conditions.back() == node && last_include) {
							node->add_expr<ast_dec::expr_type::condition_or>();
							node->branch_extra.opposite = true;
						}

						/* Fix for loop/while/repeat/generic */
						const auto prev = ast->main_block->visit_previous_addr(jmp);
						if (check_loops && prev != nullptr && condition_break_out(prev)) {

							node->branch_extra.opposite = false;

						}

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
							else if (logical_operation_target != -1 && ast->main_block->visit_next(node)->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg != logical_operation_target) {
								continue;
							}

						}
					
						/* Jmp exceeds dead values. */				
						if (jmp > over_target) {

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

								/* and/or */
								if (!scopes.empty()) {
									
									if (jmp_hit[jmp]) {
										node->add_expr<ast_dec::expr_type::condition_or>();
									}
									else {
										node->add_expr<ast_dec::expr_type::condition_and>();
									}

								}
								
							}
							else {

								
								/* New and sub scope */
								if (scopes.back().first != jmp && scopes.back().first > jmp) {
									node->add_expr<ast_dec::expr_type::condition_and>();
									node->add_expr<ast_dec::expr_type::condition_open>();
									scopes.push_back(std::make_pair(jmp, 1u /* Starts off with one could inc. */));
									node->branch_extra.opposite = true;
								}
								else {
							
									/* Jmps too end. */
									if (jmp == end || jmp == (end + ast->main_block->visit_addr(end)->lex->dissassembly->len)) {
										node->add_expr<ast_dec::expr_type::condition_or>();
									}
									else {

										/* Jmp doesnt = current and its found in scopes. */
										if (scopes.back().first != jmp && has_val(jmp)) {
											node->add_expr<ast_dec::expr_type::condition_or>();
										}
										else {
											node->add_expr<ast_dec::expr_type::condition_and>();
											node->branch_extra.opposite = true;
										}

									}

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
					const auto next = ast->main_block->visit_next(node);
					if (next != nullptr && next->lex->dissassembly->op == LuauOpcode::LOP_LOADB) {

						if (next->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp) {

							node->add_expr<ast_dec::expr_type::condition_emit_next>();
							node = ast->main_block->visit_next(next);

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

			 * Sets breaks too if not there. 
		*/
		void set_branch_statements(std::shared_ptr<ast_dec::ast>& ast) {

			/* Unsafe too make compares as definite concat if those are really variables will get handled later. */

			const auto routines = std::get<std::vector<std::pair <std::shared_ptr<ast_dec::node>, std::shared_ptr<ast_dec::node>>>>(ast->main_block->visit_expr_routine<ast_dec::expr_type::condition_logical_start, ast_dec::expr_type::condition_logical_end>(true));
			for (const auto& i : routines) {
				
				if (i.second->lex->type == lexer_dec::inst_type::branch_condition && !i.first->has_expr(ast_dec::expr_type::condition_concat_start) && !i.first->has_expr(ast_dec::expr_type::condition_concat_end)) {
					
					/* If or elseif/else */
					const auto jmp_addr = i.second->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr;
					const auto concat_start = i.first;
					const auto concat_end = i.second;
					const auto scope_end = ast_funcs::scopes::end_of_scope(ast, concat_end);
					

					auto routine_end = ast->main_block->visit_addr(jmp_addr);

					
					/* If statement */
					concat_start->add_expr<ast_dec::expr_type::condition_concat_start>();
					concat_end->add_expr<ast_dec::expr_type::condition_concat_end>();
					concat_end->add_expr<ast_dec::expr_type::if_>();
					debug_success("If statement set on %s", concat_end->str().c_str());


					/* See if jump is greater then 1. */
					ast_funcs::branches::set(ast, concat_start->address, concat_end->address, { jmp_addr }, -1, true);
					
					/* Propagate conditional concat members. */
					const auto range = ast->main_block->visit_range(concat_start->address, concat_end->address);
					for (const auto& cm_node : range) {
						cm_node->add_expr<ast_dec::expr_type::condition_concat_member>();
					}


					/* Set elseifs/else. */
					auto jmp_addr_node = ast->main_block->visit_addr(jmp_addr);
					auto prev_jmp = ast->main_block->visit_previous_addr(jmp_addr);
					const auto prev_jmp_const = prev_jmp;
					

					/* Loop for jumps. */
					while (prev_jmp->lex->dissassembly->op == LuauOpcode::LOP_JUMP /* Jump forward else */ && !prev_jmp->has_expr(ast_dec::expr_type::break_)) {
					
						debug_line("Jump from current condition leads too previous jump without it being a break on %s with jump target %s", prev_jmp->str().c_str(), jmp_addr_node->str().c_str());

						/* No jump condition else statement. */
						if (!jmp_addr_node->has_expr(ast_dec::expr_type::condition_logical_start) || !jmp_addr_node->has_expr(ast_dec::expr_type::condition_logical_end)) {
							
							debug_line("Jump target isn't a logical condition.");

							/* Check next if if existing. */
							auto next_jmp = std::get<std::shared_ptr<ast_dec::node>>(ast->main_block->visit_next_type_addr<lexer_dec::inst_type::branch_condition>(jmp_addr_node->address, false));
						
							if (next_jmp == nullptr || next_jmp->address > prev_jmp->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr  || 
								!next_jmp->has_expr(ast_dec::expr_type::condition_logical_start) || !next_jmp->has_expr(ast_dec::expr_type::condition_logical_end) ||
								!ast->main_block->visit_next(prev_jmp)->has_expr(ast_dec::expr_type::condition_logical_start)
							) {
								
							else_statement:

								debug_line("Next jump from jump target doesn't exist, out of scope, doesn't have any conditions, etc.");

								routine_end = ast->main_block->visit_addr(prev_jmp_const->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr);
								prev_jmp->add_expr<ast_dec::expr_type::jump_else>();
								jmp_addr_node->add_expr<ast_dec::expr_type::else_>();
								
								debug_success("Set else statement on %s, routine end on %s from %s", jmp_addr_node->str().c_str(), routine_end->str().c_str(), prev_jmp_const->str().c_str());

								break;
							}

							if (ast->main_block->visit_next(prev_jmp)->has_expr(ast_dec::expr_type::condition_logical_start)) {
								//goto else_statement;
							}
							
						}
						
						/* Take jump and get final condition logical end node. */
						const auto jmp_addr_cl_routine_end = std::get<std::shared_ptr<ast_dec::node>>(ast->main_block->visit_next_expr_current<ast_dec::expr_type::condition_logical_end>(jmp_addr_node->address, false));
						const auto jmp_addr_cl_routine_end_jmp = jmp_addr_cl_routine_end->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr;
						
						/* Already analyzed */
						if (jmp_addr_node->has_expr(ast_dec::expr_type::condition_concat_start)) {
							debug_warning("Jump target already has been analyzed on %s", jmp_addr_node->str().c_str());
							break;
						}
					
						/* Condition is different else if */
						if (prev_jmp->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr != prev_jmp_const->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr) {				
							/* Set end */
							debug_line("Current previous jump instruction from jump target jump doesn't match with constant on %s", prev_jmp->str().c_str());

							routine_end = ast->main_block->visit_addr(prev_jmp_const->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr);
							prev_jmp->add_expr<ast_dec::expr_type::jump_else>();
							jmp_addr_node->add_expr<ast_dec::expr_type::else_>();

							debug_success("Set else statement on %s", jmp_addr_node->str().c_str());
							break;
						}

						/* eleseif end */
						routine_end = ast->main_block->visit_addr(prev_jmp_const->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr);

						/* No elseif */
						if (!jmp_addr_cl_routine_end->has_expr(ast_dec::expr_type::elseif_)) {

							debug_line("Jump location next conditional logical routine doesn't have elseif on %s", jmp_addr_cl_routine_end->str().c_str());

							/* elseif statement */
							jmp_addr_node->add_expr<ast_dec::expr_type::condition_concat_start>();
							jmp_addr_cl_routine_end->add_expr<ast_dec::expr_type::condition_concat_end>();
							jmp_addr_cl_routine_end->add_expr<ast_dec::expr_type::elseif_>();
							prev_jmp->add_expr<ast_dec::expr_type::jump_elseif>();

							debug_success("Set elseif statement on %s", prev_jmp->str().c_str());

							/* Set data */
							ast_funcs::branches::set(ast, jmp_addr_node->address, jmp_addr_cl_routine_end->address, { jmp_addr_cl_routine_end_jmp }, -1, true);

							/* Propagate conditional concat members. */
							const auto range = ast->main_block->visit_range(jmp_addr_node->address, jmp_addr_cl_routine_end->address);
							for (const auto& cm_node : range) {
								debug_success("Condition concat member set on %s", cm_node->str().c_str());
								cm_node->add_expr<ast_dec::expr_type::condition_concat_member>();
							}

						}

						/* Set next */
						jmp_addr_node = ast->main_block->visit_addr(jmp_addr_cl_routine_end->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr);
						prev_jmp = ast->main_block->visit_previous_addr(jmp_addr_node->address);

					}					


					/* Fix routine_end based on jump. */
					if (scope_end != nullptr && scope_end->lex->dissassembly->op == LuauOpcode::LOP_JUMP && jmp_addr == scope_end->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr) {
						debug_line("Mutating routine end with scope end %s", scope_end->str().c_str());
						routine_end = scope_end;
						debug_line("Routine end mutated too %s", routine_end->str().c_str());
					}

					routine_end->add_expr<ast_dec::expr_type::scope_end>(1u, (concat_end->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr > 1u) ? ast_dec::element::front : ast_dec::element::back /* Empty expression? */);
					debug_success("Set end expr on %s", routine_end->str().c_str());

					/* Loop/while/repeat/generic */
					const auto prev = ast->main_block->visit_previous_addr(concat_end->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr /* Take jump */);
					if (condition_break_out(prev)) {

						debug_line("Previous breaks out of loop routine on %s", prev->str().c_str());

						/* Append break */
						concat_end->add_expr<ast_dec::expr_type::condition_break>();

						/* Remove scope end */
						routine_end->remove_expr<ast_dec::expr_type::scope_end>();

						debug_success("Set break on %s", concat_end->str().c_str());

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
				ast->main_block->visit_previous_dest_register(node->address, node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg)->add_expr<ast_dec::expr_type::concat_routine_start>(); /* Concat start. */
				node->add_expr<ast_dec::expr_type::concat_routine_end>(); /* Concat end. */
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

					debug_success("Label at %s", node->str().c_str());

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

		/* Sets forprep instructions exprs. ** Can be called by parent in favor for children** */
		void set_for_prep_exprs(std::shared_ptr<ast_dec::ast>& ast) {

			const auto all = ast->main_block->visit_all();
			for (const auto& i : all) {

				switch (i->lex->dissassembly->op) {

					case LuauOpcode::LOP_FORGPREP_INEXT:
					case LuauOpcode::LOP_FORGPREP_NEXT:
					case LuauOpcode::LOP_FORGPREP:
					case LuauOpcode::LOP_FORNPREP: {
						if (!i->has_expr(ast_dec::expr_type::for_prep)) {
							debug_success("Setting forprep expr on %s", i->str().c_str());
							i->add_expr<ast_dec::expr_type::for_prep>();
						}
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

			** Can be called by parent in favor for children**
		*/
		void set_for_routines(std::shared_ptr<ast_dec::ast>& ast) {

			const auto forgloops = std::get<std::vector<std::shared_ptr<ast_dec::node>>> (ast->main_block->visit_inst<LuauOpcode::LOP_FORGLOOP>(true));
			const auto fornloops = std::get<std::vector<std::shared_ptr<ast_dec::node>>> (ast->main_block->visit_inst<LuauOpcode::LOP_FORNLOOP>(true));
		
			/* Forgloops. */
			for (const auto& forloop : forgloops) {

				const auto jump_node = ast->main_block->visit_addr(forloop->address + forloop->lex->dissassembly->operands[1]->jmp /* Take exact jump */);
				const auto jump_inst = jump_node->lex->dissassembly->op;

				/* Already analyzed */
				if (jump_node->loop_extra.end_node != nullptr && forloop->loop_extra.end_node != nullptr) {
					continue;
				}

				/* for i,v in ipairs/pairs */
				if (jump_inst == LuauOpcode::LOP_FORGPREP_INEXT || jump_inst == LuauOpcode::LOP_FORGPREP_NEXT) {
					debug_success("Setting for iv routine on %s", forloop->str().c_str());
					jump_node->add_expr<ast_dec::expr_type::for_iv_start>();
					forloop->add_expr<ast_dec::expr_type::for_iv_end>();
				}
				else {
					debug_success("Setting generic for routine on %s", forloop->str().c_str());
					jump_node->add_expr<ast_dec::expr_type::for_start>();
					forloop->add_expr<ast_dec::expr_type::for_end>();
				}

				forloop->add_expr<ast_dec::expr_type::scope_end>();

				jump_node->loop_extra.start_node = jump_node;
				forloop->loop_extra.start_node = jump_node;
				jump_node->loop_extra.end_node = forloop;
				forloop->loop_extra.end_node = forloop;

				/* Set start/end registers */
				forloop->loop_extra.start_reg = forloop->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg;
				forloop->loop_extra.end_reg = forloop->loop_extra.start_reg + 4u;
				
			}

			/* Fornloops. */
			for (const auto& forloop : fornloops) {

				const auto loop = ast->main_block->visit_addr(forloop->lex->dissassembly->operands[1]->jmp_addr);

				/* Already analyzed */
				if (loop->loop_extra.end_node != nullptr && forloop->loop_extra.end_node != nullptr) {
					continue;
				}

				debug_success("Setting for n routine on %s", forloop->str().c_str());
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

								/* Propagate conditional concat members. */
								const auto range = ast->main_block->visit_range(cond.first->address, cond.second->address);
								for (const auto& cm_node : range) {
									cm_node->add_expr<ast_dec::expr_type::condition_concat_member>();
								}

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
							
								/* Propagate conditional concat members. */
								const auto range = ast->main_block->visit_range(begin_node->address, cond.second->address);
								for (const auto& cm_node : range) {
									cm_node->add_expr<ast_dec::expr_type::condition_concat_member>();
								}

							}
							
						}
						else {
							/* Last conditon doesn't jump out possible repeat until(true)*/
							jmp_node->add_expr<ast_dec::expr_type::repeat_>();
							jumpback->add_expr<ast_dec::expr_type::condition_true>(1u, ast_dec::element::front);
						}

					}

				}

				/* End of routine is given with until. Find begging. */
				for (const auto& node : nodes_routine) {

					/* Break **Not definite jump to end of until routine or end of while can mean break of any conditional** */
					if (node->lex->dissassembly->op == LuauOpcode::LOP_JUMP || node->lex->dissassembly->op == LuauOpcode::LOP_JUMPX) {

						if (!node->has_expr(ast_dec::expr_type::break_) /* No break */ && node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr > jumpback->address) {
							node->add_expr<ast_dec::expr_type::break_>();
						}

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
				std::vector <std::uint32_t> dests_ns; /* (NOT-Scoped) Registers used in dest. */
				std::vector <std::vector <std::shared_ptr<ast_dec::node>>> dests_nodes; /* (INIT) Dest nodes relative to dests (Can't be pair as too this is used for something entirely different from arguments) */
				std::vector <std::unordered_map <std::uint16_t /* Reg */, std::shared_ptr<ast_dec::node> /* Node */>> dest_nodes_map; /* When reg was last set. */
				std::vector <std::uint32_t> source_no_dest; /* Registers used in source, value but not dest. **Not written too yet but been used** */
				std::vector <std::uintptr_t> addr_scopes; /* Scopes */


				/* Emblace first */
				dests.emplace_back(std::vector <std::uint32_t>({ }));
				dests_nodes.emplace_back(std::vector <std::shared_ptr<ast_dec::node>>({ }));
				dest_nodes_map.emplace_back(std::unordered_map <std::uint16_t /* Reg */, std::shared_ptr<ast_dec::node> /* Node */>({ }));


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
							dest_nodes_map.pop_back();
						}

					}


					/* Loops are handled ahead of time as for child proto see if it has for loop and its start. */
					const auto for_node = node->loop_extra.end_node;
					if (for_node != nullptr && node->loop_extra.start_node == node) {


						/* Loop jumpback */
						addr_scopes.emplace_back(for_node->address);
						dests.emplace_back(dests.back());
						dests_nodes.emplace_back(dests_nodes.back());
						dest_nodes_map.emplace_back(dest_nodes_map.back());


						/* See if theres prep for for loop then add if so. */
						auto for_dest = node;
						if (!for_dest->has_expr(ast_dec::expr_type::for_prep)) {
							
							const auto prev = ast->main_block->visit_previous_addr(for_dest->address);
							if (prev->has_expr(ast_dec::expr_type::for_prep))
								for_dest = prev;

						}

					
						/* Add vars from those loops. */
						for (auto i = node->loop_extra.end_node->loop_extra.start_reg; i <= node->loop_extra.end_node->loop_extra.end_reg; ++i) {

							/* Add node too dest_nodes_map. */
							if (dest_nodes_map.back().find(i) != dest_nodes_map.back().end()) {
								dest_nodes_map.back()[i] = node;
							}
							else {
								dest_nodes_map.back().insert(std::make_pair(i, node));
							}

							if (std::find(dests.back().begin(), dests.back().end(), i) == dests.back().end()) { /* Didnt find dest */
								dests.back().emplace_back(i);
								dests_ns.emplace_back(i);
								dests_nodes.back().emplace_back(for_dest);
							}
							else { /* Found dest, mutate dest node. */
								dests_nodes.back()[std::find(dests.back().begin(), dests.back().end(), i) - dests.back().begin()] = for_dest;
							}

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
										node->dest_nodes_init.emplace_back(dests_nodes.back()[std::find(dests.back().begin(), dests.back().end(), a) - dests.back().begin()]);
									}

							}


							break;
						}

						case LuauOpcode::LOP_GETVARARGS: {


							const auto dest = node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;
							auto amt = node->lex->operand_expr<lexer_dec::operand_types::integer>().front()->val;

							if (amt == LUA_MULTRET) {
								amt = (*(&node - 1u))->lex->dissassembly->operands.front()->reg;
							}

							if (amt == 0) {
								amt = 1u;
							}


							for (auto a = dest; a < (dest + amt); ++a) {

								/* Add node too dest_nodes_map. */
								if (dest_nodes_map.back().find(a) != dest_nodes_map.back().end()) {
									dest_nodes_map.back()[a] = node;
								}
								else {
									dest_nodes_map.back().insert(std::make_pair(a, node));
								}

								if (std::find(dests.back().begin(), dests.back().end(), a) == dests.back().end()) {
									dests.back().emplace_back(a);
									dests_ns.emplace_back(a);
									dests_nodes.back().emplace_back(node);
								}
								else {
									node->dest_nodes_init.emplace_back(dests_nodes.back()[std::find(dests.back().begin(), dests.back().end(), a) - dests.back().begin()]);
								}

							}


							break;
						}

						case LuauOpcode::LOP_CALL: {

							auto retn = node->lex->operand_expr<lexer_dec::operand_types::integer>().back()->val;
							auto args = node->lex->operand_expr<lexer_dec::operand_types::integer>().front()->val;
							const auto start = node->lex->dissassembly->operands.front()->reg;

							/* Fix for multret. */
							if (args == LUA_MULTRET) {
								args = (generic::fix_mulret(proto, node->address) - start);
							}

							if (retn == LUA_MULTRET) {
								retn = generic::fix_mulret(proto, node->address);
							}


							/* Fix with namecall. */
							bool namecall = false;
							const auto prev = proto->main_block->visit_previous_addr(node->address);

							if (prev->lex->dissassembly->op == LuauOpcode::LOP_NAMECALL) {

								const auto data = prev->lex->operand_expr< lexer_dec::operand_types::dest>().front()->reg;

								if (start == data) {
									namecall = true;
								}

							}
							

							/* Iterate through args and see if arg is not getting used in dest. */
							for (auto i = 0; i < args; ++i) {
					
								/* Skip this */
								if (!i && namecall) {
									dests.back().emplace_back(i + 1u);
									dests_nodes.back().emplace_back(prev);
									continue;
								}

								const auto arg = (i + start + 1u);

								if (std::find(dests.back().begin(), dests.back().end(), arg) == dests.back().end() &&
									std::find(source_no_dest.begin(), source_no_dest.end(), arg) == source_no_dest.end()) {
									source_no_dest.emplace_back(arg);
								}

							}


							/* Append placement for call. */
							if (node->lex->has_operand_expr<lexer_dec::operand_types::dest>() && std::find(dests.back().begin(), dests.back().end(), start) == dests.back().end() &&
								std::find(source_no_dest.begin(), source_no_dest.end(), start) == source_no_dest.end()) {
								source_no_dest.emplace_back(start);
							}

							/* Append to dest. (Fill for multiple retn) */
							for (auto i = start; i < (start + retn); ++i) {

								/* Add node too dest_nodes_map. */
								if (dest_nodes_map.back().find(i) != dest_nodes_map.back().end()) {
									dest_nodes_map.back()[i] = node;
								}
								else {
									dest_nodes_map.back().insert(std::make_pair(i, node));
								}

								dests_ns.emplace_back(i);

								if (std::find(dests.back().begin(), dests.back().end(), i) == dests.back().end()) {
									dests.back().emplace_back(i);
									dests_nodes.back().emplace_back(node);
								}

							}
							

							break;
						}

						default: {
							
							/* Append source. */
							if (node->lex->has_operand_expr<lexer_dec::operand_types::source>()) {

								const auto regz = node->lex->operand_expr<lexer_dec::operand_types::source>();

								for (const auto& operand : regz) {

									if (dest_nodes_map.back().find(operand->reg) != dest_nodes_map.back().end()) {
										node->source_nodes.emplace_back(dest_nodes_map.back()[operand->reg]);
									}

									/* Append unused reg. */
									if (std::find(dests.back().begin(), dests.back().end(), operand->reg) == dests.back().end() && std::find(source_no_dest.begin(), source_no_dest.end(), operand->reg) == source_no_dest.end()) {
										source_no_dest.emplace_back(operand->reg);
									}
									else if (std::find(dests.back().begin(), dests.back().end(), operand->reg) != dests.back().end()) {
										node->source_nodes_init.emplace_back(dests_nodes.back()[std::find(dests.back().begin(), dests.back().end(), operand->reg) - dests.back().begin()]);
									}

								}

							}


							/* Append reg. */
							if (node->lex->has_operand_expr<lexer_dec::operand_types::reg>()) {

								const auto regz = node->lex->operand_expr<lexer_dec::operand_types::reg>();

								for (const auto& operand : regz) {

									if (dest_nodes_map.back().find(operand->reg) != dest_nodes_map.back().end()) {
										node->source_nodes.emplace_back(dest_nodes_map.back()[operand->reg]);
									}

									/* Append unused reg. */
									if (std::find(dests.back().begin(), dests.back().end(), operand->reg) == dests.back().end() && std::find(source_no_dest.begin(), source_no_dest.end(), operand->reg) == source_no_dest.end()) {
										source_no_dest.emplace_back(operand->reg);

									}
									else if (std::find(dests.back().begin(), dests.back().end(), operand->reg) != dests.back().end()) {
										node->source_nodes_init.emplace_back(dests_nodes.back()[std::find(dests.back().begin(), dests.back().end(), operand->reg) - dests.back().begin()]);
									}


								}

							}


							/* Append dest. */
							if (node->lex->has_operand_expr<lexer_dec::operand_types::dest>()) {

								const auto reg = node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;

								/* Add node too dest_nodes_map. */
								if (dest_nodes_map.back().find(reg) != dest_nodes_map.back().end()) {
									dest_nodes_map.back()[reg] = node;
								}
								else {
									dest_nodes_map.back().insert(std::make_pair(reg, node));
								}

								dests_ns.emplace_back(reg);
								if (node->lex->dissassembly->op == LuauOpcode::LOP_NAMECALL) {
									dests_ns.emplace_back(reg + 1u);
								}

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
									node->dest_nodes_init.emplace_back(dests_nodes.back()[std::find(dests.back().begin(), dests.back().end(), reg) - dests.back().begin()]);
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
							dest_nodes_map.emplace_back(dest_nodes_map.back());
						}

					}

				}

				/* Used for main skips arguments set. */
				if (dont_set) {
					return;
				}

				std::size_t max = 0u; /* Find max. */
				const auto min = 0; /* Args are on bottom of stack. */

				/* Max could be wrong. See if stack is consitance if theres a hole dec and use that as max. */
				if (!dests_ns.empty()) {
					std::sort(dests_ns.begin(), dests_ns.end());
					dests_ns.erase(std::unique(dests_ns.begin(), dests_ns.end()), dests_ns.end());
					std::sort(dests_ns.begin(), dests_ns.end());
				}

				/* Check for holes. */
				std::int16_t hole_target = 0;
				if (!dests_ns.empty()) {
				
					/* Max hole in dest. */
					auto start = dests_ns.front();
					for (auto i = 0u; i < dests_ns.size(); ++i) {
						if ((i + 1u) != dests_ns.size() && dests_ns[i + 1u] != (dests_ns[i] + 1u)) {
							start = dests_ns[i + 1u];
						}
					}

					if (start) {
						hole_target = start - 1;
					}

				}

				/* No dests->source */
				if (source_no_dest.empty()) {

					/* No dests */
					if (dests_ns.empty()) {
						return;
					}

					/* Nothing */
					if (!hole_target) {
						return;
					}
					
					/* Check front for hole. */
					if (!std::binary_search(source_no_dest.begin(), source_no_dest.end(), hole_target)) {
						max = hole_target;
					}

				}
				else {

					/* Remove dupes */
					std::sort(source_no_dest.begin(), source_no_dest.end());
					source_no_dest.erase(std::unique(source_no_dest.begin(), source_no_dest.end()), source_no_dest.end());

					max = *std::max_element(source_no_dest.begin(), source_no_dest.end());
			
					/* Check for holes. */
					if (hole_target) {

						/* Check front for hole. */
						if (!std::binary_search(source_no_dest.begin(), source_no_dest.end(), hole_target)) {
							max = hole_target;
						}

					}

				}


				/* Set args (fill stack) */
				for (auto reg = min; reg <= max; reg++)
					proto->arg_regs.emplace_back(std::make_pair(reg, emitter::create::locvar_name(ast->transpiler_config->arg_prefix, reg, ast->transpiler_config->arg_suffix_char)));


				/* ... */
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
			for (auto& proto : ast->protos) {

				if (proto->arg_regs.empty()) {

					#if display_analysis
							std::printf("[AST] For routines for children proto.\n");
					#endif
					ast_funcs::loops::set_for_routines(proto); /* Add forloops */

					#if display_analysis
							std::printf("[AST] For prep for children proto.\n");
					#endif
					ast_funcs::loops::set_for_prep_exprs(proto);

					#if display_analysis
							std::printf("[AST] Setting args.\n");
					#endif
					set_args(proto);

				}

			}

			return;
		}

	}

	namespace tables {

		/* Sets table exprs. */
		void set_table_exprs(std::shared_ptr<ast_dec::ast>& ast) {

			const auto all = ast->main_block->visit_all();
			for (const auto& i : all) {

				switch (i->lex->dissassembly->op) {

				    case LuauOpcode::LOP_DUPTABLE:
					case LuauOpcode::LOP_NEWTABLE: {
						debug_success("Setting table expr at: %s", i->str().c_str());
						i->add_expr<ast_dec::expr_type::table>();
						break;
					}

					default: {
						break;
					}

				}

			}

			return;
		}

		/* Sets table elements. */
		void set_routines(std::shared_ptr<ast_dec::ast>& ast) {

			std::vector<std::uintptr_t> ends; /* Used to avoid encapsulation with tables as this is for ends of already analyzed tables. */

			const auto tables = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_expr<ast_dec::expr_type::table>(true));
			for (const auto& table : tables) {
			
				/* Check */
				bool valid = true;
				for (const auto i : ends) 
					if (table->lex->dissassembly->addr <= i)
						valid = false;
				

				if (!valid)
					continue;


				std::uint16_t table_reg = 0u; /* Current register table target. */
				std::uint32_t nested_count = 0u; /* Used for node analysis. */
				std::uintptr_t node_size = 0u;
				std::uintptr_t array_size = 0u; 
				std::uintptr_t predicted_size = 0u; /* Previous power of 2 for array_size, gives us more context on end. */


				/* Sizes for scopes of table. (Used for scopes of nested tables) */
				std::vector<std::uintptr_t> predicted_sizes;
				std::vector<std::uintptr_t> node_sizes;
				std::vector<std::uintptr_t> array_sizes;
				std::vector<std::uint16_t> table_target;
				std::vector<std::uint16_t> table_target_uf; /* Unfinished reg set table set when hit table end. */
				std::unordered_map<std::uint16_t /* reg */, std::shared_ptr<ast_dec::node> /* node */> dest_map;

				std::shared_ptr<ast_dec::node> last_set = table;



				/* Cache data for scopes. */
				auto cache = [&](const bool start /* Start new cache or set old cache? */) mutable -> void {

					if (start) {
						predicted_sizes.emplace_back(predicted_size);
						node_sizes.emplace_back(node_size);
						array_sizes.emplace_back(array_size);
						table_target.emplace_back(table_reg);
					}
					else {

						predicted_sizes.pop_back();
						node_sizes.pop_back();
						array_sizes.pop_back();
						table_target.pop_back();

						/* Check size */
						if (!predicted_sizes.empty())
							predicted_size = predicted_sizes.back();

						if (!node_sizes.empty())
							node_size = node_sizes.back();

						if (!array_sizes.empty())
							array_size = array_sizes.back();

						if (!table_target.empty())
							table_reg = table_target.back();

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
				
							if (x) {
								--x;
								x = x | (x >> 1);
								x = x | (x >> 2);
								x = x | (x >> 4);
								x = x | (x >> 8);
								x = x | (x >> 16);
								predicted_size = x - (x >> 1);
							}

							table_reg = node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;

							debug_result("Set size for %s, node size %" PRIuPTR ", array size %" PRIuPTR ", predicted size %" PRIuPTR ", target %d", node->str().c_str(), node_size, array_size, predicted_size, table_reg);

							cache(true);
							node->add_expr<ast_dec::expr_type::table_start>();
							debug_success("Added table start expr too %s", node->str().c_str());

							break;
						}

						case LuauOpcode::LOP_DUPTABLE: {
							
							const auto kvalue = node->lex->dissassembly->operands[1]->k_value;
							auto kval = kvalue.substr(kvalue.find_last_not_of("0123456789") + 1u);

							if (!std::all_of(kval.begin(), kval.end(), ::isdigit)) {
								throw std::runtime_error("All chars in kval is not digits.");
							}

							const auto table = gco2h(ast->p->k[std::stoi(kval)].value.gc);

							/* Opposite to newtable vise versa. */
							auto x = table->lsizenode;
							const auto pow = 1 << x;

							if (pow) {
								predicted_size = std::pow(std::log2(pow) - 1u, 2u); /* Last too next power of too from pow. */
							}

							array_size = table->sizearray;
							node_size = pow; 
							table_reg = node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;

							debug_result("Set size for %s, node size %" PRIuPTR ", array size %" PRIuPTR ", predicted size %" PRIuPTR ", target %d ", node->str().c_str(), node_size, array_size, predicted_size, table_reg);

							cache(true);
							node->add_expr<ast_dec::expr_type::table_start>();
							debug_success("Added table start expr too %s", node->str().c_str());

							break;
						}

						case LuauOpcode::LOP_SETLIST: {

							const auto dest = node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;
							auto amt = node->lex->dissassembly->operands[2]->val;
							if (amt == LUA_MULTRET) {
								amt = generic::fix_mulret(ast, node->address);
							}
							
							array_size -= std::uintptr_t (amt);
	

							/* Target */
							if (std::find (table_target_uf.begin (), table_target_uf.end(), dest) != table_target_uf.end()) {

								for (auto i = 0u; i < std::count(table_target_uf.begin(), table_target_uf.end(), dest); ++i) {
									node->add_expr<ast_dec::expr_type::table_end>();
									cache(false);
									--nested_count;
									debug_success("Added extra table end expr.");
								}

							}
							

							/* Could be set by end if end? */
							if (node_sizes.size() /* Base line must have members. */) {
								node->add_expr<ast_dec::expr_type::table_end>();
								ends.emplace_back(node->address);
								cache(false);
							}
		
							debug_result("Decreased array size for %s, array size %" PRIuPTR, node->str().c_str(), array_size);
							debug_success("Added table end expr too %s", node->str().c_str());
				
							break;
						}

						default: {
							break;
						}

					}

					return;
				};
				
				
				const auto inside_call_routine = ast->main_block->inside_routine<ast_dec::expr_type::call_mulret_start, ast_dec::expr_type::call_mulret_end>(table->address);
		
				/* Add first info */
				if (table->has_expr(ast_dec::expr_type::table)) {
					
					/* Has table members? */
					set_size(table);
					
					if (array_size || node_size) {

						auto nodes = ast->main_block->visit_rest(table->address);
						for (auto& node : nodes) {				
							
							/* Map out dest */
							if (!inside_call_routine && node->lex->has_operand_expr<lexer_dec::operand_types::dest>()) {

								const auto dest = node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;
								
								if (dest_map.find(dest) == dest_map.end()) {
									dest_map.insert(std::make_pair(dest, node)); /* Add */
								}
								else {
									dest_map[dest] = node; /* Change */
								}

							}

							/* New/set table instruction inc/dec for nested. */
							if (node->has_expr(ast_dec::expr_type::table)) {

								++nested_count;
								debug_line("Hit table increased nested count too %d from %s", nested_count, node->str().c_str());

							}
							else if (node->lex->type == lexer_dec::inst_type::set_table) {

								--nested_count;
								debug_line("Hit set table decreased nested count too %d from %s", nested_count, node->str().c_str());

							}

							set_size(node);
			
							/* Node_size equals predicted_size or is less then predicted_size that means that it exceeded predicted_size. */
							if (node_size) {

								/* Table set so check if it's the end. */
								if (node->lex->type == lexer_dec::inst_type::table_set) {
									
									debug_result("Current table %s, node size %" PRIuPTR ", array size %" PRIuPTR ", predicted size %" PRIuPTR ", target %d, nested %d", node->str().c_str(), node_size, array_size, predicted_size, table_reg, nested_count);

									/* Double check new set table. */
									if (node->lex->operand_expr<lexer_dec::operand_types::reg>().front()->reg != table_reg) {

										debug_line("Current reg and table target does not match.");

										--node_size; /* Dec for node. */
										node->add_expr<ast_dec::expr_type::table_end>(); /* End of table. */
										last_set = node;
										cache(false); /* Revert back cache. */

										debug_success("Added table end expr too %s", node->str().c_str());

										/* End of a nested table. */
										if (nested_count) {
											--nested_count;
										}
										else {  /* Nested table is 0 and we entered a new table. */
											break;
										}

									}
									else {

										/* Check next from last set. */
										const auto source = node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg;
										if (dest_map.find(source) != dest_map.end() && node != ast->main_block->visit_next(dest_map[source])) {

											debug_warning("Dest set is not next from node %s and set %s", node->str().c_str(), ast->main_block->visit_next(dest_map[source])->str().c_str());

											node = dest_map[source];
											goto node_end;
										}


										/* Dec not predicted. */
										if (predicted_size < (node_size - 1u)) {

											--node_size;
											node->add_expr<ast_dec::expr_type::table_element>();
											last_set = node;
											debug_line("Decreased node size too %" PRIuPTR " from %s", node_size, node->str().c_str());

										}
										else {

											std::uint32_t routine = 0u;

											/* Really just visiting future instructions too see if table source, dest, or idx gets set twice indicating end. */
											const auto rest_nodes = ast->main_block->visit_rest(node->address);
											for (const auto& i : rest_nodes) {

												/* Log routines */
												routine_inc_basic(i, routine);
												routine_dec_basic(i, routine);

												/* Skip */
												if (routine) {
													continue;
												}

												/* End (new/end nested table) */
												if (nested_count) {

													if (i->lex->type == lexer_dec::inst_type::set_table) {

														debug_line("Hit setlist for nested table set on %s", i->str().c_str());

														node->add_existance<ast_dec::expr_type::table_element>();
														last_set = node;
														last_set = i;

														cache(false); /* Revert back cache. */

													}
													else if (i->has_expr(ast_dec::expr_type::table)) {

														debug_line("Hit new forming nested table set on %s", i->str().c_str());

														node->add_existance<ast_dec::expr_type::table_element>();
														last_set = node;

													}

													break;
												}

												if (i->lex->has_operand_expr<lexer_dec::operand_types::dest>() && predicted_size > node_size) {

													debug_line("Hit dest from predicted on %s for %s", i->str().c_str(), node->str().c_str());

													/* Dest is logical out of table. */
													if (regs::logical_dest_register(ast, i, i->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg)) {
														
														debug_warning("Reg is logical end.");

														node->add_existance<ast_dec::expr_type::table_element>();
														last_set = node;
														node_size = 0;
														goto node_end;
													}

												}
												else {

													/* Current is table Set. */
													if (i->lex->type == lexer_dec::inst_type::table_set) {

														debug_line("Hit table set on %s for %s", i->str().c_str(), node->str().c_str());

														/* Table has same table source as current table source valid element.*/
														if (table_reg == i->lex->operand_expr<lexer_dec::operand_types::reg>().front()->reg) {

															debug_line("Same table source as current valid element.");

															--node_size; /* Dec for node. */
															node->add_existance<ast_dec::expr_type::table_element>();
															last_set = node;

															break;
														}
														else { /* New table */

															if (nested_count) {

																/* Inside nested */

																debug_line("New nested table.");

																--node_size; /* Dec for node. */

																node->add_existance<ast_dec::expr_type::table_element>();
																last_set = i;
																cache(false); /* Revert back cache. */

																if (array_size) {

																	table_target_uf.emplace_back(table_reg);
																	debug_warning("Current table still has array size apppended reg to cache for later set on %s", i->str().c_str());

																}
																else {
																	i->add_expr<ast_dec::expr_type::table_end>(); /* End of table. */

																	/* Dec nested table */
																	--nested_count;
																	debug_success("Added table end expr too %s", i->str().c_str());

																}

															}
															else {

																debug_line("New table.");

																/* End */
																if (array_size) {

																	table_target_uf.emplace_back(table_reg);
																	debug_warning("Array size still not 0, cached table reg target.");

																}
																else {
																	node->add_existance<ast_dec::expr_type::table_element>();
																	last_set = node;
																	node_size = 0;
																	goto node_end;
																}

															}

															break;
														}

													}
													else if (no_dest(i)) {
														/* Something different */

														debug_warning("Jumping too end because of not table set or dest on %s", i->str().c_str());

														node->add_existance<ast_dec::expr_type::table_element>();
														last_set = node;
														node_size = 0;
														goto node_end;
													}

												}

											}

											/* Fail case */
											if (!node->has_expr(ast_dec::expr_type::table_element)) {
												node->add_existance<ast_dec::expr_type::table_element>();
												last_set = node;
											}

										}

									}

								}
								else if (no_dest(node)) {

									debug_warning("No dest adbrupt end for %s", node->str().c_str());
									node = last_set;

									goto node_end;
								}

							}

							/* Found end, end anlysis. */
							if (!node_size && !array_size) {
							node_end:
								debug_success("Adding table end expr too %s", node->str().c_str());
								node->add_expr<ast_dec::expr_type::table_end>();
								ends.emplace_back(node->address);
								cache(false); /* Revert back cache. */
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

			std::cout << "RET " << ast->tree_str() << std::endl;
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

					node->add_existance<ast_dec::expr_type::locvar>();
					
					/* Inc for call if. */
					if (node->lex->type == lexer_dec::inst_type::call) {

						auto retn = node->lex->operand_expr<lexer_dec::operand_types::integer>().back()->val;

						if (retn == LUA_MULTRET) {
							retn = generic::fix_mulret(ast, node->address);
						}
						
						if (retn > 1) {
							node->dest_loc.multret_amount = retn;
							node->add_existance<ast_dec::expr_type::locvar_multret>();
						}

						registers.back() += retn;
						
					}
					else {
						++registers.back();
					}

					/* Set names */
					if (node->dest_loc.multret_amount) {

						/* Append to dest. (Fill for multiple retn) */
						for (auto i = operand->reg; i < (operand->reg + node->dest_loc.multret_amount); ++i) {

							node->dest_loc.multret_names.emplace_back(emitter::create::locvar_name(ast->transpiler_config->variable_prefix, i, ast->transpiler_config->var_suffix_char));

						}

					}
					else {
						node->dest_loc.name = emitter::create::locvar_name(ast->transpiler_config->variable_prefix, registers.back(), ast->transpiler_config->var_suffix_char);
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
						if (capture->lex->has_operand_expr<lexer_dec::operand_types::source>() && capture->lex->operand_expr<lexer_dec::operand_types::source>().front ()->capture_reg == dest->reg) {
								#if lv_regs
							if (dest->reg == registers.back()) {
								#endif

								node_var(dest);

								#if lv_regs
							}
								#endif
							bad = true;
							break;
						}
					if (bad)
						continue;
					 
					
					/* Check concat and call routines if the dest is used as a dest in them no locvar. */
					const auto calls = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_next_expr_scope<ast_dec::expr_type::call_routine_start>(node->address, true));
					for (const auto& call : calls) {

						const auto node_end = ast->main_block->visit_relative_next_expr_scope_current<ast_dec::expr_type::call_routine_end>(call->address, { ast_dec::expr_type::call_routine_start });
					
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
						
						const auto node_end = ast->main_block->visit_relative_next_expr_scope_current<ast_dec::expr_type::concat_routine_end>(concat->address, { ast_dec::expr_type::concat_routine_start });
					
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


						#if lv_regs
					if (dest->reg == registers.back()) {				
						#endif	


						/* Check mulret */
						bool logical_mult = false;
						if (node->lex->type == lexer_dec::inst_type::call) {

							auto retn = node->lex->operand_expr<lexer_dec::operand_types::integer>().back()->val;

							if (retn == LUA_MULTRET) {
								retn = generic::fix_mulret(ast, node->address);
							}

							if (retn > 1) {

								/* Check all multrets */
								for (auto i = dest->reg; i < (dest->reg + retn); i++)
									if (ast_funcs::regs::logical_dest_register(ast, node, i)) {
										logical_mult = true;
									}

							}

						}
				
						/* See if dest register is logical. */
						if (logical_mult || ast_funcs::regs::logical_dest_register(ast, node, dest->reg)) {
					
							/* Mutate global */
							if (node->has_expr(ast_dec::expr_type::closure_global)) {
							
								node->replace_next<ast_dec::expr_type::closure_global, ast_dec::expr_type::closure_local>();
								node->remove_all_expr <ast_dec::expr_type::closure_global>();

								ast->protos[node->closure_extra.closure_idx]->closure_type = ast_dec::closure_type::local;
								
								if (node->closure_extra.setglobal_node != nullptr) {
									node->closure_extra.setglobal_node->remove_all_expr <ast_dec::expr_type::dead_instruction>();
								}

								emitter::override::locvar_name(ast->protos[node->closure_extra.closure_idx]->closure_name, ast->transpiler_config->function_prefix, registers.back()++, ast->transpiler_config->function_suffix_char);
					
							}
							else {							
								node_var(node->lex->dissassembly->operands.front());
							}

							/* Has indexes, remove dead instructions. */
							if (node->closure_extra.idx_nodes.first != nullptr) {

								const auto range = ast->main_block->visit_range_current(node->closure_extra.idx_nodes.first->address, node->closure_extra.idx_nodes.second->address);
								for (const auto& i : range) {
									i->remove_all_expr<ast_dec::expr_type::dead_instruction>();
								}

							}

						}

						continue;
					

						#if lv_regs
					}
						#endif

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

						if (jmp > end && (jump_out == nullptr || jmp >= jump_out->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr)) {

							/* Check too see if no break and no jump out before. */
							if (!node->has_expr(ast_dec::expr_type::break_)) {

								/* Make sure no jumps from start -> node current. */
								bool valid = true;
								const auto range_ = ast->main_block->visit_range_current(node->address, end);
								for (const auto& i : range_) {
									
									if (i->lex->dissassembly->op == LuauOpcode::LOP_JUMP) {
										valid = false;
										break;
									}

								}

								if (valid) {
									jump_out = node;
								}

							}

						}

					}

				}
			
				return jump_out;
			};
			

			auto all = ast->main_block->visit_all();
			for (auto& i : all) {

				/* Loadb or either branch condition. */
				const auto i_next = ast->main_block->visit_next(i); // (i->lex->dissassembly->op == LuauOpcode::LOP_LOADB && i_next != nullptr) || 
				if (i->lex->type == lexer_dec::inst_type::branch_condition) {

					auto cached_init = i; /* Mutable by logical operations. */
					std::vector<std::uint16_t> compares; /* Singular loadb compare(jmp 1+; loadb r1 +1; loadb r1 0; ???) jumps log registers and see if it gets used in compare first. */
					std::vector<std::uint16_t> compares_double; /* Double compare(jmp 3+ loadb r1,+1; loadb r1 0; loadb r2, 0; ???) */
					bool node_nit_b = false;
					std::shared_ptr<ast_dec::node> node_hit = nullptr; /* Prevents infinite looping if something goes wrong. */

					/* Make sure it hasnt already been analyzed. */
					if (!i->has_expr(ast_dec::expr_type::condition_logical_start) && !i->has_expr(ast_dec::expr_type::condition_logical) && !i->has_expr(ast_dec::expr_type::condition_logical_end) && !i->has_expr(ast_dec::expr_type::condition_concat_member) && !i->has_expr(ast_dec::expr_type::condition_concat_start) && !i->has_expr(ast_dec::expr_type::condition_concat_end)) {

						/* Operation could be logical expression? */
						bool predicted_logical = false;

						do {
							
							debug_success("Starting with node %s", i->str().c_str());

							/* Attempts to prevent bugs. */
							if (node_hit != i) {
								node_hit = i;
								node_nit_b = false;
							}
							else {
								
								/* Looped over 3 on the same node. */
								if (node_nit_b) {
									debug_warning("Looped infinitly for %s", i->str().c_str());
									#if display_warnings 				
										std::printf("[WARNING] Looped infinitely on %s for logical conditions.\n", i->lex->dissassembly->data.c_str());
									#endif
									break;
								}

								node_nit_b = true;
							}

							/* Get next branch jump. */
							if (i->lex->dissassembly->op == LuauOpcode::LOP_LOADB) {
								i = ast->main_block->visit_next(i);
								debug_line("Mutated loadb i node too %s", i->str().c_str());
							}

							/* No compare routine */
							if (!i->has_expr(ast_dec::expr_type::condition_routine_start) && !i->has_expr(ast_dec::expr_type::condition_routine_end) && !i->has_expr(ast_dec::expr_type::condition_routine)) {
								debug_warning("No compare routine for %s", i->str().c_str());
								break;
							}
						
							/* Get next compare jump if current isnt one. */
							if (i->lex->type != lexer_dec::inst_type::branch_condition) {
								i = std::get<std::shared_ptr<ast_dec::node>>(ast->main_block->visit_next_type_addr<lexer_dec::inst_type::branch_condition>(i->address, false));
								debug_line("Current wasn't compare changed too %s", i->str().c_str());
							}
						
							/* Not a compare branch something went wrong. */
							if (i == nullptr || i->lex->type != lexer_dec::inst_type::branch_condition) {
								debug_warning("Current is not a compare.");
								break;
							}

							
							auto current_jmp = i->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr;


							/* Check range for return if there is a return if else return not also if concat. */
							bool retn = false;
							const auto range = ast->main_block->visit_range(i->address, current_jmp);
							for (const auto& i : range) {
								if (i->has_expr(ast_dec::expr_type::return_)) {
									retn = true;
									break;
								}
							}
							/* Not concat */
							if (retn) {
								debug_warning("Has return, not concat.");
								break;
							}


							/* Check next jump and current for different breaks. */
							const auto next_jmp = std::get<std::shared_ptr<ast_dec::node>>(ast->main_block->visit_next_type_addr<lexer_dec::inst_type::branch_condition>(i->address, false));
							if (next_jmp != nullptr) {

								const auto jmp_prev = ast->main_block->visit_previous_addr(current_jmp);
								const auto next_jmp_prev = ast->main_block->visit_previous_addr(next_jmp->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr);
					
								if (jmp_prev != nullptr && next_jmp_prev != nullptr && condition_break(jmp_prev) && condition_break(next_jmp_prev)) {

									/* Not same jump */
									if (jmp_prev->address != next_jmp_prev->address) {
										debug_warning("Next is break out.");
										break;
									}

								}

							}

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
								debug_success("Jump out found for %s", i->str().c_str());
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
									
									debug_line("Compare count isn't 2.");

									const auto next = ast->main_block->visit_next(i);

									/* Nothing */
									if (next == nullptr) {
										debug_warning("Next is nullptr.");
										break;
									}

									if (next->lex->dissassembly->op == LuauOpcode::LOP_LOADB) {
										debug_line("Next is loadb.");
										continue;
									}

								}

								const auto next = ast->main_block->visit_next(i);
								
								/* Nothing*/
								if (next == nullptr) {
									debug_warning("Next is nullptr.");
									break;
								}
								
								/* Next has loadb with jump. */
								if (next->lex->dissassembly->op == LuauOpcode::LOP_LOADB && next->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp) {

									debug_line("Next is loadb with jump on %s", next->str().c_str());

									/* Singular */
									if (current_jmp == (next->address + next->lex->dissassembly->len)) {
										
										debug_success("Current jump = next.");

										compares.push_back(next->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg);

										/* Exceeded max of 2 routine is something else. */
										if (compares.size() > 2u) {
											debug_warning("Compares exceeded max of 2.");
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

									debug_line("Previous is loadb with %s", prev->str().c_str());

									/* Doesnt have jump check previous. */
									if (!prev->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp) {

										debug_line("Previous doesn't have jump");

										const auto p_prev = ast->main_block->visit_previous_addr(prev->address);

										/* Has previous loadb jump.  */
										if (p_prev->lex->dissassembly->op == LuauOpcode::LOP_LOADB && p_prev->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp) {
											
											debug_line("Prev-Previous is loadb with jump.");

											compares_double.emplace_back(p_prev->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg);

											/* Exceeded max of 2 routine is something else. */
											if (compares_double.size() > 2u) {
												debug_warning("Compares exceeded max of 2.");
												break;
											}

											/* Visit jump */
											i = ast->main_block->visit_addr(current_jmp);
										
											continue;
										}
										else {
											debug_line("Prev-Previous is not loadb with jump.");
											break;
										}

									}
									else {

										
									}

								}
								else {
								
									/* Analyze range of start current and jump. */
									auto next_inst = ast->main_block->visit_next(i);
									const auto next_jmp = std::get<std::shared_ptr<ast_dec::node>>(ast->main_block->visit_next_type_addr<lexer_dec::inst_type::branch_condition>(i->address, false));
									

									/* No next end. */
									if (next_inst == nullptr) {
										debug_warning("Next instruction is nullptr.");
										break;
									} else if (next_jmp == nullptr) {

										debug_warning("Next jump is nullptr.");

										/* Still could be logical exprssion. */

										/* Logical operation. */
										if (ast->main_block->filled(i, prev)) {

											debug_result("Current - previous is filled %s - %s", i->str().c_str(), prev->str().c_str());

											i = ast->main_block->visit_addr(current_jmp);
											debug_line("Current - previous is filled, current changed too %s", i->str().c_str());

											if (cached_init->lex->type == lexer_dec::inst_type::branch_condition) {
												cached_init = ast->main_block->visit_previous_addr(cached_init->address);
												debug_result("Cached changed too %s", cached_init->str().c_str());
											}

											predicted_logical = true;
											debug_success("Predicted logical is true.");

										}
									
										break;
									}
								
								
									/* Jump data */
									const auto next_jmp_target = next_jmp->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr;
									const auto next_jmp_target_node = ast->main_block->visit_addr(next_jmp_target);
									

									/* Jump too same address possibly or? */
									if (current_jmp == next_jmp->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr) {					
										
										debug_line("Current jump = next jump on %s", next_jmp->str().c_str());

										/* No compare routine */
										if (!next_inst->has_expr(ast_dec::expr_type::condition_routine_start) && !next_inst->has_expr(ast_dec::expr_type::condition_routine_end) && !next_inst->has_expr(ast_dec::expr_type::condition_routine)) {
										
											debug_line("No compare routine for %s", next_inst->str().c_str());

											/* Logical operation. */
											if (ast->main_block->filled(i, ast->main_block->visit_previous_addr(next_jmp->address))) {

												debug_success("Next jump and current is filled.");

												i = next_jmp;

												if (cached_init->lex->type == lexer_dec::inst_type::branch_condition) {
													cached_init = ast->main_block->visit_previous_addr(cached_init->address);
												}

												predicted_logical = true;

											}
											else {
												debug_warning("Next jump and current is not filled.");
												break;
											}

										}
										else {						

											/* Has compare routine following next. */
											i = next_jmp;
											debug_line("Current mutated too %s", i->str().c_str());

											/* Maybe logical operation? */
											if (i->lex->type == lexer_dec::inst_type::branch_condition) {
												
												debug_line("Current is branch condition.");

												const auto jmp_target_next = i->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr;
									
												/* Logical operation */
												if (ast->main_block->filled(i, ast->main_block->visit_previous_addr(jmp_target_next))) {

													debug_success("Previous jump and current is filled.");

													i = ast->main_block->visit_previous_addr(jmp_target_next);

													if (cached_init->lex->type == lexer_dec::inst_type::branch_condition) {
														cached_init = ast->main_block->visit_previous_addr(cached_init->address);
													}

													predicted_logical = true;

												}

											}

										}

										continue;
									}
								
									/* Next is loadb check also next. */
									if (next_inst->lex->dissassembly->op == LuauOpcode::LOP_LOADB) {
										next_inst = ast->main_block->visit_next(next_inst);
										debug_line("Next instuction mutated too %s", next_inst->str().c_str());
									}
								
									/* Logical operation. */
									if (ast->main_block->filled(i, prev)) {
										
										debug_success("Current and previous is filled for %s", prev->str().c_str());

										i = ast->main_block->visit_addr(current_jmp);

										if (cached_init->lex->type == lexer_dec::inst_type::branch_condition) {
											cached_init = ast->main_block->visit_previous_addr(cached_init->address);
										}

										predicted_logical = true;

									}
									
									/* Logical operation. */
									if (ast->main_block->filled(i, ast->main_block->visit_previous_addr(next_jmp_target))) {

										/* Check too see if regs gets used. */
										debug_success("Current and previous instruction is filled.");

										i = ast->main_block->visit_addr(current_jmp);

										if (cached_init->lex->type == lexer_dec::inst_type::branch_condition) {
											cached_init = ast->main_block->visit_previous_addr(cached_init->address);
										}

										predicted_logical = true;

									}

									/* Next leads too routine */
									if (next_inst->has_expr(ast_dec::expr_type::condition_routine_start) || next_inst->has_expr(ast_dec::expr_type::condition_routine_end) || next_inst->has_expr(ast_dec::expr_type::condition_routine)) {
										
										/* Check loadbs */

										const auto next = ast->main_block->visit_addr(i->address + i->lex->dissassembly->len);

										/* Nothing */
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


									/* Check all jump conditions within jump and see if any hit jump to current jump target and all conditions are all filled. */
									const auto cond_jumps = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_type_next_addr<lexer_dec::inst_type::branch_condition>(true, i->address));
									for (const auto& jmp : cond_jumps) {

										const auto jmp_target = jmp->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr;
										const auto next = std::get<std::shared_ptr<ast_dec::node>>(ast->main_block->visit_next_type_addr<lexer_dec::inst_type::branch_condition>(jmp->address, false));

										/* No next jump. */
										if (next == nullptr || next->address >= jmp_target) {
											break;
										}



									}


									break; 
								}
								
							}

			
						} while (true);
				
						cached_init->add_expr<ast_dec::expr_type::condition_logical_start>();
						debug_result("Cached init added expr conditon logical start on %s", cached_init->str().c_str());
						debug_line("Current is currently %s", i->str().c_str());

						/* Fixed i. */
						if (!predicted_logical && (i->lex->dissassembly->op != LuauOpcode::LOP_LOADB || i->lex->type != lexer_dec::inst_type::branch_condition)) {
						
							auto prev = ast->main_block->visit_previous_addr(i->address);
							prev = (prev != nullptr) ?  ((cached_init->address < prev->address) ? prev : i) : i;
						
							if (prev->lex->dissassembly->op == LuauOpcode::LOP_LOADB || prev->lex->type == lexer_dec::inst_type::branch_condition) {
							
								i = prev;
								debug_result("Mutated current too %s", i->str().c_str());

							}

						}

						if (predicted_logical) {

							debug_line("Is predicted logical.");

							i->add_expr<ast_dec::expr_type::conditional_expression_predicted>();
							debug_success("Added conditional expression predicted too current on %s", i->str().c_str());

							/* Fix i too previous instruction from jump. */
							const auto prev_cond = ast->main_block->visit_prev_type_current<lexer_dec::inst_type::branch_condition>(i->address);
							if (prev_cond != nullptr && i->address == prev_cond->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr) {

								i = ast->main_block->visit_previous_addr(i->address);
								debug_result("Mutated current for predicted logical too %s", i->str().c_str());

							}

						}


						debug_line("Adding condition logical exprs too members range from %s - %s", cached_init->str().c_str(), i->str().c_str());

						/* Members */
						const auto range = ast->main_block->visit_range(cached_init->address, i->address);
						for (const auto& i : range)
							i->add_expr<ast_dec::expr_type::condition_logical>();

						/* End */
						i->add_expr<ast_dec::expr_type::condition_logical_end>();	
						debug_success("Added condition logical end expr too %s", i->str().c_str());

					}

				}

			}

			return;
		}


		/* Sets logical expressions. */
		void set_logical_expression(std::shared_ptr<ast_dec::ast>& ast) {

			std::int32_t count = 0u;
			std::shared_ptr<ast_dec::node> start_node = nullptr;
			std::shared_ptr<ast_dec::node> target_cond = nullptr;

			const auto all = ast->main_block->visit_all();
			for (const auto& i : all) {


				auto set = [&]() mutable -> void {

					debug_success("Setting logical expression start %s, end %s ", start_node->str().c_str(), i->str().c_str());

					/* Set start */
					start_node->add_existance<ast_dec::expr_type::conditional_expression_start>();
					start_node->add_existance<ast_dec::expr_type::condition_concat_start>();

					/* Set ends */
					i->add_existance<ast_dec::expr_type::condition_concat_end>();
					i->add_existance<ast_dec::expr_type::conditional_expression_end>();
					i->add_existance<ast_dec::expr_type::condition_append_source>();

					/* Remove LOP_LOADB dead expr. */
					i->remove_expr<ast_dec::expr_type::dead_instruction>();

					/* Set logical operations. */
					ast_funcs::branches::set(ast, start_node->address, i->address, { target_cond->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr }, -1, true, true);

					/* Remove emitted next */
					target_cond->remove_expr<ast_dec::expr_type::condition_emit_next>();

				};


				if (i->has_expr(ast_dec::expr_type::condition_logical_start)) {

					debug_line("Current node has logical start %s", i->str().c_str());

					/* Set start */
					if (!count) {
						start_node = i;
					}

					++count;
					debug_result("Increased count too %d", count);;
				}


				if (count != 1u && i->has_expr(ast_dec::expr_type::condition_logical_end)) {

					debug_line("Decreasing count because current node has logical end expr on %s", i->str().c_str());
					--count;
					debug_result("Decreased count too %d", count);

				}
				else if (i->has_expr(ast_dec::expr_type::condition_logical_end)) {

					debug_line("Current node has condition logical end expr on %s", i->str().c_str());

					auto target = i;
					const auto loadb = i->lex->dissassembly->op == LuauOpcode::LOP_LOADB;
					
					if (!i->has_expr(ast_dec::expr_type::conditional_expression_predicted)) {

						debug_line("Current node doesn't have conditional expression predicted on %s", i->str().c_str());

						/* See if loadb with jump exists if so get it. */
						if (!target->lex->has_operand_expr<lexer_dec::operand_types::memaddr>() || !target->lex->operand_expr<lexer_dec::operand_types::memaddr>().back()->val) {
							debug_line("Target doesnt have jump operand on %s", target->str().c_str());
							target = ast->main_block->visit_previous_addr(target->address);
						}

						/* Not expression */
						if (target == nullptr || (target->lex->dissassembly->op != LuauOpcode::LOP_LOADB && target->lex->type != lexer_dec::inst_type::branch_condition)) {
							
							debug_line("Target is nullptr, not loadb, or branch condition on %s", target->str().c_str());
							--count;
							debug_result("Decreased count too %d", count);

							continue;
						}

						if (!target->lex->operand_expr<lexer_dec::operand_types::memaddr>().back()->val) {

							debug_line("Target doesn't jump on %s", target->str().c_str());
							--count;
							debug_result("Decreased count too %d", count);

							continue;
						}

						target_cond = (target->lex->type == lexer_dec::inst_type::branch_condition) ? target : ast->main_block->visit_previous_addr(target->address);

						/* Not expression */
						if (target_cond == nullptr || target_cond->lex->type != lexer_dec::inst_type::branch_condition) {
							debug_result("Target condition is nullptr or isnt a branch condition.");
							--count;
							debug_result("Decreased count too %d", count);
						}
						else if ((loadb && target_cond->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr == i->address) || (!loadb && target_cond->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr == (i->address + i->lex->dissassembly->len))) {

							debug_line("Target condition is expected loadb with jump or expected branch compare on %s", target->str().c_str());
							
							if (!(--count)) {
								set();
							}

							debug_result("Decreased count too %d", count);
						}
						else {

							debug_warning("Nothing hit decreasing count.");
							--count;
							debug_result("Decreased count too %d", count);

						}

					}
					else {

						debug_line("Current node does have conditional expression predicted on %s", i->str().c_str());

						/* Get previous from address with type of branch compare. */
						target_cond = ast->main_block->visit_prev_type_current<lexer_dec::inst_type::branch_condition>(i->address);

						/* Shouldn't happen but incase it does. */
						if (target_cond == nullptr) {
							throw std::runtime_error("target_cond returned nullptr.");
						}

						set();

					}

				}

			}

			return;
		}

	}

	void init_ast(std::shared_ptr<ast_dec::ast>& ast) {

		#if display_analysis
				std::printf("[AST] Return instruction(s) exprs.\n");
		#endif
		ast_funcs::instructions::set_return_exprs(ast);

		#if display_analysis
				std::printf("[AST] Setting table exprs.\n");
		#endif
		ast_funcs::tables::set_table_exprs(ast);

		#if display_analysis
				std::printf("[AST] For prep exprs.\n");
		#endif
		ast_funcs::loops::set_for_prep_exprs(ast);

		#if display_analysis
				std::printf("[AST] Setting for routines.\n");
		#endif
		ast_funcs::loops::set_for_routines(ast);

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
				std::printf("[AST] Setting arguments for children proto.\n");
		#endif
		ast_funcs::arguments::set(ast);

		#if display_analysis
				std::printf("[AST] Setting logical operations.\n");
		#endif
		ast_funcs::locvars::set_logical_operations(ast);

		#if display_analysis
				std::printf("[AST] Setting logical expressions.\n");
		#endif
		ast_funcs::locvars::set_logical_expression(ast);

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
				std::printf("[AST] Sorting loops.\n");
		#endif
		ast_funcs::loops::sort_loops(ast);

		#if display_analysis
				std::printf("[AST] Setting call multret routines.\n");
		#endif
		ast_funcs::calls::set_multret_routines(ast);

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
		ast->add_block(ast->main_block);

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
							ast->add_block(jump_block);
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
							ast->add_block(jump_block);
						}

						current->branches.emplace_back(nojump_block);
						ast->add_block(nojump_block);

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
		retn->total += dism->total;
	}

	/* Set current proto blocks. */
	#if display_analysis
		std::printf("[AST] Initing main blocks.\n");
	#endif
	blocks::set_blocks(retn);


	do {

		debug_init("AST");

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
					retn->total += dism->total;
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

		debug_close("AST");

	} while (protos_ast.size());

	/* Clear cache for new asts. */
	global_cache::clear();


	return retn;
}