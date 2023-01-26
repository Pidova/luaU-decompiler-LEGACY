#include "../ast_functions.hpp"

/* Sets table exprs. */
void ast_funcs::tables::set_table_exprs(std::shared_ptr<ast_dec::ast> &ast) {

      const auto all = ast->main_block->visit_all();
      for (const auto &i : all) {

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
void ast_funcs::tables::set_routines(std::shared_ptr<ast_dec::ast> &ast) {

      std::vector<std::uintptr_t> ends; /* Used to avoid encapsulation with tables as this is for ends of already analyzed tables. */

      const auto tables = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_expr<ast_dec::expr_type::table>(true));
      for (const auto &table : tables) {

            /* Check */
            bool valid = true;
            for (const auto i : ends)
                  if (table->lex->dissassembly->addr <= i)
                        valid = false;

            if (!valid)
                  continue;

            std::uint16_t table_reg = 0u;    /* Current register table target. */
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
                  } else {

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
            auto set_size = [&](const std::shared_ptr<ast_dec::node> &node) mutable -> void {
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

                              array_size -= std::uintptr_t(amt);

                              /* Target */
                              if (std::find(table_target_uf.begin(), table_target_uf.end(), dest) != table_target_uf.end()) {

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
                        for (auto &node : nodes) {

                              /* Map out dest */
                              if (!inside_call_routine && node->lex->has_operand_expr<lexer_dec::operand_types::dest>()) {

                                    const auto dest = node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;

                                    if (dest_map.find(dest) == dest_map.end()) {
                                          dest_map.insert(std::make_pair(dest, node)); /* Add */
                                    } else {
                                          dest_map[dest] = node; /* Change */
                                    }
                              }

                              /* New/set table instruction inc/dec for nested. */
                              if (node->has_expr(ast_dec::expr_type::table)) {

                                    ++nested_count;
                                    debug_line("Hit table increased nested count too %d from %s", nested_count, node->str().c_str());

                              } else if (node->lex->type == lexer_dec::inst_type::set_table) {

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

                                                --node_size;                                     /* Dec for node. */
                                                node->add_expr<ast_dec::expr_type::table_end>(); /* End of table. */
                                                last_set = node;
                                                cache(false); /* Revert back cache. */

                                                debug_success("Added table end expr too %s", node->str().c_str());

                                                /* End of a nested table. */
                                                if (nested_count) {
                                                      --nested_count;
                                                } else { /* Nested table is 0 and we entered a new table. */
                                                      break;
                                                }

                                          } else {

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

                                                } else {

                                                      std::uint32_t routine = 0u;

                                                      /* Really just visiting future instructions too see if table source, dest, or idx gets set twice indicating end. */
                                                      const auto rest_nodes = ast->main_block->visit_rest(node->address);
                                                      for (const auto &i : rest_nodes) {

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

                                                                  } else if (i->has_expr(ast_dec::expr_type::table)) {

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

                                                            } else {

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
                                                                        } else { /* New table */

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

                                                                                    } else {
                                                                                          i->add_expr<ast_dec::expr_type::table_end>(); /* End of table. */

                                                                                          /* Dec nested table */
                                                                                          --nested_count;
                                                                                          debug_success("Added table end expr too %s", i->str().c_str());
                                                                                    }

                                                                              } else {

                                                                                    debug_line("New table.");

                                                                                    /* End */
                                                                                    if (array_size) {

                                                                                          table_target_uf.emplace_back(table_reg);
                                                                                          debug_warning("Array size still not 0, cached table reg target.");

                                                                                    } else {
                                                                                          node->add_existance<ast_dec::expr_type::table_element>();
                                                                                          last_set = node;
                                                                                          node_size = 0;
                                                                                          goto node_end;
                                                                                    }
                                                                              }

                                                                              break;
                                                                        }

                                                                  } else if (no_dest(i)) {
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

                                    } else if (no_dest(node)) {

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

                  } else {
                        /* No table members. */
                        ends.emplace_back(table->address);
                  }

            } else {
                  throw std::runtime_error("Expected NEWTABLE or DUPTABLE instruction to init table.");
            }
      }

      std::cout << "RET " << ast->tree_str() << std::endl;
      return;
}
