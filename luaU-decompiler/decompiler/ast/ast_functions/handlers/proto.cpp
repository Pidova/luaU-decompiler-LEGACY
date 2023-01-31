#include "../ast_functions.hpp"

void ast_funcs::proto::set_closure_info(const std::shared_ptr<ast_dec::ast> &current_proto, const std::size_t child_proto_id) {

      std::shared_ptr<ast_dec::node> end_capture = nullptr;
      std::shared_ptr<ast_dec::node> closure_node = nullptr;

      /* Newcloure, Dupclosures node. */
      auto closures = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(current_proto->main_block->visit_inst<LuauOpcode::LOP_NEWCLOSURE>(true));
      const auto dupclosures = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(current_proto->main_block->visit_inst<LuauOpcode::LOP_DUPCLOSURE>(true));

      /* All closures. */
      closures.insert(closures.end(), dupclosures.begin(), dupclosures.end());

      /* Iterate through closures and get expression for it relative to proto given. */
      for (const auto &i : closures) {

            /* Already analyzed. */
            if (i->has_expr(ast_dec::expr_type::closure_global) || i->has_expr(ast_dec::expr_type::closure_local) || i->has_expr(ast_dec::expr_type::closure_newclosure))
                  continue;

            /* Expression has been set. */
            if (closure_node != nullptr)
                  break;

            /* Set newcclosure node. */
            if (i->lex->dissassembly->operands[1]->proto == child_proto_id) {

                  closure_node = i; /* Set node. */

                  /* Skip captures if any. */
                  auto next = current_proto->main_block->visit_next(i);
                  while (next != nullptr && next->lex->dissassembly->op == LuauOpcode::LOP_CAPTURE) {
                        next = current_proto->main_block->visit_next(next);
                  }

                  const auto next_prev = current_proto->main_block->visit_previous_addr(next->address);
                  end_capture = (next != nullptr && next->lex->dissassembly->op == LuauOpcode::LOP_CAPTURE) ? next : (next_prev != nullptr && next_prev->lex->dissassembly->op == LuauOpcode::LOP_CAPTURE) ? next_prev
                                                                                                                                                                                                           : closure_node;

                  /* Next is null */
                  if (next == nullptr) {
                        closure_node->add_expr<ast_dec::expr_type::closure_local>(1u, ast_dec::element::front); /* local function ?? (??) [MUTABLE] */
                  } else {

                        /* Next is setglobal and uses reg as source. */
                        if (next->lex->dissassembly->op == LuauOpcode::LOP_SETGLOBAL && next->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg == i->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg) {
                              closure_node->add_expr<ast_dec::expr_type::closure_global>(1u, ast_dec::element::front);
                              closure_node->closure_extra.setglobal_node = next;
                              closure_node = next;
                              closure_node->add_expr<ast_dec::expr_type::closure_global>(1u, ast_dec::element::front); /*  function ?? (??) */
                        } else {
                              closure_node->add_expr<ast_dec::expr_type::closure_local>(1u, ast_dec::element::front); /* local function ?? (??) [MUTABLE] */
                        }
                  }

                  closure_node->closure_extra.closure_idx = child_proto_id;
            }
      }

      /* Turn expression to closure type. */

      auto proto = current_proto->protos[child_proto_id];

      /* Check for getimport followed by potential gettable. */
      std::int16_t target = -1;
      std::shared_ptr<ast_dec::node> next = end_capture;
      std::string name = "";
      do {

            next = current_proto->main_block->visit_next(next);

            if (next != nullptr) {

                  if (next->lex->dissassembly->op == LuauOpcode::LOP_GETIMPORT) {

                        /* Can't use getimport twice. */
                        if (target != -1) {
                              next = nullptr;
                              break;
                        }

                        target = next->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;
                        name += next->lex->operand_expr<lexer_dec::operand_types::kvalue>().front()->k_value;

                  } else if (next->lex->type == lexer_dec::inst_type::table_get && next->lex->has_operand_expr<lexer_dec::operand_types::kvalue>()) {

                        const auto dest = next->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;
                        const auto source = next->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg;

                        if (target == source) {
                              target = dest;
                        } else {
                              next = nullptr;
                              break;
                        }

                        name += '.' + next->lex->operand_expr<lexer_dec::operand_types::kvalue>().front()->k_value;

                  } else if (next->lex->type == lexer_dec::inst_type::table_set) {

                        const auto source = next->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg;
                        const auto reg = next->lex->operand_expr<lexer_dec::operand_types::reg>().front()->reg;

                        if (source == closure_node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg && reg == target) {
                              name += '.' + next->lex->operand_expr<lexer_dec::operand_types::kvalue>().front()->k_value;
                              break;
                        }

                        next = nullptr;
                        break;
                  } else {
                        /* Only getimport or table get/set. */
                        next = nullptr;
                        break;
                  }
            }

      } while (next != nullptr);

      /* Change too global function with set name. */
      if (next != nullptr) {

            /* Erase , ", ' */
            if (name.find('\"') != std::string::npos) {
                  name.erase(std::remove(name.begin(), name.end(), '\"'), name.end());
            }

            if (name.find('\'') != std::string::npos) {
                  name.erase(std::remove(name.begin(), name.end(), '\''), name.end());
            }

            proto->closure_type = ast_dec::closure_type::global;
            proto->closure_name = name;

            /* Replace next */
            if (closure_node->has_expr(ast_dec::expr_type::closure_local)) {
                  closure_node->replace_next<ast_dec::expr_type::closure_local, ast_dec::expr_type::closure_global>();
            } else if (closure_node->has_expr(ast_dec::expr_type::closure_newclosure)) {
                  closure_node->replace_next<ast_dec::expr_type::closure_newclosure, ast_dec::expr_type::closure_global>();
            }

            /* Fill with dead instructions. */
            for (const auto &i : current_proto->main_block->visit_range_current((closure_node->address + closure_node->lex->dissassembly->len), next->address))
                  i->add_expr<ast_dec::expr_type::dead_instruction>(); /* Handled by before hand. */

            closure_node->closure_extra.idx_nodes = std::make_pair(end_capture, next);

      } else {

            /* Comes with closure name. */
            if (closure_node->has_expr(ast_dec::expr_type::closure_global)) {

                  proto->closure_type = ast_dec::closure_type::global;
                  proto->closure_name = closure_node->lex->operand_expr<lexer_dec::operand_types::kvalue>().front()->k_value;
                  closure_node->add_expr<ast_dec::expr_type::dead_instruction>(); /* Handled by before hand. */

            } else if (closure_node->has_expr(ast_dec::expr_type::closure_local)) { /* Closure name doesn't get compiled unless specified. */

                  proto->closure_type = ast_dec::closure_type::local;

            } else if (closure_node->has_expr(ast_dec::expr_type::closure_newclosure)) { /* No closure name. */

                  proto->closure_type = ast_dec::closure_type::newclosure;

            } else { /* Shouldn't happen but incase it does. */
                  throw std::runtime_error("Unkown expression for closure_type.");
            }
      }

      return;
}
