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
      bool return_ = false; /* Analysys ended on return? */
      std::int8_t used_next = -1; /* Set dest next used source repeat always. */
      std::shared_ptr<ast_dec::node> dest_node = start;
      std::shared_ptr<ast_dec::node> dest_node_nm = start; /* Dest node non mutable by sources. */
      std::int32_t dest_scope = 0;                        /* Scope where dest was set. (Can be reset by source) */
      std::int32_t set_scope = 0;                         /* Scope where dest was set. (Cannot be reset by source) */

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

                        /* Reset data */
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
            if (dest_scope > scope && scope >= 0) {
                  debug_success("Used outside of scope valid register on %s", s_node->str().c_str());
                  no_locvar = false;
                  break;
            }

            /* Out of scope or last scope and hit return. */
            if (scope < 0 || (!scope && s_node->lex->type == lexer_dec::inst_type::return_)) {

                  debug_line("Register use out of scope or hit return without scoped on %s", s_node->str().c_str());

                  const auto source = ast_funcs::regs::get_source_list(ast, s_node);
                  return_ = std::find(source.begin(), source.end(), target) != source.end();

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

                        /* Used in routine not locvar. */
                        if (routine) {
                              debug_warning("Register used as dest inside routine on %s", s_node->str().c_str());
                              no_locvar = true;
                              break;
                        }

                        /* Set twice without used. Locvar */
                        if (used_target_1_dest && set_scope == scope) {
                              debug_warning("Register was set twice without being used on %s", s_node->str().c_str());
                              no_locvar = (dest_node != start && dest_scope > scope && scope >= 0);
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

      /* Ended used on return. */
      if (return_) {
            debug_warning("Not locvar used in return for end.");
            return false;
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

/* Gets list of all dests registers being set.  **Dependent on for loop data** */
std::vector<std::uint16_t> ast_funcs::regs::get_dest_list(std::shared_ptr<ast_dec::ast> &ast, const std::shared_ptr<ast_dec::node> &node) {

    std::vector<std::uint16_t> retn;

    /* Loops */
    const auto for_node = node->loop_extra.end_node;
    if (for_node != nullptr && node->loop_extra.start_node == node) {

          for (auto i = node->loop_extra.end_node->loop_extra.start_reg; i <= node->loop_extra.end_node->loop_extra.end_reg; ++i) 
                  retn.emplace_back(i);
          
    }

     switch (node->lex->dissassembly->op) {

           case LuauOpcode::LOP_RETURN: {

                 const auto dest = node->lex->operand_expr<lexer_dec::operand_types::reg>().front()->reg;
                 auto amt = node->lex->operand_expr<lexer_dec::operand_types::integer>().front()->val;

                 if (amt) {

                       if (amt == -1)
                             amt = (*(&node - 1u))->lex->dissassembly->operands.front()->reg;

                       for (auto a = dest; a < (dest + amt); ++a)
                             retn.emplace_back(a);
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

                 for (auto a = dest; a < (dest + amt); ++a) 
                     retn.emplace_back(a);            

                 break;
           }

           case LuauOpcode::LOP_CALL: {

                 auto call_retn = node->lex->operand_expr<lexer_dec::operand_types::integer>().back()->val;
                 const auto start = node->lex->dissassembly->operands.front()->reg;

                 if (call_retn == LUA_MULTRET) {
                       call_retn = generic::fix_mulret(ast, node->address);
                 }

                 /* Fix with namecall. */
                 bool namecall = false;
                 const auto prev = ast->main_block->visit_previous_addr(node->address);

                 if (prev != nullptr && prev->lex->dissassembly->op == LuauOpcode::LOP_NAMECALL) {

                       namecall = (start == prev->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg);
         
                 }

                 /* Append to dest. (Fill for multiple retn) */
                 for (auto i = start; i < (start + call_retn); ++i) {

                      /* Skip this */
                       if (!i && namecall) {
                             retn.emplace_back(i + 1u);
                             continue;
                       }
                 
                       retn.emplace_back(i);
                 }
                     
                 break;
           }

           default: {

                 /* Append dest */
                 if (node->lex->has_operand_expr<lexer_dec::operand_types::dest>()) {

                       retn.emplace_back(node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg);

                 }

                 break;
           }
     }

     return retn;
}

/* Gets list of all source registers being set. */
std::vector<std::uint16_t> ast_funcs::regs::get_source_list(std::shared_ptr<ast_dec::ast> &ast, const std::shared_ptr<ast_dec::node> &node) {

      bool ignore = false;
      std::vector<std::uint16_t> retn;

      /* Loops */
      const auto for_node = node->loop_extra.end_node;
      if (for_node != nullptr && node->loop_extra.start_node == node) {

            for (auto i = node->loop_extra.end_node->loop_extra.start_reg; i <= node->loop_extra.end_node->loop_extra.end_reg; ++i)
                  retn.emplace_back(i);
      }

      switch (node->lex->dissassembly->op) {

            case LuauOpcode::LOP_RETURN: {

                  const auto dest = node->lex->operand_expr<lexer_dec::operand_types::reg>().front()->reg;
                  auto amt = node->lex->operand_expr<lexer_dec::operand_types::integer>().front()->val;

                  if (amt) {

                        if (amt == -1)
                              amt = node->lex->dissassembly->operands.front()->reg;

                        for (auto a = dest; a < (dest + amt); ++a)
                              retn.emplace_back(a);
                  }

                  ignore = true;

                  break;
            }

            case LuauOpcode::LOP_CALL: {

                  auto args = node->lex->operand_expr<lexer_dec::operand_types::integer>().front()->val;
                  const auto start = node->lex->dissassembly->operands.front()->reg;

                  /* Fix for multret. */
                  if (args == LUA_MULTRET) {
                        args = (generic::fix_mulret(ast, node->address) - start);
                  }

                  for (auto i = 0; i < args; ++i)         
                        retn.emplace_back(i + start + 1u);

                  ignore = true;

                  break;
            }

            default: {
                  break;
            }

      }

      /* Done */
      if (ignore) {
            return retn;
      }

      /* Append source */
      if (node->lex->has_operand_expr<lexer_dec::operand_types::source>()) {

            const auto regz = node->lex->operand_expr<lexer_dec::operand_types::source>();

            for (const auto &operand : regz)
                  retn.emplace_back(operand->reg);

      }

      /* Append reg */
      if (node->lex->has_operand_expr<lexer_dec::operand_types::reg>()) {

            const auto regz = node->lex->operand_expr<lexer_dec::operand_types::reg>();

            for (const auto &operand : regz)
                  retn.emplace_back(operand->reg);

      }

      /* Append compare */
      if (node->lex->has_operand_expr<lexer_dec::operand_types::compare>()) {

            const auto regz = node->lex->operand_expr<lexer_dec::operand_types::compare>();

            for (const auto &operand : regz)
                  retn.emplace_back(operand->reg);

      }

      return retn;
}

/* Sets register_scope_dest expr. */
void ast_funcs::regs::set_source_scope_expr(std::shared_ptr<ast_dec::ast> &ast) {

    const auto all = ast->main_block->visit_all();

    ast_funcs::scopes::scope_data::scope scope (ast, true);
    scope = all;

    ast_funcs::scopes::scope_data::scope_vector<std::uint16_t> regs (scope);

    for (const auto &i : all) {
   
        /* Change table */
        regs[i];

        /* Append dests */
        for (const auto d : ast_funcs::regs::get_dest_list(ast, i)) 
              regs.push_back(d);

        /* Check source */
        for (const auto s : ast_funcs::regs::get_source_list(ast, i))
              if (std::find_if(ast->arg_regs.begin(), ast->arg_regs.end(), [&](const std::pair<std::int16_t, std::string> &pair) { return pair.first == s; }) == ast->arg_regs.end() /* Not arg? */ && regs.find<true>(s) /* Out of scope */ && !regs.find(s) /* In scope */) {
                    i->add_expr<ast_dec::expr_type::source_outside_scope>();
              } else {
                    i->add_expr<ast_dec::expr_type::source_inside_scope>();
              }
       
    }

    return;
}

/* Register is used twice by either source or dest without being reset by either vice versa. */
bool ast_funcs::regs::reg_used_twice(std::shared_ptr<ast_dec::ast> &ast, const std::shared_ptr<ast_dec::node> &start, const std::uint16_t target, const bool same_start /* Dest hit and start are the same? */) {

    bool source = false;
    bool dest = false;

    std::shared_ptr<ast_dec::node> last_dest = nullptr;
    const auto rest = ast->main_block->visit_rest_curr(start->address);

    for (const auto &i : rest) {
    
        for (const auto reg : ast_funcs::regs::get_dest_list(ast, i)) {
        
            if (reg == target) {

                    if (dest) {

                          if (same_start) {
                                return (last_dest == start) ? true : false;
                          } else {
                                return true;
                          }
                    }

                    dest = true;
                    source = false;
                    last_dest = start;

              }

        }
        
        for (const auto reg : ast_funcs::regs::get_dest_list(ast, i)) {

             if (reg == target) {

                    if (source) {

                          if (same_start) {
                                return (last_dest == start) ? true : false;
                          } else {
                                return true;
                          }
                    }

                    dest = false;
                    source = true;

              }

        }

    }

    return false;
}