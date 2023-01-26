#pragma once

#define debug_name "AST-DEBUG"

#define node_nonmutable(node) node->has_expr(ast_dec::expr_type::condition_nonmutable)

#define routine_inc(node, routine) routine += node->count_expr<ast_dec::expr_type::concat_routine_start>() + node->count_expr<ast_dec::expr_type::call_routine_start>() + node->count_expr<ast_dec::expr_type::table_start>() + node->count_expr<ast_dec::expr_type::condition_concat_start>() + node->count_expr<ast_dec::expr_type::conditional_expression_start>()
#define routine_dec(node, routine) routine -= node->count_expr<ast_dec::expr_type::concat_routine_end>() + node->count_expr<ast_dec::expr_type::call_routine_end>() + node->count_expr<ast_dec::expr_type::table_end>() + node->count_expr<ast_dec::expr_type::condition_concat_end>() + node->count_expr<ast_dec::expr_type::conditional_expression_end>()

/* Same thing as routines(inc/dec) but conditional concat routines gets ignored because they don't garunteed a locvar. */
#define routine_inc_lv(node, routine) routine += node->count_expr<ast_dec::expr_type::concat_routine_start>() + node->count_expr<ast_dec::expr_type::call_routine_start>() + node->count_expr<ast_dec::expr_type::table_start>() + node->count_expr<ast_dec::expr_type::conditional_expression_start>()
#define routine_dec_lv(node, routine) routine -= node->count_expr<ast_dec::expr_type::concat_routine_end>() + node->count_expr<ast_dec::expr_type::call_routine_end>() + node->count_expr<ast_dec::expr_type::table_end>() + node->count_expr<ast_dec::expr_type::conditional_expression_end>()

/* Same thing as routines(inc/dec) but it is very basic. */
#define routine_inc_basic(node, routine) routine += node->count_expr<ast_dec::expr_type::concat_routine_start>() + node->count_expr<ast_dec::expr_type::call_routine_start>() + node->count_expr<ast_dec::expr_type::conditional_expression_start>()
#define routine_dec_basic(node, routine) routine -= node->count_expr<ast_dec::expr_type::concat_routine_end>() + node->count_expr<ast_dec::expr_type::call_routine_end>() + node->count_expr<ast_dec::expr_type::conditional_expression_end>()

/* See if there is no dest (Includes fastcall) */
#define no_dest(node) !node->lex->has_operand_expr<lexer_dec::operand_types::dest>() && !node->lex->has_operand_expr<lexer_dec::operand_types::fastcall_idx>()

/* Has generic condition break? */
#define condition_break(node) (node->has_expr(ast_dec::expr_type::break_) ||          \
                               node->has_expr(ast_dec::expr_type::condition_break) || \
                               node->has_expr(ast_dec::expr_type::return_) ||         \
                               node->has_expr(ast_dec::expr_type::until_) ||          \
                               node->has_expr(ast_dec::expr_type::while_end) ||       \
                               node->has_expr(ast_dec::expr_type::for_end) ||         \
                               node->has_expr(ast_dec::expr_type::for_n_end) ||       \
                               node->has_expr(ast_dec::expr_type::for_iv_end))

/* Has condition break out? */
#define condition_break_out(node) (node->has_expr(ast_dec::expr_type::until_) ||    \
                                   node->has_expr(ast_dec::expr_type::while_end) || \
                                   node->has_expr(ast_dec::expr_type::for_end) ||   \
                                   node->has_expr(ast_dec::expr_type::for_n_end) || \
                                   node->has_expr(ast_dec::expr_type::for_iv_end))
