#include "post_ast.hpp"


void ast_post::table::set_indexs(const std::shared_ptr<ast_dec::ast> &ast) {

      const auto all = ast->main_block->visit_all();
      for (const auto &i : all) {

            if (i->lex->type == lexer_dec::inst_type::table_get) {

                  i->add_expr<ast_dec::expr_type::table_index>();
            }
      }

      return;
}

void ast_post::table::fill_elements(const std::shared_ptr<ast_dec::ast> &ast) {

      const auto table_starts = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_expr<ast_dec::expr_type::table_start>(true));
      for (const auto &i : table_starts) {

            const auto range = ast->main_block->visit_range_current(i->address, ast->main_block->visit_relative_next_expr_scope_current<ast_dec::expr_type::table_end>(i->address, {ast_dec::expr_type::table_start})->address);
            for (const auto &node : range) {

                if (node->lex->type == lexer_dec::inst_type::set_table) {
                  
                    node->add_existance<ast_dec::expr_type::table_element>();

                }

            }

      }

      return;
}

void ast_post::arith::set_arith_exprs(const std::shared_ptr<ast_dec::ast> &ast) {

      const auto all = ast->main_block->visit_all();
      for (const auto &i : all) {

            if (i->lex->type == lexer_dec::inst_type::arith) {

                  if (i->lex->has_operand_expr<lexer_dec::operand_types::kvalue>()) {
                        i->add_expr<ast_dec::expr_type::arithK>();
                  } else {
                        i->add_expr<ast_dec::expr_type::arith>();
                  }
            }
      }

      return;
}