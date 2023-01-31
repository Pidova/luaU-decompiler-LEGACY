#include "../ast_functions.hpp"

/* Sees if destination is logical in scope. */
bool ast_funcs::regs::logical_dest_register(std::shared_ptr<ast_dec::ast> &ast, std::shared_ptr<ast_dec::node> start, const std::int16_t target) {

      /* Check for loop too see if they override any of the regs. */
      const auto next_scope = ast->main_block->visit_rest_scope_addr(start->address);
      for (const auto &i : next_scope) {

            /* See if register falls within loop vars. */
            if (i->has_expr(ast_dec::expr_type::for_start) || i->has_expr(ast_dec::expr_type::for_iv_start) || i->has_expr(ast_dec::expr_type::for_n_start)) {
                  const auto for_target = i->loop_extra.end_node;
                  if (for_target->loop_extra.start_reg <= target && for_target->loop_extra.end_reg >= target) {
                        return false;
                  }
            }
      }

      /* See if register gets used twice by dest or source with repecting scopes and either or reseting it. */
      std::intptr_t routine = 0;
      std::intptr_t scope = 0;
      std::size_t iter_count = 0u; /* Counts times it was iterated. */
      const auto target_1 = target;
      bool used_target_1_dest = true;
      bool used_target_1_source = false;
      bool no_locvar = true;
      std::int8_t used_next = -1; /* Set dest next used source repeat always. */
      std::shared_ptr<ast_dec::node> dest_node = start;
      std::shared_ptr<ast_dec::node> dest_node_nm = start; /* Dest node non mutable by sources. */
      std::int32_t dest_scope = 0u;                        /* Scope where dest was set. (Can be reset by source) */
      std::int32_t set_scope = 0u;                         /* Scope where dest was set. (Cannot be reset by source) */

      debug_success("Starting with: %s", start->str().c_str());

      /* Really just visiting future instructions too see if table source, dest, or idx gets set twice indicating end. */
      const auto rest_nodes = ast->main_block->visit_rest(start->address);
      for (const auto &s_node : rest_nodes) {

            ++iter_count;

            /* Checks operands if targets get used or not but if it does get used just resets target. */
            bool used_source_twice = false;   /* Used by source twice. */
            bool used_source_routine = false; /* Used by source in routine. */
            std::function<void(const std::shared_ptr<LuaU_dissassembler::operand> &, const lexer_dec::operand_types)> check_usage = [&](const std::shared_ptr<LuaU_dissassembler::operand> &operand, const lexer_dec::operand_types tt) mutable {
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

                        debug_line("Reset data.");

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
            scope += (s_node->count_expr<ast_dec::expr_type::repeat_>() +
                      s_node->count_expr<ast_dec::expr_type::while_>() +
                      s_node->count_expr<ast_dec::expr_type::for_start>() +
                      s_node->count_expr<ast_dec::expr_type::for_iv_start>() +
                      s_node->count_expr<ast_dec::expr_type::for_n_start>() +
                      s_node->count_expr<ast_dec::expr_type::if_>());
            scope -= s_node->count_expr<ast_dec::expr_type::scope_end>();

            /* Set but now used outside of scope. */
            if (dest_scope > scope && scope > 0) {
                  debug_success("Used outside of scope valid register on %s", s_node->str().c_str());
                  no_locvar = false;
                  break;
            }

            /* Out of scope or last scope and hit return. */
            if (scope < 0 || (!scope && s_node->lex->type == lexer_dec::inst_type::return_)) {
                  debug_line("Register use out of scope or hit return without scoped.");
                  break;
            }

            /* End of scope */
            if (!scope && s_node->lex->type == lexer_dec::inst_type::return_) {

                  /* Just ended with it just using it as dest. */
                  if (used_target_1_dest && !used_target_1_source) {
                        debug_success("Return used register as source on %s", s_node->str().c_str());
                        no_locvar = false;
                        break;
                  }
            }

            /* Capture */
            if (s_node->lex->type == lexer_dec::inst_type::capture && s_node->lex->has_operand_expr<lexer_dec::operand_types::source>() && s_node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg == target_1) {      
                 debug_line("Register used on capture for %s", s_node->str().c_str());
                 no_locvar = (dest_node != start);
                 break;
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
                        set_scope = std::int32_t (scope);
                        dest_scope = std::int32_t (scope);
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
      if (next != nullptr && next->lex->type == lexer_dec::inst_type::return_ && iter_count == 1u) {
            debug_warning("Register created right before a return instruction not a locvar.");
            return false;
      }

      debug_success("It is a locvar.");
      return true;
}

/* Sets statements based on register stack. */
void ast_funcs::regs::set_statements(std::shared_ptr<ast_dec::ast> &ast) {

      const auto all = ast->main_block->visit_all();
      for (const auto &i : all) {

            if (i->lex->has_operand_expr<lexer_dec::operand_types::dest>()) {
            }
      }
}


