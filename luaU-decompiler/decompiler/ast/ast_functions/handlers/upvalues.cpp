#include "../ast_functions.hpp"

void ast_funcs::upvalues::set(const std::shared_ptr<ast_dec::ast> &ast) {

      /* Captures follow newclosure or dupclosure(RARLEY). */
      auto closures = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_NEWCLOSURE>(true));
      const auto dupclosures = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_DUPCLOSURE>(true));

      /* All closures. */
      closures.insert(closures.end(), dupclosures.begin(), dupclosures.end());

      for (const auto &node : closures) {

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
                  } else {

                        /* Pass variable */

                        const auto reg = next->lex->dissassembly->operands.back()->reg;

                        std::string name = "";
                        emitter::locvar_name(name, ast->transpiler_config->upvalue_prefix, std::stoi(std::to_string(ast->ast_id) + std::to_string(reg)) /* Str -> int for formatting */, ast->transpiler_config->upvalue_suffix_char);
                        ast->protos[node->closure_extra.closure_idx]->upvalues.insert(std::make_pair(idx++, std::make_pair(name, reg)));

                        /* Change arg name if reg. */
                        for (auto &arg : ast->arg_regs)
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
                        for (const auto &node_ : back) {

                              /* Variable/local function */
                              if (node_->lex->has_operand_expr<lexer_dec::operand_types::dest>() && (node_->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg == reg || ((node_->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg + node_->dest_loc.multret_amount) >= reg && node_->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg <= reg))) {

                                    if (node_->has_expr(ast_dec::expr_type::closure_local)) {
                                          /* local function ?? */
                                          debug_success("Upvalue is a local function.");

                                          /* Set name of upvalue closure. */
                                          ast->protos[(node->lex->has_operand_expr<lexer_dec::operand_types::kvalue>()) ? node_->lex->operand_expr<lexer_dec::operand_types::kvalue>().front()->k_idx : node_->lex->operand_expr<lexer_dec::operand_types::proto>().front()->proto]->closure_name = name;
                                          break;
                                    } else if (node_->has_expr(ast_dec::expr_type::locvar)) {
                                          /* Variable */
                                          debug_success("Upvalue is a local variable.");

                                          /* Set multret names if is. */
                                          if (!node_->dest_loc.multret_amount) {
                                                node_->dest_loc.name = name;
                                          } else {
                                                node_->dest_loc.multret_names[reg - node_->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg] = name;
                                          }

                                          node_->add_existance<ast_dec::expr_type::locvar_upvalue>();
                                          break;
                                    }

                              } else if (node_->has_expr(ast_dec::expr_type::for_iv_start) || node_->has_expr(ast_dec::expr_type::for_start) || node_->has_expr(ast_dec::expr_type::for_n_start)) {

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

                              } else if (node_->sub_node != nullptr && node_->get_sub_node()->has_expr(ast_dec::expr_type::locvar) && node_->get_sub_node()->lex->has_operand_expr<lexer_dec::operand_types::dest>() && (node_->get_sub_node()->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg == reg || ((node_->get_sub_node()->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg + node_->dest_loc.multret_amount) >= reg && node_->get_sub_node()->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg <= reg))) {

                                    debug_success("Upvalue is apart of a call multret.");

                                    /* Set multret names if is. */
                                    if (!node_->dest_loc.multret_amount) {
                                          node_->get_sub_node()->dest_loc.name = name;
                                    } else {
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
