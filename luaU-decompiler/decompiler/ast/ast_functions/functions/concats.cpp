#include "../ast_functions.hpp"

/*
	Concat routines in luaU are a bit special. When a concat opcode is called the start register and end register operand assemble the concat too
	get placed as a dest. This can be used too find it's routine by getting the preceeding dest of the start and the end being the concat we can determine
	the routine and where it starts and end.
*/
void ast_funcs::concats::set_routines(std::shared_ptr<ast_dec::ast>& ast) {

	const auto concats = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_CONCAT>(true));

	/* Set concat info. */
	for (const auto& node : concats) {
		ast->main_block->visit_previous_dest_register(node->address, node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg)->add_expr<ast_dec::expr_type::concat_routine_start>(); /* Concat start. */
		node->add_expr<ast_dec::expr_type::concat_routine_end>(); /* Concat end. */
	}

	return;
}


