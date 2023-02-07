#include "../ast_functions.hpp"

/*
	*Note: This isn't perfect and only sets args useful to that proto Ie. print (arg1). Things that arent neccesarly useful like
		   an argument getting set right after or inside of a routine before getting used will most of the time just become a
		   variable depending on some conditions like if it gets used after a condition but it can be set by that condition not
		   garunteed it will become an argument. All of this is put together this way instead of basing everything off of it's
		   arg stack become random args that don't even get used.

		   **Will also set sub nodes and dest nodes with sources**
*/
void ast_funcs::arguments::set(std::shared_ptr<ast_dec::ast> &ast) {

      auto set_args = [&](const std::shared_ptr<ast_dec::ast> &proto, const bool dont_set = false /* Used for main moves. */) mutable {
            std::vector<std::vector<std::uint32_t>> dests;                                                                      /* (Scoped) Registers used in dest. **getting written too** */
            std::vector<std::uint32_t> dests_ns;                                                                                /* (NOT-Scoped) Registers used in dest. */
            std::vector<std::vector<std::shared_ptr<ast_dec::node>>> dests_nodes;                                               /* (INIT) Dest nodes relative to dests (Can't be pair as too this is used for something entirely different from arguments) */
            std::vector<std::unordered_map<std::uint16_t /* Reg */, std::shared_ptr<ast_dec::node> /* Node */>> dest_nodes_map; /* When reg was last set. */
            std::vector<std::uint32_t> source_no_dest;                                                                          /* Registers used in source, value but not dest. **Not written too yet but been used** */
            std::vector<std::uintptr_t> addr_scopes;                                                                            /* Scopes */

            /* Emblace first */
            dests.emplace_back(std::vector<std::uint32_t>({}));
            dests_nodes.emplace_back(std::vector<std::shared_ptr<ast_dec::node>>({}));
            dest_nodes_map.emplace_back(std::unordered_map<std::uint16_t /* Reg */, std::shared_ptr<ast_dec::node> /* Node */>({}));

            /* Already been analyzed. */
            if (!proto->arg_regs.empty()) {
                  return;
            }

            const auto all = proto->main_block->visit_all();
            for (const auto &node : all) {

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
                              } else {
                                    dest_nodes_map.back().insert(std::make_pair(i, node));
                              }

                              if (std::find(dests.back().begin(), dests.back().end(), i) == dests.back().end()) { /* Didnt find dest */
                                    dests.back().emplace_back(i);
                                    dests_ns.emplace_back(i);
                                    dests_nodes.back().emplace_back(for_dest);
                              } else { /* Found dest, mutate dest node. */
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
                                          } else {
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
                                    } else {
                                          dest_nodes_map.back().insert(std::make_pair(a, node));
                                    }

                                    if (std::find(dests.back().begin(), dests.back().end(), a) == dests.back().end()) {
                                          dests.back().emplace_back(a);
                                          dests_ns.emplace_back(a);
                                          dests_nodes.back().emplace_back(node);
                                    } else {
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

                                    const auto data = prev->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;

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
                                    } else {
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

                                    for (const auto &operand : regz) {

                                          if (dest_nodes_map.back().find(operand->reg) != dest_nodes_map.back().end()) {
                                                node->source_nodes.emplace_back(dest_nodes_map.back()[operand->reg]);
                                          }

                                          /* Append unused reg. */
                                          if (std::find(dests.back().begin(), dests.back().end(), operand->reg) == dests.back().end() && std::find(source_no_dest.begin(), source_no_dest.end(), operand->reg) == source_no_dest.end()) {
                                                source_no_dest.emplace_back(operand->reg);
                                          } else if (std::find(dests.back().begin(), dests.back().end(), operand->reg) != dests.back().end()) {
                                                node->source_nodes_init.emplace_back(dests_nodes.back()[std::find(dests.back().begin(), dests.back().end(), operand->reg) - dests.back().begin()]);
                                          }
                                    }
                              }

                              /* Append reg. */
                              if (node->lex->has_operand_expr<lexer_dec::operand_types::reg>()) {

                                    const auto regz = node->lex->operand_expr<lexer_dec::operand_types::reg>();

                                    for (const auto &operand : regz) {

                                          if (dest_nodes_map.back().find(operand->reg) != dest_nodes_map.back().end()) {
                                                node->source_nodes.emplace_back(dest_nodes_map.back()[operand->reg]);
                                          }

                                          /* Append unused reg. */
                                          if (std::find(dests.back().begin(), dests.back().end(), operand->reg) == dests.back().end() && std::find(source_no_dest.begin(), source_no_dest.end(), operand->reg) == source_no_dest.end()) {
                                                source_no_dest.emplace_back(operand->reg);

                                          } else if (std::find(dests.back().begin(), dests.back().end(), operand->reg) != dests.back().end()) {
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
                                    } else {
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

                                    } else {
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
            const auto min = 0;   /* Args are on bottom of stack. */

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
                  if (hole_target < 0) {
                        return;
                  }

                  /* Check front for hole. */
                  if (!std::binary_search(source_no_dest.begin(), source_no_dest.end(), hole_target)) {
                        max = hole_target;
                  }

            } else {

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
      for (auto &proto : ast->protos) {

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
