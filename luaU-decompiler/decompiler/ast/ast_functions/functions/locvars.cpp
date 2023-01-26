#include "../ast_functions.hpp"

/*

	Sets locvars based on certain hueristics. More detailed info about it can be found in ast_config.hpp at control_flow_vars macro.
	Changing that will effect the behavior of this function but all the information you need can be found there but judge your discision
	on how you want the code to be in the decompilation. This isn't 100% perfect because where mostly judging on hueristics and nothing else.
	Depending on how the code is decompiled as of this version of luaU debug information contains stuff about variables (names, etc) but there isn't
	a garunteed it will be present or not so this will try to recreat that.

*/
void ast_funcs::locvars::set_lv(std::shared_ptr<ast_dec::ast> &ast, const std::uint16_t start_reg) {

      std::vector<std::uint16_t> registers = {start_reg}; /* Registers for scope. (Based on target register) */
      std::uintptr_t routine = 0u;                        /* Inside concat, call, table routine, inc for start, dec for end. */

      for (const auto &node : ast->main_block->visit_all()) {

            /* Turns node into variable. */
            auto node_var = [&](const std::shared_ptr<LuaU_dissassembler::operand> &operand) -> void {
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

                  } else {
                        ++registers.back();
                  }

                  /* Set names */
                  if (node->dest_loc.multret_amount) {

                        /* Append to dest. (Fill for multiple retn) */
                        for (auto i = operand->reg; i < (operand->reg + node->dest_loc.multret_amount); ++i) {

                              node->dest_loc.multret_names.emplace_back(emitter::create::locvar_name(ast->transpiler_config->variable_prefix, i, ast->transpiler_config->var_suffix_char));
                        }

                  } else {
                        node->dest_loc.name = emitter::create::locvar_name(ast->transpiler_config->variable_prefix, registers.back(), ast->transpiler_config->var_suffix_char);
                  }

                  return;
            };

            /* End of scope. */
            for (auto i = 0u; i < node->count_expr<ast_dec::expr_type::scope_end>(); ++i) {
                  registers.pop_back();
            }

            /* Log reg scope start. */
            if (node->has_expr(ast_dec::expr_type::for_start) || node->has_expr(ast_dec::expr_type::for_n_start) || node->has_expr(ast_dec::expr_type::for_iv_start)) {
                  registers.emplace_back(node->loop_extra.end_node->loop_extra.end_reg + 1u);
            } else if (node->has_expr(ast_dec::expr_type::if_) || node->has_expr(ast_dec::expr_type::repeat_) || node->has_expr(ast_dec::expr_type::while_)) {
                  registers.emplace_back(registers.back());
            } else if (node->has_expr(ast_dec::expr_type::elseif_) || node->has_expr(ast_dec::expr_type::else_)) {
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
                  const auto dest = node->lex->operand_expr<lexer_dec::operand_types::dest>().front();

                  /* Fix name */
                  if (node->has_expr(ast_dec::expr_type::closure_local)) {
                        emitter::override::locvar_name(ast->protos[node->closure_extra.closure_idx]->closure_name, ast->transpiler_config->function_prefix, registers.back(), ast->transpiler_config->function_suffix_char);
                  }

                  /* Capture with source garunteeds locvar so check there. */
                  const auto captures = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst_scope<LuauOpcode::LOP_CAPTURE>(node->address, true));
                  for (const auto &capture : captures)
                        if (capture->lex->has_operand_expr<lexer_dec::operand_types::source>() && capture->lex->operand_expr<lexer_dec::operand_types::source>().front()->capture_reg == dest->reg) {
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
                  for (const auto &call : calls) {

                        const auto node_end = ast->main_block->visit_relative_next_expr_scope_current<ast_dec::expr_type::call_routine_end>(call->address, {ast_dec::expr_type::call_routine_start});

                        /* Target dest reg used in call routine dest. */
                        for (const auto &call_node : ast->main_block->visit_range(call->address, node_end->address))
                              if (call_node->lex->has_operand_expr<lexer_dec::operand_types::dest>() && call_node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg == registers.back())
                                    bad = true;
                  }
                  if (bad)
                        continue;

                  /* Concat */
                  const auto concats = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_next_expr_scope<ast_dec::expr_type::concat_routine_start>(node->address, true));
                  for (const auto &concat : concats) {

                        const auto node_end = ast->main_block->visit_relative_next_expr_scope_current<ast_dec::expr_type::concat_routine_end>(concat->address, {ast_dec::expr_type::concat_routine_start});

                        /* Target dest reg used in call routine dest. */
                        for (const auto &concat_node : ast->main_block->visit_range(concat->address, node_end->address))
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
                        } else {
                              emitter::override::locvar_name(ast->protos[node->closure_extra.closure_idx]->closure_name, ast->transpiler_config->function_prefix, registers.back()++, ast->transpiler_config->function_suffix_char);
                        }

                  } else {

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
                                          node->remove_all_expr<ast_dec::expr_type::closure_global>();

                                          ast->protos[node->closure_extra.closure_idx]->closure_type = ast_dec::closure_type::local;

                                          if (node->closure_extra.setglobal_node != nullptr) {
                                                node->closure_extra.setglobal_node->remove_all_expr<ast_dec::expr_type::dead_instruction>();
                                          }

                                          emitter::override::locvar_name(ast->protos[node->closure_extra.closure_idx]->closure_name, ast->transpiler_config->function_prefix, registers.back()++, ast->transpiler_config->function_suffix_char);

                                    } else {
                                          node_var(node->lex->dissassembly->operands.front());
                                    }

                                    /* Has indexes, remove dead instructions. */
                                    if (node->closure_extra.idx_nodes.first != nullptr) {

                                          const auto range = ast->main_block->visit_range_current(node->closure_extra.idx_nodes.first->address, node->closure_extra.idx_nodes.second->address);
                                          for (const auto &i : range) {
                                                i->remove_all_expr<ast_dec::expr_type::dead_instruction>();
                                          }
                                    }
                              }

                              continue;

