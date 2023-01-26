#include "../ast_functions.hpp"

/* Set return exprs for return opcode(s). */
void ast_funcs::instructions::set_return_exprs(std::shared_ptr<ast_dec::ast> &ast) {

      const auto all = ast->main_block->visit_all();
      for (const auto &i : all) {

            switch (i->lex->dissassembly->op) {

                  case LuauOpcode::LOP_RETURN: {
                        debug_success("Setting return on: %s", i->str().c_str());
                        i->add_expr<ast_dec::expr_type::return_>();
                        break;
                  }

                  default: {
                        break;
                  }
            }
      }

      return;
}