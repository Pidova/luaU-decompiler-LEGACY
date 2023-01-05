#include "post_ast.hpp"

void ast_post::table::set_node_end(const std::shared_ptr<ast_dec::ast>& ast) {

	std::vector<std::uintptr_t> nodes_end;
	const auto table_starts = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_expr<ast_dec::expr_type::table_start>(true));

	for (const auto& i : table_starts) {

		bool valid = true;
		for (const auto end : nodes_end)
			if (end > i->address)
				valid = false;

		if (!valid)
			continue;

		/* No elements */
		if (i->has_expr(ast_dec::expr_type::table_end)) {
			i->table_extra.end_table = i->address;
			nodes_end.emplace_back(i->address);
			continue;
		}

		const auto start_node = i->address;
		const auto end_node = ast->main_block->visit_relative_next_expr<ast_dec::expr_type::table_end>(i->address, { ast_dec::expr_type::table_start })->address;

		const auto range = ast->main_block->visit_range_current(start_node, end_node);
		for (const auto& node : range)
			node->table_extra.end_table = end_node;

		nodes_end.emplace_back(end_node);
	}

	return;
}


void ast_post::table::set_indexs(const std::shared_ptr<ast_dec::ast>& ast) {

	const auto all = ast->main_block->visit_all();
	for (const auto& i : all) {

		if (i->lex->type == lexer_dec::inst_type::table_get) {

			i->add_expr<ast_dec::expr_type::table_index>();

		}

	}

	return;
}


void ast_post::arith::set_arith_exprs(const std::shared_ptr<ast_dec::ast>& ast) {

	const auto all = ast->main_block->visit_all();
	for (const auto& i : all) {

		if (i->lex->type == lexer_dec::inst_type::arith) {

			if (i->lex->has_operand_expr<lexer_dec::operand_types::kvalue>()) {
				i->add_expr<ast_dec::expr_type::arithK>();
			}
			else {
				i->add_expr<ast_dec::expr_type::arith>();
			}
			
		}

	}

	return;
}