#if lv_regs
                        }
#endif
                  }

            } else if (!routine && node->has_expr(ast_dec::expr_type::table_end)) { /* No routine and end of table garunteed locvar. */
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

	Sets logical operations. (Can be used for if/elseif statements).
*/
void ast_funcs::locvars::set_logical_operations(std::shared_ptr<ast_dec::ast> &ast) {

      /* Max jump out of jump compare with not being break(pre). */
      auto jump_out = [&](const std::uintptr_t start, const std::uintptr_t end) mutable -> std::shared_ptr<ast_dec::node> {
            /* See if jump inside jump jumps out. */
            std::shared_ptr<ast_dec::node> jump_out = nullptr;
            const auto range = ast->main_block->visit_range(start, end);
            for (const auto &node : range) {

                  if (node->lex->type == lexer_dec::inst_type::branch_condition) {

                        const auto jmp = node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr;

                        if (jmp > end && (jump_out == nullptr || jmp >= jump_out->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr)) {

                              /* Check too see if no break and no jump out before. */
                              if (!node->has_expr(ast_dec::expr_type::break_)) {

                                    /* Make sure no jumps from start -> node current. */
                                    bool valid = true;
                                    const auto range_ = ast->main_block->visit_range_current(node->address, end);
                                    for (const auto &i : range_) {

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
      for (auto &i : all) {

            /* Loadb or either branch condition. */
            const auto i_next = ast->main_block->visit_next(i); // (i->lex->dissassembly->op == LuauOpcode::LOP_LOADB && i_next != nullptr) ||
            if (i->lex->type == lexer_dec::inst_type::branch_condition) {

                  auto cached_init = i;                       /* Mutable by logical operations. */
                  std::vector<std::uint16_t> compares;        /* Singular loadb compare(jmp 1+; loadb r1 +1; loadb r1 0; ???) jumps log registers and see if it gets used in compare first. */
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
                              } else {

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
                              for (const auto &i : range) {
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
                                        (jump_out_n == nullptr) ? current_jmp : jump_out_n->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr);
                              } while (jump_out_temp != nullptr);

                              /* Found hueristic 1 check passthrough again. */
                              if (jump_out_n != nullptr) {
                                    i = jump_out_n;
                                    debug_success("Jump out found for %s", i->str().c_str());
                              } else {

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

                                          } else if (compares.size() == 2u) { /* Max compare is 2 */

                                                std::vector<std::uint16_t> t_vect = {compare_operands.front()->reg, compare_operands.back()->reg};

                                                std::sort(t_vect.begin(), t_vect.end());
                                                std::sort(compares.begin(), compares.end());
                                                std::sort(compares_double.begin(), compares_double.end());

                                                /* Check for compares when sorted. */

                                                /* Singular */
                                                if (!compares.empty() && compares.front() == t_vect.front() && compares.back() == t_vect.back()) {
                                                      compares.clear(); /* Hit */
                                                }

                                                /* Double */
                                                if (!compares_double.empty() && compares_double.front() == t_vect.front() && compares_double.back() == t_vect.back()) {
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
                                                } else {
                                                      debug_line("Prev-Previous is not loadb with jump.");
                                                      break;
                                                }

                                          } else {
                                          }

                                    } else {

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

                                                      } else {
                                                            debug_warning("Next jump and current is not filled.");
                                                            break;
                                                      }

                                                } else {

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
                                                            } else {
                                                                  break;
                                                            }

                                                      } else {

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
                                          for (const auto &jmp : cond_jumps) {

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
                              prev = (prev != nullptr) ? ((cached_init->address < prev->address) ? prev : i) : i;

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
                        for (const auto &i : range)
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
void ast_funcs::locvars::set_logical_expression(std::shared_ptr<ast_dec::ast> &ast) {

      std::int32_t count = 0u;
      std::shared_ptr<ast_dec::node> start_node = nullptr;
      std::shared_ptr<ast_dec::node> target_cond = nullptr;

      const auto all = ast->main_block->visit_all();
      for (const auto &i : all) {

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
                  ast_funcs::branches::set(ast, start_node->address, i->address, {target_cond->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr}, -1, true, true);

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
                  debug_result("Increased count too %d", count);
                  ;
            }

            if (count != 1u && i->has_expr(ast_dec::expr_type::condition_logical_end)) {

                  debug_line("Decreasing count because current node has logical end expr on %s", i->str().c_str());
                  --count;
                  debug_result("Decreased count too %d", count);

            } else if (i->has_expr(ast_dec::expr_type::condition_logical_end)) {

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
                        } else if ((loadb && target_cond->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr == i->address) || (!loadb && target_cond->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr == (i->address + i->lex->dissassembly->len))) {

                              debug_line("Target condition is expected loadb with jump or expected branch compare on %s", target->str().c_str());

                              if (!(--count)) {
                                    set();
                              }

                              debug_result("Decreased count too %d", count);
                        } else {

                              debug_warning("Nothing hit decreasing count.");
                              --count;
                              debug_result("Decreased count too %d", count);
                        }

                  } else {

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
