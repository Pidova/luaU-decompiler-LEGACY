#include "../ast_functions.hpp"

/* Sorts loops based on ends. */
void ast_funcs::loops::sort_loops(std::shared_ptr<ast_dec::ast> &ast) {

      auto labels = ast->main_block->visit_all_goto();
      std::sort(labels.begin(), labels.end(), [](std::pair<std::uintptr_t, std::vector<std::shared_ptr<ast_dec::node>>> &a, std::pair<std::uintptr_t, std::vector<std::shared_ptr<ast_dec::node>>> &b) { return a.first < b.first; });

      const auto all = ast->main_block->visit_all();
      for (const auto &node : all) {

            auto node_found = std::find_if(labels.begin(), labels.end(), [&](const std::pair<std::uintptr_t, std::vector<std::shared_ptr<ast_dec::node>>> &pair) { return pair.first == node->address; });

            /* Has goto */
            if (labels.size() && node_found != labels.end()) {

                  debug_success("Label at %s", node->str().c_str());

                  std::vector<std::pair<ast_dec::expr_type, std::size_t>> exprs;

                  if (labels.size() == 1u)
                        continue;

                  /* Collaprse exprs */
                  node->collapse_expr();

                  /* Reverse */
                  std::reverse(node_found->second.begin(), node_found->second.end());

                  for (const auto &label : node_found->second) {

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
                  for (const auto &expr : exprs)
                        node->add_expr_tt(expr.first, expr.second);
            }
      }

      return;
}

/* Sets forprep instructions exprs. ** Can be called by parent in favor for children** */
void ast_funcs::loops::set_for_prep_exprs(std::shared_ptr<ast_dec::ast> &ast) {

      const auto all = ast->main_block->visit_all();
      for (const auto &i : all) {

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
void ast_funcs::loops::set_for_routines(std::shared_ptr<ast_dec::ast> &ast) {

      const auto forgloops = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_FORGLOOP>(true));
      const auto fornloops = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_FORNLOOP>(true));

      /* Forgloops. */
      for (const auto &forloop : forgloops) {

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
            } else {
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
      for (const auto &forloop : fornloops) {

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
void ast_funcs::loops::set_whilerep_routines(std::shared_ptr<ast_dec::ast> &ast) {

      std::vector<std::shared_ptr<ast_dec::node>> nodes;

      const auto jump_backs = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_JUMPBACK>(true));
      const auto jump_conds = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_next_type<lexer_dec::inst_type::branch_condition>(true));

      /* Check typical jumpbacks *Previous inst is condition its until else end for while. */
      for (const auto &jmp_back : jump_backs) {

            if (ast->main_block->visit_previous_addr(jmp_back->address)->lex->type == lexer_dec::inst_type::branch_condition) {
                  jmp_back->add_expr<ast_dec::expr_type::until_>(); /* Until end. */
                  nodes.emplace_back(jmp_back);
            } else {
                  jmp_back->add_expr<ast_dec::expr_type::scope_end>(); /* While loop end. */
                  jmp_back->add_expr<ast_dec::expr_type::while_end>(); /* While loop end. */
                  nodes.emplace_back(jmp_back);
            }
      }

      /* See if jump memaddrs are negatives usally means there until. */
      for (const auto &jmp_back : jump_conds)
            if (jmp_back->lex->dissassembly->operands[std::find(jmp_back->lex->operands.begin(), jmp_back->lex->operands.end(), lexer_dec::operand_types::memaddr) - jmp_back->lex->operands.begin()]->jmp < 0) { /* See if mem address of jump is negative (We need to get idx of memaddr operand). */
                  jmp_back->add_expr<ast_dec::expr_type::until_>();                                                                                                                                               /* Until end. */
                  nodes.emplace_back(jmp_back);
            }

      /* We need to get range of until/while routines conditions. */
      for (const auto &jumpback : nodes) {

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
                  } else {

                        const auto cond = condition.back();

                        if (cond.second->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr > jumpback->address) {

                              /* Not analyzed */
                              if (!cond.first->has_expr(ast_dec::expr_type::condition_concat_start)) {

                                    cond.first->add_expr<ast_dec::expr_type::condition_concat_start>();
                                    cond.second->add_expr<ast_dec::expr_type::condition_concat_end>();

                                    branches::set(ast, cond.first->address, cond.second->address, {jumpback->address});

                                    /* Propagate conditional concat members. */
                                    const auto range = ast->main_block->visit_range(cond.first->address, cond.second->address);
                                    for (const auto &cm_node : range) {
                                          cm_node->add_expr<ast_dec::expr_type::condition_concat_member>();
                                    }
                              }

                        } else {
                              /* Last conditon doesn't jump out possible repeat until(true)*/
                              jumpback->add_expr<ast_dec::expr_type::condition_true>(1u, ast_dec::element::front);
                        }
                  }

            } else { /* While loop */

                  /* No conditions in it (Garunteed repeat (true) do) */
                  if (!condition.size()) {
                        jmp_node->add_expr<ast_dec::expr_type::repeat_>();
                        jumpback->add_expr<ast_dec::expr_type::condition_true>(1u, ast_dec::element::front);
                  } else {

                        const auto cond = condition.front();

                        if (cond.second->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr > jumpback->address) {

                              bool dont_set = false;
                              std::shared_ptr<ast_dec::node> begin_node = cond.first;

                              /* Go through second and first range and see regs logicals. */
                              const auto range = ast->main_block->visit_range_current(cond.first->address, cond.second->address);
                              for (const auto &node : range) {

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
                                                } else if (logical) {

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
                                    branches::set(ast, begin_node->address, cond.second->address, {jumpback->address, (jumpback->address + jumpback->lex->dissassembly->len)});

                                    /* Propagate conditional concat members. */
                                    const auto range = ast->main_block->visit_range(begin_node->address, cond.second->address);
                                    for (const auto &cm_node : range) {
                                          cm_node->add_expr<ast_dec::expr_type::condition_concat_member>();
                                    }
                              }

                        } else {
                              /* Last conditon doesn't jump out possible repeat until(true)*/
                              jmp_node->add_expr<ast_dec::expr_type::repeat_>();
                              jumpback->add_expr<ast_dec::expr_type::condition_true>(1u, ast_dec::element::front);
                        }
                  }
            }

            /* End of routine is given with until. Find begging. */
            for (const auto &node : nodes_routine) {

                  /* Break **Not definite jump to end of until routine or end of while can mean break of any conditional** */
                  if (node->lex->type == lexer_dec::inst_type::branch) {

                        if (!node->has_expr(ast_dec::expr_type::break_) /* No break */ && node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr > jumpback->address) {
                              node->add_expr<ast_dec::expr_type::break_>();
                        }
                  }

                  if (node->lex->type == lexer_dec::inst_type::branch_condition) {
                  
                      if (node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr > jumpback->address) {
                              node->add_expr<ast_dec::expr_type::conditional_break>();
                      }
                      
                  }

            }
      }

      return;
}
