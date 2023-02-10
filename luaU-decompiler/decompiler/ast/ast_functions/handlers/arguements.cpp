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

      auto set_args = [&](std::shared_ptr<ast_dec::ast> &proto, const bool dont_set = false /* Used for main moves. */) mutable {

            /* Already been analyzed. */
            if (!proto->arg_regs.empty()) {
                  return;
            }

            const auto all = proto->main_block->visit_all();
            ast_funcs::scopes::scope_data::scope scope(proto, true);
            scope = all;
  
            ast_funcs::scopes::scope_data::scope_vector<std::uint32_t> dests (scope);                                                                    /* (Scoped) Registers used in dest. **getting written too** */
            ast_funcs::scopes::scope_data::scope_vector<std::shared_ptr<ast_dec::node>> dests_nodes (scope);                                              /* (INIT) Dest nodes relative to dests (Can't be pair as too this is used for something entirely different from arguments) */
            ast_funcs::scopes::scope_data::scope_umap<std::uint16_t /* Reg */, std::shared_ptr<ast_dec::node> /* Node */> dests_nodes_map (scope); /* When reg was last set. */
            std::vector<std::uint32_t> source_no_dest;                                                                          /* Registers used in source, value but not dest. **Not written too yet but been used** */
            std::vector<std::uint32_t> dests_ns;                                                                                                                  /* (NOT-Scoped) Registers used in dest. */


            for (const auto &node : all) {
                 
                  /* Fix scope */
                  dests[node];
                  dests_nodes[node];
                  dests_nodes_map[node];
                  
                  /* See if theres prep for for loop then add if so. */
                  bool mutated = false;
                  auto target_node = node;
                  if (!target_node->has_expr(ast_dec::expr_type::for_prep)) {

                        const auto prev = ast->main_block->visit_previous_addr(target_node->address);
                        if (prev != nullptr && prev->has_expr(ast_dec::expr_type::for_prep)) {
                                    target_node = prev;
                                    mutated = true;
                        }
                                    
                  }
                         

                  /* Append sources */
                  for (const auto source : ast_funcs::regs::get_source_list(ast, node)) {

                        if (dests_nodes_map.find(source)) {

                            node->source_nodes.emplace_back(dests_nodes_map.get().front()[source]);

                        }

                        /* Append unused reg. */
                        if (!dests.find(source) && std::find(source_no_dest.begin(), source_no_dest.end(), source) == source_no_dest.end()) {
                              node->debug_print_dissassembly("NIGNOG");
                              source_no_dest.emplace_back(source);

                        } else if (dests.find(source)) {

                              node->source_nodes_init.emplace_back(dests_nodes.idx_get(dests.index(source)));
                        
                        }

                  }

                                    /* Append dests */
                  for (const auto dest : ast_funcs::regs::get_dest_list(ast, node)) {

                        /* Add node too dest_nodes_map. */
                        if (dests_nodes_map.find(dest)) {

                              for (auto &i : dests_nodes_map.get())
                                    i[dest] = node;
                        } else {

                              dests_nodes_map.insert(std::make_pair(dest, node));
                        }

                        /* Set target nodes. */
                        if (!dests.find(dest)) { /* Didn't find. */

                              /* Assign */
                              dests.push_back(dest);

                              dests_ns.emplace_back(dest);

                              dests_nodes.push_back(target_node);

                        } else { /* Found dest, mutate dest node. */

                              if (mutated) {

                                    dests_nodes.idx_set(dests.index(dest), target_node);

                              } else {

                                    node->dest_nodes_init.emplace_back(dests_nodes.idx_get(dests.index(dest)));
                              }
                        }
                  }     

                  /* Add for move */
                  if (node->lex->dissassembly->op == LuauOpcode::LOP_MOVE) {

                        const auto reg = node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg;
                        if (dests.find(reg)) {

                              node->sub_node = dests_nodes.idx_get(dests.index(reg));

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
