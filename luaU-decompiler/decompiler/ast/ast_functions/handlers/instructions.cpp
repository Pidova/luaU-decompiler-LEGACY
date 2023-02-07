#include "../ast_functions.hpp"

/* Sets exit exprs. */
void ast_funcs::instructions::set_exit_exprs(std::shared_ptr<ast_dec::ast> &ast) {

	std::vector<std::uintptr_t> scopes;

	bool hit_end = false;

	const auto all = ast->main_block->visit_all();
    for (const auto &i : all) {
    
		/* Hit */
		if (hit_end) {
             i->add_expr<ast_dec::expr_type::exit_dead>();  
		}

		/* Append */
		if ((i->lex->type == lexer_dec::inst_type::branch || i->lex->type == lexer_dec::inst_type::branch_condition) && i->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp >= 0) {
                scopes.emplace_back(i->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr);  
		}

		/* Found */
		if (!scopes.empty() && std::find(scopes.begin(), scopes.end(), i->address) != scopes.end()) {
               scopes.erase(std::remove(scopes.begin(), scopes.end(), i->address), scopes.end());       
		}

		/* End */
		if (scopes.empty() && i->lex->type == lexer_dec::inst_type::return_) {
               i->add_expr<ast_dec::expr_type::exit_post>();
               hit_end = true;
		}

	}


	return;
}