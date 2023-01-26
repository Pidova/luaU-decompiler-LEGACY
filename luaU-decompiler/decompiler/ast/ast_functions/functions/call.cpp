#include "../ast_functions.hpp"

/*
	Calls are a bit special in luaU because they rely on parent registers for both arguments and stack. Using this call routines usally get assembled right before
	a call opcode is executed. We get the beggining of this by judging by it's parent target register stack found in the call opcode in previous code. Note you can
	assemble the call and everything ahead of time and call later but that is very impracticle and very unoptimized which is generally not done.
*/
void ast_funcs::calls::set_routines(std::shared_ptr<ast_dec::ast>& ast) {

			const auto calls = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_CALL>(true));

			/* Set call info. */
			for (const auto& node : calls) {

				auto prev = ast->main_block->visit_previous_dest_register(node->address, node->lex->dissassembly->operands.front()->reg);

				/* Namecall gets special treatment. */
				if (prev->lex->dissassembly->op == LuauOpcode::LOP_NAMECALL) {

					const auto data = prev->lex->operand_expr< lexer_dec::operand_types::dest>().front()->reg;
					const auto data_1 = prev->lex->operand_expr< lexer_dec::operand_types::source>().front()->reg;

					/* Call first operand will be the same as previous. */
					if (data == data_1) {
						ast->main_block->visit_previous_dest_register(prev->address, node->lex->dissassembly->operands.front()->reg)->add_expr<ast_dec::expr_type::call_routine_start>(); /* Call start */
					}
					else {

						const auto args = node->lex->operand_expr<lexer_dec::operand_types::integer>().front()->val - 1u;

						if (args) {
							/* Set previous as call register + 2(1 is reserved, other is slot) */
							ast->main_block->visit_previous_dest_register(node->address, node->lex->dissassembly->operands.front()->reg + 2u)->add_expr<ast_dec::expr_type::call_routine_start>(); /* Call start */
						}
						else {
							/* No args namecall is end. */
							prev->add_expr<ast_dec::expr_type::call_routine_start>(); /* Call start */
						}

					}

				}
				else {

					/* Next is end and has args fix prev. */
					if (ast->main_block->visit_next(prev) == node && node->lex->operand_expr<lexer_dec::operand_types::integer>().front()->val) {

						prev = ast->main_block->visit_previous_dest_register(prev->address, node->lex->dissassembly->operands.front()->reg + 1);

					}

					prev->add_expr<ast_dec::expr_type::call_routine_start>(); /* Call start */
				}

				node->add_expr<ast_dec::expr_type::call_routine_end>(); /* Call end. */
			}

			return;
		}


/* Sets multret call routines **Only applies to variables/args initing variables will get handled by transpiler automatically** */
void ast_funcs::calls::set_multret_routines(std::shared_ptr<ast_dec::ast>& ast) {

			std::uint32_t routine = 0u;

			/* Checking for regs and vector reg. */
			bool first = false;
			std::vector<std::uint16_t> check_regs;

			const auto all = ast->main_block->visit_all();
			for (const auto& node : all) {

				/* Inc for concat start, call start, and table start. Dec for concat end, call end, and start end. */
				routine_inc_lv(node, routine);
				routine_dec_lv(node, routine);

				/* Call? */
				if (!routine && node->lex->type == lexer_dec::inst_type::call && !node->has_expr(ast_dec::expr_type::locvar)) {

					auto retn = node->lex->operand_expr<lexer_dec::operand_types::integer>().back()->val;
					const auto start = node->lex->dissassembly->operands.front()->reg;

					/* Fix for multret. */
					if (retn == LUA_MULTRET) {
						retn = generic::fix_mulret(ast, node->address);
					}

					if (retn > 1) {

						/* Add multrets. */
						for (auto i = start; i < (start + retn); ++i) {
							check_regs.emplace_back(i);
						}

						first = true;

					}

				}

				/* Looks for regs. */
				if (check_regs.size() && node->lex->has_operand_expr<lexer_dec::operand_types::source>()) {

					const auto sources = node->lex->operand_expr<lexer_dec::operand_types::source>();
					for (const auto& operand : sources) {

						const auto reg = operand->reg;
						if (std::find(check_regs.begin(), check_regs.end(), reg) != check_regs.end()) {

							/* First */
							if (first) {
								node->add_expr<ast_dec::expr_type::call_mulret_start>();
								first = false;
							}
							else {

								if (check_regs.size() == 1u) {
									node->add_expr<ast_dec::expr_type::call_mulret_end>();
								}
								else {
									node->add_expr<ast_dec::expr_type::call_mulret_member>();
								}

							}

							check_regs.erase(std::remove(check_regs.begin(), check_regs.end(), reg), check_regs.end());

						}

					}

				}

			}

			return;
		}

/* Register gets used in call? */
bool ast_funcs::calls::reg_arg(std::shared_ptr<ast_dec::ast>& ast, const std::shared_ptr<ast_dec::node>& call, const std::uint16_t target) {

			/* Call */
			if (call->lex->type == lexer_dec::inst_type::call && call->lex->has_operand_expr<lexer_dec::operand_types::dest>()) {

				const auto dest = call->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;
				auto arg = call->lex->operand_expr<lexer_dec::operand_types::integer>().front()->val;

				if (arg == LUA_MULTRET) {
					arg = generic::fix_mulret(ast, call->address);
				}

				if (arg) {
					return ((1u + dest) <= target && (1u + dest + arg) >= target);
				}

			}

			return false;
		}