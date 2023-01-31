#pragma once
#include "ast_dec.hpp"

namespace ast_post {

      namespace table {


            /* Set indexs exprs for gettable instructions. */
            void set_indexs(const std::shared_ptr<ast_dec::ast> &ast);

            /* Double checks that every settable opcode in table routine is an element. */
            void fill_elements(const std::shared_ptr<ast_dec::ast> &ast);

      } // namespace table

      namespace arith {

            /* Sets arith exprs. */
            void set_arith_exprs(const std::shared_ptr<ast_dec::ast> &ast);

      } // namespace arith

} // namespace ast_post