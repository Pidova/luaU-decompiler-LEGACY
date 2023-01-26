#include "../ast_functions.hpp"

/* Set logical operations in routine with given range. */
void ast_funcs::branches::set(std::shared_ptr<ast_dec::ast>& ast, const std::uintptr_t begin, const std::uintptr_t end, const std::vector <std::uintptr_t> dead /* Always opposite and when hit. */, const std::int16_t logical_operation_target /* Used for ignoring ands/ors. */, const bool check_loops /* Checks jumps too see if they lead too loop by expr and preform opposite. */, const bool last_include /* Includes last compare as or. */) {

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
void ast_funcs::branches::set_valid_branch_routine(std::shared_ptr<ast_dec::ast>& ast) {

		std::size_t idx = 0u;
		std::size_t routine = 0u;

		const auto conditions = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_type<lexer_dec::inst_type::branch_condition>(true));

		/* Nothing to analyze */
		if (conditions.empty()) {
			return;
		}

		auto condition = conditions[idx++]->lex->operand_expr<lexer_dec::operand_types::compare>();
		auto target_1 = condition.front()->reg;
		auto target_2 = (condition.size() > 1u) ? condition.back()->reg : -1;
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

				const auto dest = node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;

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
void ast_funcs::branches::set_branch_statements(std::shared_ptr<ast_dec::ast>& ast) {

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

						if (next_jmp == nullptr || next_jmp->address > prev_jmp->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr ||
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


