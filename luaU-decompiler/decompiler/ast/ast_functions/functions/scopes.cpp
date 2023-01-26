#include "../ast_functions.hpp"

/* Gets end of scope node. (Doesn't count for current, looks for ends/jumps/returns) */
std::shared_ptr<ast_dec::node> ast_funcs::scopes::end_of_scope(std::shared_ptr<ast_dec::ast> &ast, const std::shared_ptr<ast_dec::node> &start) {

      /* Check cached */
      if (global_cache::scopes::cached_ends.find(start) != global_cache::scopes::cached_ends.end()) {
            return global_cache::scopes::cached_ends[start]; /* Append */
      }

      std::vector<std::uintptr_t> addr_scopes; /* Scopes */

      std::intptr_t scope = 0;

      const auto all = ast->main_block->visit_rest(start->address);
      for (const auto &node : all) {

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
