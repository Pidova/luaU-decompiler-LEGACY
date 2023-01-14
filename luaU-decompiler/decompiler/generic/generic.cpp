#include "generic.hpp"

std::int16_t generic::fix_mulret(const std::shared_ptr<ast_dec::ast>& ast, const std::uintptr_t start) {

	auto prev = ast->main_block->visit_previous_addr(start);

	if (prev == nullptr) {
		return 0;
	}

	/* Not routine return. */
	if (!prev->has_expr(ast_dec::expr_type::call_routine_start) && prev->lex->has_operand_expr<lexer_dec::operand_types::dest>()) {
		return prev->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;
	}


	/* Loop */
	while (prev->has_expr(ast_dec::expr_type::call_routine_start) || !prev->lex->has_operand_expr<lexer_dec::operand_types::dest>()) {
		prev = ast->main_block->visit_previous_addr(prev->address);
	}


	return (prev->lex->has_operand_expr<lexer_dec::operand_types::dest>()) ? prev->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg : 0;
}