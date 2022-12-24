#include <algorithm>
#include "ast_config.hpp"
#include "ast_dec.hpp"
#include "post_ast.hpp"

#define node_nonmutable(node) node->has_expr(ast_dec::expr_type::condition_nonmutable)

namespace ast_funcs {

	namespace proto {

		void set_closure_info(const std::shared_ptr<ast_dec::ast>& current_proto, const std::size_t child_proto_id) {

			std::shared_ptr<ast_dec::node> closure_node = nullptr;

			/* Newclosure, Dupclosures node. */
			auto closures = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(current_proto->main_block->visit_inst<LuauOpcode::LOP_NEWCLOSURE>(true));
			const auto dupclosures = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(current_proto->main_block->visit_inst<LuauOpcode::LOP_DUPCLOSURE>(true));

			/* All closures. */
			closures.insert(closures.end(), dupclosures.begin(), dupclosures.end());

			/* Iterate through closures and get expression for it relative to proto given. */
			for (const auto& i : closures) {

				/* Expression has been set. */
				if (closure_node != nullptr)
					break;

				switch (i->lex->dissassembly->op) {

					case LuauOpcode::LOP_NEWCLOSURE: {

						/* Set newcclosure node. */
						if (i->lex->dissassembly->operands[1]->proto == child_proto_id)
							closure_node = i; /* Set node. */

						break;
					}

					case LuauOpcode::LOP_DUPCLOSURE: {

						/* Get proto from dupclosure kvalue. */
						if (current_proto->p->p[child_proto_id] == gco2cl(current_proto->p->k[i->lex->dissassembly->operands[1]->k_idx].value.gc)->l.p)
							closure_node = i; /* Set node. */

						break;
					}

					default: {
						throw std::runtime_error("Unkown opcode for closure_type.");
					}

				}

			}

			/* Turn expression to closure type. */
			for (const auto& e : closure_node->expr) {

				auto& proto = current_proto->protos[child_proto_id];

				switch (e.first) {

					/* Comes with closure name. */
					case ast_dec::expr_type::closure_global: {
						proto->closure_type = ast_dec::closure_type::global;
						proto->closure_name = std::get<std::shared_ptr<ast_dec::node>>(current_proto->main_block->visit_next_inst<LuauOpcode::LOP_SETGLOBAL>(closure_node->address, false))->lex->dissassembly->operands[2]->k_value;
						break;
					}

					/* Closure name doesn't get compiled unless specified. */
					case ast_dec::expr_type::closure_local: {
						proto->closure_type = ast_dec::closure_type::local;
						break;
					}

					/* No closure name. */
					case ast_dec::expr_type::closure_newclosure: {
						proto->closure_type = ast_dec::closure_type::newclosure;
						break;
					}

					/* Shouldn't happen but incase it does. */
					default: {
						throw std::runtime_error("Unkown expression for closure_type.");
					}

				}

			}

			return;
		}

	}

	namespace branches {
		
		/* Set branches conditons in routine with given range. */
		template <ast_dec::expr_type tt /* */, bool loadb = false /* loadb influences compare (conditional operations) */>
		void set(std::shared_ptr<ast_dec::ast>& ast, const std::uintptr_t begin, const std::uintptr_t end) {

			std::uintptr_t ignore_till_addr = 0u;

			const auto range = ast->main_block->visit_range(begin, end);
			const auto loadbs = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_LOADB>(true));

			for (const auto& node : range) {

				/* Used for loadb jump. */
				if (ignore_till_addr && node->address > ignore_till_addr)
					continue;

				if (node->lex->type == lexer_dec::inst_type::branch_condition || node->lex->type == lexer_dec::inst_type::branch) {

					// const auto jmp = node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr;

				}

				/* Check loadb as end or data. */
				if (node->lex->dissassembly->op == LuauOpcode::LOP_LOADB) {

					const auto jmp = node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front();

					/* Jump. (Logical operation) */
					if (jmp->jmp) {

						ignore_till_addr = jmp->jmp_addr;

						const auto jmp_target = ast->main_block->visit_addr(ignore_till_addr);

						/* Next loadb exists so check to see if it has a jump. */
						if (ast->main_block->has_next_inst<LuauOpcode::LOP_LOADB>(jmp_target->address)) {

							bool next_bool_jump = false;

							for (const auto& lb : loadbs) {

								/* Fits in range and has jump. */
								if (lb->address > jmp_target->address && lb->address < end && lb->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp) {
									next_bool_jump = true;
									break;
								}

							}

							/* Next bools in the routines don't have jumps. */
							if (!next_bool_jump) {

								node->add_expr<tt>();

							}
							else { /* Next bools in the routine does have jump. */

								/* Conditional for the set jmp and dead for the jmp target. */
								node->add_expr<ast_dec::expr_type::conditional>();						

							}

							jmp_target->add_expr<ast_dec::expr_type::dead_instruction>();

						}
						else {

							/* Mark loadb so it gets analyzed. */
							ast->main_block->visit_addr(ignore_till_addr)->add_expr<ast_dec::expr_type::dead_instruction>();

						}

					}

				}

			}

			ast->main_block->visit_addr(end)->add_expr<tt>();

			return;
		}

		/* Sets valid branch routines for a range with expr: "condition_routine" */
		void set_valid_branch_routine(std::shared_ptr<ast_dec::ast>& ast) {

			const auto all = ast->main_block->visit_all();

		    auto condition = std::get<std::shared_ptr<ast_dec::node>>(ast->main_block->visit_type<lexer_dec::inst_type::branch_condition>(false))->lex->operand_expr<lexer_dec::operand_types::compare>();
			auto target_1 = condition.front()->reg;
			auto target_2 = (condition.size () > 1u) ? condition.back()->reg : -1;
			bool used_target_1 = false;
			bool used_target_2 = false;

			std::shared_ptr<ast_dec::node> start_node = nullptr;

			/* Checks operands if targets get used or not but if it does get used just resets target. */
			std::function<void(const std::shared_ptr<LuaU_dissassembler::operand>&, const lexer_dec::operand_types)> check_usage = [&](const std::shared_ptr<LuaU_dissassembler::operand>& operand, const lexer_dec::operand_types tt) mutable {

				const auto val = operand->reg;

				if (val == target_1)
					used_target_1 = false;

				if (target_2 != -1 && signed(val) == target_2)
					used_target_2 = false;

				return;
			};



			for (const auto& node : all) {

				node->lex->operand_expr_callback<lexer_dec::operand_types::source>(check_usage);
				node->lex->operand_expr_callback<lexer_dec::operand_types::compare>(check_usage);
				node->lex->operand_expr_callback<lexer_dec::operand_types::reg>(check_usage);


				if (node->lex->has_operand_expr<lexer_dec::operand_types::dest>()) {

					const auto dest = node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;

					/* Check target usage. Abrubt end. */
					if (dest == target_1) {

						/* Set start node for buffer. */
						if (start_node == nullptr) {
							start_node = node;
						}

						/* Set twice without used. Abrubt end. */
						if (used_target_1) {
							used_target_1 = false;
							used_target_2 = false;
							start_node = nullptr;
							continue;
						}

						used_target_1 = true;
					}

					if (target_2 != -1 && signed(dest) == target_2) {

						/* Set start node for buffer. */
						if (start_node == nullptr) {
							start_node = node;
						}

						/* Set twice without used. Abrubt end. */
						if (used_target_2) {
							used_target_1 = false;
							used_target_2 = false;
							start_node = nullptr;
							continue;
						}

						used_target_2 = true;
					}

				}

				if (node->lex->type == lexer_dec::inst_type::branch_condition) {

					/* Start node is null so just use current node. */
					if (start_node == nullptr) {
						start_node = node;
					}

					/* Append condition routine expr. */
					start_node->add_expr<ast_dec::expr_type::condition_routine_start>();
					node->add_expr<ast_dec::expr_type::condition_routine_end>();

					const auto range_br = ast->main_block->visit_range(start_node->address, node->address);
					for (const auto& i : range_br) {
						i->add_expr<ast_dec::expr_type::condition_routine>();
					}

					/* Set next. */
					condition = std::get<std::shared_ptr<ast_dec::node>>(ast->main_block->visit_type<lexer_dec::inst_type::branch_condition>(false))->lex->operand_expr<lexer_dec::operand_types::compare>();
					target_1 = condition.front()->reg;
					target_2 = (condition.size() > 1u) ? condition.back()->reg : -1;
					used_target_1 = false;
					used_target_2 = false;
					start_node = nullptr;

				}

			}

			return;
		}

	}

	namespace arguments {

		/* 
			*Note: This isn't perfect and only sets args useful to that proto Ie. print (arg1). Things that arent neccesarly useful like
				   an argument getting set right after or inside of a routine before getting used will most of the time just become a 
				   variable depending on some conditions like if it gets used after a condition but it can be set by that condition not
				   garunteed it will become an argument. All of this is put together this way instead of basing everything off of it's
				   arg stack become random args that don't even get used.
		*/
		void set(std::shared_ptr<ast_dec::ast>& ast) {

			std::vector <std::uint32_t> dests; /* Registers used in dest. **getting written too** */
			std::vector <std::uint32_t> source_no_dest; /* Registers used in source, value but not dest. **Not written too yet but been used** */


			/* Everything needs to go through based on control flow. */
			const auto all = ast->main_block->visit_all();
			for (const auto& node : all) {

				/* Loops usally overwrite some regs per part of there routine append them to dest. */
				if (node->lex->type == lexer_dec::inst_type::for_) {

					auto start_reg = 0u; /* for start */
					auto iteration = 0u; /* for regs being consumed for its operation */

					switch (node->lex->dissassembly->op) {

						case LuauOpcode::LOP_FORGLOOP: {
							start_reg = node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg;
							iteration = 4u;
							break;
						}
						case LuauOpcode::LOP_FORNLOOP: {
							start_reg = node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg;
							iteration = 2u;
							break;
						}

						/* Could be loop prep??? maybe */
						default: {
							continue;
						}

					}

					/* Add vars from those loops. */
					for (auto i = start_reg; i < (start_reg + iteration + 1u); ++i)
						if (std::find(dests.begin(), dests.end(), i) == dests.end()) /* Found dest */
							dests.emplace_back(i);

				}

				
				switch (node->lex->dissassembly->op) {
					
					case LuauOpcode::LOP_RETURN: {

						const auto dest = node->lex->operand_expr<lexer_dec::operand_types::reg>().front()->reg;
						auto amt = node->lex->operand_expr<lexer_dec::operand_types::integer>().front()->val;

						if (amt) {

							if (amt == -1)
								amt = (*(&node - 1u))->lex->dissassembly->operands.front()->reg;

							for (auto a = dest; a < (dest + amt); ++a)
								if (std::find(dests.begin(), dests.end(), a) == dests.end())
									dests.emplace_back(a);

						}

						break;
					}

					case LuauOpcode::LOP_GETVARARGS: {

						const auto dest = node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;
						auto amt = node->lex->operand_expr<lexer_dec::operand_types::integer>().front()->val;

						if (amt == -1)
							amt = (*(&node - 1u))->lex->dissassembly->operands.front()->reg;

						for (auto a = dest; a < (dest + amt); ++a)
							if (std::find(dests.begin(), dests.end(), a) == dests.end())
								dests.emplace_back(a);

						break;
					}

					case LuauOpcode::LOP_CALL: {

						auto args = node->lex->operand_expr<lexer_dec::operand_types::integer>().front()->val;
						const auto start = node->lex->dissassembly->operands.front()->reg;

						if (args == -1)
							args = ((*(&node - 1u))->lex->dissassembly->operands.front()->reg - start);

						/* Iterate through args and see if arg is not getting used in dest. */
						for (auto i = 0; i < args; ++i) {

							const auto arg = (i + start + 1u);

							if (std::find(dests.begin(), dests.end(), arg) == dests.end() &&
								std::find(source_no_dest.begin(), source_no_dest.end(), arg) == source_no_dest.end())
									source_no_dest.emplace_back(arg);

						}

						/* Add placement for call. */
						if (node->lex->has_operand_expr<lexer_dec::operand_types::dest>() && std::find(dests.begin(), dests.end(), start) == dests.end() &&
							std::find(source_no_dest.begin(), source_no_dest.end(), start) == source_no_dest.end())
								source_no_dest.emplace_back(start);

						break;
					}

					default: {

						/* Append source. */
						if (node->lex->has_operand_expr<lexer_dec::operand_types::source>()) {

							const auto regz = node->lex->operand_expr<lexer_dec::operand_types::source>();

							for (const auto& operand : regz) {

								/* Append unused reg. */
								if (std::find(dests.begin(), dests.end(), operand->reg) == dests.end() && std::find(source_no_dest.begin(), source_no_dest.end(), operand->reg) == source_no_dest.end())
									source_no_dest.emplace_back(operand->reg);

							}

						}


						/* Append reg. */
						if (node->lex->has_operand_expr<lexer_dec::operand_types::reg>()) {

							const auto regz = node->lex->operand_expr<lexer_dec::operand_types::reg>();

							for (const auto& operand : regz) {

								/* Append unused reg. */
								if (std::find(dests.begin(), dests.end(), operand->reg) == dests.end() && std::find(source_no_dest.begin(), source_no_dest.end(), operand->reg) == source_no_dest.end())
									source_no_dest.emplace_back(operand->reg);

							}

						}


						/* Append dest. */
						if (node->lex->has_operand_expr<lexer_dec::operand_types::dest>() && std::find(dests.begin(), dests.end(), node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg) == dests.end()) {

							const auto reg = node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;

							dests.emplace_back(reg);

							/* Append this. */
							if (node->lex->dissassembly->op == LuauOpcode::LOP_NAMECALL)
								dests.emplace_back(reg + 1u);
						}

						break;
					}

				}

			}
			
			return;
		}

	}

	namespace concats {

		/*
			Concat routines in luaU are a bit special. When a concat opcode is called the start register and end register operand assemble the concat too
			get placed as a dest. This can be used too find it's routine by getting the preceeding dest of the start and the end being the concat we can determine
			the routine and where it starts and end.
		*/
		void set_routines(std::shared_ptr<ast_dec::ast>& ast) {

			const auto concats = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_CONCAT>(true));
			
			/* Set concat info. */
			for (const auto& node : concats) {
				ast->main_block->visit_previous_dest_register(node->address, node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg)->add_expr<ast_dec::expr_type::concat_routine_start>(); /* Concat start. */
				node->add_expr<ast_dec::expr_type::concat_routine_end>(); /* Concat end. */
			}
			
			return;
		}

	}

	namespace calls {

		/* 
			Calls are a bit special in luaU because they rely on parent registers for both arguments and stack. Using this call routines usally get assembled right before
			a call opcode is executed. We get the beggining of this by judging by it's parent target register stack found in the call opcode in previous code. Note you can
			assemble the call and everything ahead of time and call later but that is very impracticle and very unoptimized which is generally not done.
		*/
		void set_routines(std::shared_ptr<ast_dec::ast>& ast) {

			const auto calls = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_CALL>(true));
			
			/* Set call info. */
			for (const auto& node : calls) {

				const auto prev = ast->main_block->visit_previous_dest_register(node->address, node->lex->dissassembly->operands.front()->reg);

				/* Namecall gets special treatment. */
				if (prev->lex->dissassembly->op == LuauOpcode::LOP_NAMECALL) {
					// lexer_dec::operand_types::source

					const auto data = prev->lex->operand_expr< lexer_dec::operand_types::source>().front()->reg;
					const auto data_1 = prev->lex->operand_expr< lexer_dec::operand_types::integer>().front()->reg;
					
					/* Call first operand will be the same as previous. */
					if (data == data_1) {
						ast->main_block->visit_previous_dest_register(prev->address, node->lex->dissassembly->operands.front()->reg)->add_expr<ast_dec::expr_type::call_routine_start>(); /* Call start */
					}
					else {

						const auto args = node->lex->operand_expr< lexer_dec::operand_types::integer>().front()->val - 1u;

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
					prev->add_expr<ast_dec::expr_type::call_routine_start>(); /* Call start */
				}

				node->add_expr<ast_dec::expr_type::call_routine_end>(); /* Call end. */
			}

			return;
		}

	}

	namespace loops {

		/* 
			For loops in luaU are generally easy to find based on it instruction jumpback range and where a for loop instruction is though this only finds
			start and end of the routine not where variables get assembled that is usally self handled by the transpiler and IGNORED by locvar huertistics
			though special cases may apply changing this.
		*/
		void set_for_routines(std::shared_ptr<ast_dec::ast>& ast) {

			const auto forgloops = std::get<std::vector<std::shared_ptr<ast_dec::node>>> (ast->main_block->visit_inst<LuauOpcode::LOP_FORGLOOP>(true));
			const auto fornloops = std::get<std::vector<std::shared_ptr<ast_dec::node>>> (ast->main_block->visit_inst<LuauOpcode::LOP_FORNLOOP>(true));
		
			/* Forgloops. */
			for (const auto& forloop : forgloops) {

				const auto jump_node = ast->main_block->visit_addr(forloop->lex->dissassembly->operands[1]->jmp_addr);
				const auto jump_inst = jump_node->lex->dissassembly->op;

				/* for i,v in ipairs/pairs */
				if (jump_inst == LuauOpcode::LOP_FORGPREP_INEXT || jump_inst == LuauOpcode::LOP_FORGPREP_NEXT)
					jump_node->add_expr<ast_dec::expr_type::for_iv_start>();
				else
					jump_node->add_expr<ast_dec::expr_type::for_start>();

				forloop->add_expr<ast_dec::expr_type::scope_end>();

				jump_node->loop_extra.end_node = forloop;
				forloop->loop_extra.end_node = forloop;
			}

			/* Fornloops. */
			for (const auto& forloop : fornloops) {

				const auto loop = ast->main_block->visit_addr(forloop->lex->dissassembly->operands[1]->jmp_addr);
				//loop->add_expr<ast_dec::expr_type::for_n_start>();
				forloop->add_expr<ast_dec::expr_type::scope_end>();

				loop->loop_extra.end_node = forloop;
				forloop->loop_extra.end_node = forloop;

			}

			return;
		}

		/*
			While and repeat loops are "pretty tricky" to detect. 
				* Repeat: The compare will come at the end of the routine and is near a jumpback instruction.
				* While:  The compare will come at the beginging of the routine but its jump will go past jumpback instruction.
		
			This issue with this is not determing which one it is, it is determing which one is a valid while,until expression then a if/elseif.
			Though some hueristics can be used to mitigate this problem and determine if its apart of the loop or not:
				* If the jump exceeds the jumpback opcode or goes directly too it.
				* If a variable doesn't get written between the next branch if there proceeds a jump.
				* Opcodes that shouldn't be in between the next branch and proceeding jump (ie. return/nop/etc).
				* Use of any of the compares used as dest between the next branch and proceeding jump.

		*/
		void set_whilerep_routines(std::shared_ptr<ast_dec::ast>& ast) {

			std::vector<std::shared_ptr<ast_dec::node>> nodes;

			const auto jump_backs = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_JUMPBACK>(true));
			const auto jump_conds = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_next_type<lexer_dec::inst_type::branch_condition>(true));


			/* Check typical jumpbacks *Previous inst is condition its until else end for while. */
			for (const auto& jmp_back : jump_backs) {

				if (ast->main_block->visit_previous_addr(jmp_back->address)->lex->type == lexer_dec::inst_type::branch_condition) {
					jmp_back->add_expr<ast_dec::expr_type::until_>(); /* Until end. */
					nodes.emplace_back(jmp_back);
				}
				else {
					jmp_back->add_expr<ast_dec::expr_type::scope_end>(); /* While loop end. */
					nodes.emplace_back(jmp_back);
				}

			}

			/* See if jump memaddrs are negatives usally means there until. */
			for (const auto& jmp_back : jump_conds) 
				if (jmp_back->lex->dissassembly->operands[std::find(jmp_back->lex->operands.begin(), jmp_back->lex->operands.end(), lexer_dec::operand_types::memaddr) - jmp_back->lex->operands.begin()]->jmp < 0) { /* See if mem address of jump is negative (We need to get idx of memaddr operand). */
					jmp_back->add_expr<ast_dec::expr_type::until_>(); /* Until end. */
					nodes.emplace_back(jmp_back);
				}


			/* We need to get range of until/while routines conditions. */
			for (const auto& jumpback : nodes) {

				std::shared_ptr<ast_dec::node> marked_node = nullptr;

				/* See if jumpback is a branch. */
				if (jumpback->lex->type != lexer_dec::inst_type::branch_condition && jumpback->lex->type != lexer_dec::inst_type::branch)
					throw std::runtime_error("until/while branch jumpback isn't a branch.");

				const auto jmp_addr = jumpback->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr;
				const auto jmp_node = ast->main_block->visit_addr(jmp_addr);
				const auto nodes_routine = ast->main_block->visit_range_current(jmp_addr, jumpback->address);
				const auto condition = ast->main_block->visit_expr_routine_range_touching<ast_dec::expr_type::condition_routine_start, ast_dec::expr_type::condition_routine_end>(jmp_addr, jumpback->address);




				if (jumpback->has_expr(ast_dec::expr_type::until_)) {

					/* No conditions in it (Garunteed repeat (true) do) */
					if (!condition.size()) {
						jmp_node->add_expr<ast_dec::expr_type::repeat_>();
						jumpback->add_expr<ast_dec::expr_type::condition_true>(1u, ast_dec::element::front);
					}
					else {

						const auto cond =  condition.back();

						if (cond.second->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr > jumpback->address) {
							cond.first->add_expr<ast_dec::expr_type::condition_concat_start>();
							cond.second->add_expr<ast_dec::expr_type::condition_concat_end>();
						}
						else {
							/* Last conditon doesn't jump out possible repeat until(true)*/
							jmp_node->add_expr<ast_dec::expr_type::repeat_>();
							jumpback->add_expr<ast_dec::expr_type::condition_true>(1u, ast_dec::element::front);
						}

					}				

				}
				else { /* While loop */

					/* No conditions in it (Garunteed repeat (true) do) */
					if (!condition.size()) {
						jmp_node->add_expr<ast_dec::expr_type::repeat_>();
						jumpback->add_expr<ast_dec::expr_type::condition_true>(1u, ast_dec::element::front);
					}
					else {
						const auto cond = condition.front();

						if (cond.second->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr > jumpback->address) {
							cond.first->add_expr<ast_dec::expr_type::condition_concat_start>();
							cond.second->add_expr<ast_dec::expr_type::condition_concat_end>();
						}
						else {
							/* Last conditon doesn't jump out possible repeat until(true)*/
							jmp_node->add_expr<ast_dec::expr_type::repeat_>();
							jumpback->add_expr<ast_dec::expr_type::condition_true>(1u, ast_dec::element::front);
						}

					}

				}

				/* Add breaks by getting second condition that doesn't have concat and marking. */
				for (const auto& c : condition) {

					if (!c.second->has_expr(ast_dec::expr_type::condition_concat_end) && c.second->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr > jumpback->address) {
						c.second->add_expr<ast_dec::expr_type::condition_concat_start>();
						c.second->add_expr<ast_dec::expr_type::condition_break>();
					}

				}

				/* End of routine is given with until. Find begging. */
				for (const auto& node : nodes_routine) {

					/* Break **Not definite jump to end of until routine or end of while can mean break of any conditional** */
					if (node->lex->dissassembly->op == LuauOpcode::LOP_JUMP || node->lex->dissassembly->op == LuauOpcode::LOP_JUMPX) {

						if (node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr > jumpback->address) {
							node->add_expr<ast_dec::expr_type::break_>();
						}

					}

				}

			}
			
			return;
		}
		
	}

	namespace tables {

		void set_routines(std::shared_ptr<ast_dec::ast>& ast) {

		    auto tables = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_NEWTABLE>(true));
			const auto duptables = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_DUPTABLE>(true));		
			tables.insert(tables.end(), duptables.begin(), duptables.end());

			std::vector<std::uintptr_t> ends; /* Used to avoid encapsulation with tables as this is for ends of already analyzed tables. */

			for (const auto& table : tables) {
			
				/* Check */
				bool valid = true;
				for (const auto i : ends) 
					if (table->lex->dissassembly->addr <= i)
						valid = false;
				

				if (!valid)
					continue;


				std::uint16_t reg = 0u; /* Previous register dest. */
				std::uint32_t nested_count = 0u; /* Used for node analysis. */
				std::uintptr_t node_size = 0u;
				std::uintptr_t array_size = 0u; 
				std::uintptr_t predicted_size = 0u; /* Previous power of 2 for array_size, gives us more context on end. */


				/* Sizes for scopes of table. (Used for scopes of nested tables) */
				std::vector<std::uintptr_t> predicted_sizes;
				std::vector<std::uintptr_t> node_sizes;
				std::vector<std::uintptr_t> array_sizes;



				/* Cache data for scopes. */
				auto cache = [&](const bool start /* Start new cache or set old cache? */) mutable -> void {

					if (start) {
						predicted_sizes.emplace_back(predicted_size);
						node_sizes.emplace_back(node_size);
						array_sizes.emplace_back(array_size);
					}
					else {

						predicted_sizes.pop_back();
						node_sizes.pop_back();
						array_sizes.pop_back();

						/* Check size */
						if (predicted_sizes.size())
							predicted_size = predicted_sizes.back();

						if (node_sizes.size())
							node_size = node_sizes.back();

						if (array_sizes.size())
							array_size = array_sizes.back();

					}

				};

				/* Set size for node_size and array_size also sets table elements/start/end. */
				auto set_size = [&](const std::shared_ptr<ast_dec::node>& node) mutable -> void {

					switch (node->lex->dissassembly->op) {

						case LuauOpcode::LOP_NEWTABLE: {
							
							const auto operands = node->lex->operand_expr<lexer_dec::operand_types::integer>();
							auto x = operands.front()->table_size;

							node_size = x;
							array_size = operands.back()->val;
				

							--x;
							x = x | (x >> 1);
							x = x | (x >> 2);
							x = x | (x >> 4);
							x = x | (x >> 8);
							x = x | (x >> 16);
							predicted_size = x - (x >> 1);

							cache(true);
							node->add_expr<ast_dec::expr_type::table_start>();
							
							break;
						}

						case LuauOpcode::LOP_DUPTABLE: {

							auto x = std::stoi(node->lex->dissassembly->operands[1]->k_value.c_str());
							node_size = x;

							--x;
							x = x | (x >> 1);
							x = x | (x >> 2);
							x = x | (x >> 4);
							x = x | (x >> 8);
							x = x | (x >> 16);
							predicted_size = x - (x >> 1);
							cache(true);
							node->add_expr<ast_dec::expr_type::table_start>();
							break;
						}


						case LuauOpcode::LOP_SETTABLE:
						case LuauOpcode::LOP_SETTABLEKS:
						case LuauOpcode::LOP_SETTABLEN: {
							--node_size;
							node->add_expr<ast_dec::expr_type::table_element>();
							break;
						}

						case LuauOpcode::LOP_SETLIST: {

							auto amt = node->lex->dissassembly->operands[2]->val;
							if (amt == -1)
								amt = reg;
							
							array_size -= std::uintptr_t (amt);
							
							if (predicted_sizes.size () > 1u && !node_size && !array_size) {
								node->add_expr<ast_dec::expr_type::table_end>();
								ends.emplace_back(node->address);
							}

							cache(false);

							break;
						}

						default: {
							break;
						}

					}

					return;
				};
				

		
				/* Add first info */
				if (table->lex->dissassembly->op == LuauOpcode::LOP_NEWTABLE || table->lex->dissassembly->op == LuauOpcode::LOP_DUPTABLE) {
					
					/* Has table members? */
					set_size(table);
					
					if (array_size || node_size) {

						const auto nodes = ast->main_block->visit_rest(table->address);
						for (const auto& node : nodes) {
					
						
							set_size(node);
		
							/* Set previous dest register. */
							if (node->lex->has_operand_expr<lexer_dec::operand_types::dest>())
								reg = node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;


							/* Node_size equals predicted_size or is less then predicted_size that means that it exceeded predicted_size. */
							if (node_size && predicted_size >= node_size) {


								/* Table set so check if it's the end. */
								if (node->lex->type == lexer_dec::inst_type::table_set) {


									/* Target table. */
									const auto table_reg = node->lex->operand_expr<lexer_dec::operand_types::reg>().front()->reg;

									const auto target_2 = (node->lex->dissassembly->op == LuauOpcode::LOP_SETTABLE) ? node->lex->operand_expr<lexer_dec::operand_types::table_idx>().front()->reg : -1; /* Index for SETTABLE. */
									const auto target_1 = node->lex->operand_expr<lexer_dec::operand_types::source>().front()->reg; /* Source data can be idx. */
									bool used_target_1 = false;
									bool used_target_2 = false;


									/* Really just visiting future instructions too see if table source, dest, or idx gets set twice indicating end. */
									const auto rest_nodes = ast->main_block->visit_rest(table->address);
									for (const auto& i : rest_nodes) {
										
										/* Checks operands if targets get used or not but if it does get used just resets target. */
										std::function<void(const std::shared_ptr<LuaU_dissassembler::operand>&, const lexer_dec::operand_types)> check_usage = [&](const std::shared_ptr<LuaU_dissassembler::operand>& operand, const lexer_dec::operand_types tt) mutable {
											
											const auto val = operand->reg;

											if (val == target_1)
												used_target_1 = false;

											if (target_2 != -1 && signed(val) == target_2)
												used_target_2 = false;

											return;
										};


										/* Check reg operands for usage. */
										i->lex->operand_expr_callback<lexer_dec::operand_types::source>(check_usage);
										i->lex->operand_expr_callback<lexer_dec::operand_types::compare>(check_usage);
										i->lex->operand_expr_callback<lexer_dec::operand_types::reg>(check_usage);

										/* New table instruction inc for nested. */
										if (i->lex->dissassembly->op == LuauOpcode::LOP_DUPTABLE || i->lex->dissassembly->op == LuauOpcode::LOP_NEWTABLE)
											++nested_count;

										if (i->lex->has_operand_expr<lexer_dec::operand_types::dest>()) {

											const auto dest = i->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;

											/* Check target usage. Abrubt end. */
											if (dest == target_1) {

												/* Set twice without used. Abrubt end. */
												if (used_target_1) {
													goto node_end;
												}

												used_target_1 = true;
											}

											if (target_2 != -1 && signed(dest) == target_2) {

												/* Set twice without used. Abrubt end. */
												if (used_target_2) {
													goto node_end;
												}

												used_target_2 = true;
											}

										}
										else {
											
											if (i->lex->type == lexer_dec::inst_type::table_set) {

												/* Table has same table source as current table source valid element.*/
												if (table_reg == i->lex->operand_expr<lexer_dec::operand_types::reg>().front()->reg) {
													--node_size;
												}
												else { /* New table */

													--node_size; /* Inc for node. */
													node->add_expr<ast_dec::expr_type::table_end>(); /* End of table. */
													cache(false); /* Revert back cache. */

													/* End of a nested table. */
													if (nested_count) {
														--nested_count;
													}
													else {  /* Nested table is 0 and we entered a new table. */
														break;
													}

													/* Target mets then its okay. */
													if (used_target_1 && ((target_2 != -1 && used_target_2) || target_2 == -1)) {
														break;
													}
	
													break;
												}

											}
											else {/* check for new table */
												goto node_end;
											}
											
										}

									}

								}

							}

							/* Found end, end anlysis. */
							if (!node_size && !array_size) {
							node_end:
								node->add_expr<ast_dec::expr_type::table_end>();
								ends.emplace_back(node->address);
								break;
							}

						}

					}
					else {
						/* No table members. */
						table->add_expr<ast_dec::expr_type::table_start>();
						table->add_expr<ast_dec::expr_type::table_end>();
						ends.emplace_back(table->address);
					}

				}
				else {
					throw std::runtime_error("Expected NEWTABLE or DUPTABLE instruction to init table.");
				}

			}

			return;
		}

	}

	namespace locvars {

		/*
		
			Sets locvars based on certain hueristics. More detailed info about it can be found in ast_config.hpp at control_flow_vars macro.
			Changing that will effect the behavior of this function but all the information you need can be found there but judge your discision
			on how you want the code to be in the decompilation. This isn't 100% perfect because where mostly judging on hueristics and nothing else.
			Depending on how the code is decompiled as of this version of luaU debug information contains stuff about variables (names, etc) but there isn't 
			a garunteed it will be present or not so this will try to recreat that.
		
		*/
		void set_lv(std::shared_ptr<ast_dec::ast>& ast, const std::uint16_t start_reg) {

			std::vector<std::uint16_t> registers = { start_reg }; /* Registers for scope. (Based on target register) */
			auto target = start_reg; /* Target register. */
			std::uintptr_t routine = 0u; /* Inside concat, call, table routine, inc for start, dec for end. */

			for (const auto& node : ast->main_block->visit_all()) {


				/* Turns node into variable. */
				auto node_var = [&](const std::shared_ptr<LuaU_dissassembler::operand>& operand) -> void {
				
					/* Not set yet. */
					if (operand->reg == target) {
						node->dest_loc.is_dest_loc = true;
						node->dest_loc.name = std::to_string(target);
						++target;
					}

					return;
				};


				/* End of scope. */
				for (auto i = 0u; i < node->count_expr <ast_dec::expr_type::scope_end>(); ++i)
					registers.pop_back();

				/* Log reg scope start. */
				if (node->lex->scope_start()) {
					target = registers.back();
					registers.emplace_back(target);
				}


				/* Inc for concat start, call start, and table start. Dec for concat end, call end, and start end. */
				routine += node->count_expr <ast_dec::expr_type::concat_routine_start>() + node->count_expr <ast_dec::expr_type::call_routine_start>() + node->count_expr <ast_dec::expr_type::table_start>();
				routine -= node->count_expr <ast_dec::expr_type::concat_routine_end>() + node->count_expr <ast_dec::expr_type::call_routine_end>() + node->count_expr <ast_dec::expr_type::table_end>();


				/* Not inside routine and dest. */
				if (!routine && node->lex->has_operand_expr<lexer_dec::operand_types::dest>()) {
					
					auto bad = false; /* Failed any checks. (Can also be used if node is already set. */
					const auto dest = node->lex->operand_expr<lexer_dec::operand_types::dest>().front ();


					/* Capture with source garunteeds locvar so check there. */
					const auto captures = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_CAPTURE>(true));
					for (const auto& capture : captures)
						if (capture->lex->has_operand_expr<lexer_dec::operand_types::source>() && capture->lex->operand_expr<lexer_dec::operand_types::source>().front ()->capture_reg == target) {
							node_var(dest);
							bad = true;
							break;
						}
					if (bad)
						continue;
					 
					
					/* Check concat and call routines if the dest is used as a dest in them no locvar. */
					const auto calls = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_next_expr<ast_dec::expr_type::call_routine_start>(node->address, true));
					for (const auto& call : calls) {
						
						const auto node_end = ast->main_block->visit_relative_next_expr<ast_dec::expr_type::call_routine_end>(call->address, { ast_dec::expr_type::call_routine_start });
			
						/* Target dest reg used in call routine dest. */
						for (const auto& call_node : ast->main_block->visit_range(call->address, node_end->address))
							if (call_node->lex->has_operand_expr<lexer_dec::operand_types::dest>() && call_node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg == target)
								bad = true;
					
					}
					if (bad)
						continue;
			
					/* Concat */
					const auto concats = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_next_expr<ast_dec::expr_type::concat_routine_start>(node->address, true));
					for (const auto& concat : concats) {

						const auto node_end = ast->main_block->visit_relative_next_expr<ast_dec::expr_type::concat_routine_end>(concat->address, { ast_dec::expr_type::concat_routine_start });

						/* Target dest reg used in call routine dest. */
						for (const auto& concat_node : ast->main_block->visit_range(concat->address, node_end->address))
							if (concat_node->lex->has_operand_expr<lexer_dec::operand_types::dest>() && concat_node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg == target)
								bad = true;

					}
					if (bad)
						continue;
					
					/* 
					
						After checking concat and call routines which can garunteed a variable this is where we mostly look at hueristics and decide if
						a register will become a variable or not. Either one it's set too will effect the preformance of the decompiled code and accuracy.
					
					*/


					/* Garunteed */
					if (node->has_expr(ast_dec::expr_type::table_end)) {
						node_var(node->lex->dissassembly->operands.front());
						continue;
					}

					/* TEST */
					if (dest->reg == target) {
						node_var(node->lex->dissassembly->operands.front());
						continue;
					}

				}
				else if (!routine && node->has_expr(ast_dec::expr_type::table_end)) { /* No routine and end of table garunteed locvar. */
					node_var(node->lex->dissassembly->operands.front());
				}

			}

			return;
		}

	}

	void init_ast(std::shared_ptr<ast_dec::ast>& ast) {

		#if display_analysis
				std::printf("[AST] Setting arguments.\n");
		#endif
		ast_funcs::arguments::set(ast);

		#if display_analysis
				std::printf("[AST] Setting call routines.\n");
		#endif
		ast_funcs::calls::set_routines(ast);

		#if display_analysis
				std::printf("[AST] Setting concat routines.\n");
		#endif
		ast_funcs::concats::set_routines(ast);

		#if display_analysis
				std::printf("[AST] Setting table routines.\n");
		#endif
		ast_funcs::tables::set_routines(ast);

		#if display_analysis
				std::printf("[AST] Setting valid branch routines.\n");
		#endif
		ast_funcs::branches::set_valid_branch_routine(ast);

		#if display_analysis
				std::printf("[AST] Setting while/repeat routines.\n");
		#endif
		ast_funcs::loops::set_whilerep_routines(ast); /* Needed after concat can mess up if before or after. (expr_type::scope_end needed only for while end) */
		
		#if display_analysis
				std::printf("[AST] Setting for routines.\n");
		#endif
		ast_funcs::loops::set_for_routines(ast);

		#if display_analysis
				std::printf("[AST] Setting locvars.\n");
		#endif
		ast_funcs::locvars::set_lv(ast, ast->arg_regs.size());

		return;
	}

	void post_ast(std::shared_ptr<ast_dec::ast>& ast) {

		#if display_analysis
				std::printf("[AST] Setting table node ends.\n");
		#endif
		ast_post::table::set_node_end(ast);

		#if display_analysis
				std::printf("[AST] Setting table indexes exprs.\n");
		#endif
		ast_post::table::set_indexs(ast);

		#if display_analysis
				std::printf("[AST] Setting arith exprs.\n");
		#endif
		ast_post::arith::set_arith_exprs(ast);

		return;
	}
}

namespace blocks {

	/* Set singular block *Jumpbacks act as end. */
	std::tuple <std::vector<std::shared_ptr<ast_dec::node>>, std::uintptr_t /* Start next pc. */, std::uintptr_t /* Final instruction. */> init_current_block(std::shared_ptr<ast_dec::ast>& ast, std::uintptr_t pc, std::vector<std::uintptr_t>& branch_ends) {
	
		std::vector<std::shared_ptr<ast_dec::node>> retn;

		/* Init basic node data. */
		do {
			
			auto current_dissassembly = ast->dissassembly[pc];

			/* Pc is already at a branch ending. So just return empty vector. */
			if (std::binary_search(branch_ends.begin(), branch_ends.end(), pc)) {
				branch_ends.erase(std::remove(branch_ends.begin(), branch_ends.end(), pc), branch_ends.end());
				return std::make_tuple(retn, pc, pc);
			}

			/* Set current node. */
			auto node = std::make_shared<ast_dec::node>();
			retn.emplace_back(node);

			/* Set node data. */
			node->address = pc;
			node->lex = lexer_dec::lexer(current_dissassembly);
			
			#if display_analysis
				std::printf("[AST-dissassembly] %llu %s ", pc, current_dissassembly->data.c_str());
				if (node->lex->type == lexer_dec::inst_type::branch || node->lex->type == lexer_dec::inst_type::branch_condition) {
					std::printf(" - %llu\n", node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr);
				}
				else {
					std::printf("\n");
				}
			#endif	

			/* Branch/end so break. */
			if (node->lex->type == lexer_dec::inst_type::branch || node->lex->type == lexer_dec::inst_type::branch_condition || (pc + current_dissassembly->len) == ast->p->sizecode)
				break;		

			pc += current_dissassembly->len;

		} while (true /* Earlier code will exit if hit a branch or passed branch ends. */);
		
		return std::make_tuple(retn, pc + ast->dissassembly[pc]->len, pc);
	}


	/* Set blocks for current ast. */
	void set_blocks(std::shared_ptr<ast_dec::ast>& ast) {

		std::uintptr_t pc = 0u;
		std::vector<std::uintptr_t> branch_ends;
		std::vector<std::uintptr_t> branch_ends_clone; /* Same as branch ends but a clone used for certain things. */
		std::unordered_map <std::uintptr_t /* PC(start) */, std::tuple <std::uintptr_t  /* PC(end) */, std::uintptr_t  /* PC(end(end + curr->len)) */, std::vector<std::shared_ptr<ast_dec::node>> /* Nodes */>> linear_blocks; /* Block data for scopes. */


		/* Set each end as every branch taken pc. */
		for (auto& dism : ast->dissassembly) {

			const auto temp_lex = lexer_dec::lexer(dism.second);

			/* Don't log if jumpback. */
			if (temp_lex->dissassembly->op == LuauOpcode::LOP_JUMPBACK) {
				continue;
			}

			if (temp_lex->type == lexer_dec::inst_type::branch || temp_lex->type == lexer_dec::inst_type::branch_condition) {

				/* Jump with no memaddr operand idk how this has happened. */
				if (!temp_lex->has_operand_expr<lexer_dec::operand_types::memaddr>()) {
					throw std::runtime_error("Jump with no memaddr operand in lexer at set_blocks.");
				}

				/* Jump is negative don't take. */
				if (temp_lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp < 0) {
					continue;
				}

				branch_ends.emplace_back(temp_lex->operand_expr<lexer_dec::operand_types::memaddr>().front ()->jmp_addr);
				
			}
		
		}

		/* Clone branch ends. */
		branch_ends_clone.reserve(branch_ends.size());
		std::copy(branch_ends.begin(), branch_ends.end(), branch_ends_clone.begin());


		/* Set main block. */
		ast->main_block = std::make_shared<ast_dec::block>();


		/* Append blocks */
		do {
			const auto block = init_current_block(ast, pc, branch_ends_clone);
			linear_blocks.insert(std::make_pair(pc, std::make_tuple(std::get<2>(block), std::get<1>(block), std::get<0>(block))));
			pc = std::get<1>(block);
		} while (pc < ast->p->sizecode && (pc + ast->dissassembly[pc]->len) < ast->p->sizecode /* Pc didn't exceed sizecode. */);

		
		/* Reset PC. */
		pc = 0u;

		/* Init main (Block search is dependent on main so needs to be seperate from everything). */
		ast->main_block->node_start = pc;
		ast->main_block->node_end = std::get<0>(linear_blocks[pc]);
		ast->main_block->nodes = std::get<2>(linear_blocks[pc]);
		

		/* Assemble blocks */
		while (pc < ast->p->sizecode /* Pc didn't exceed sizecode. */) {

			const auto node_block = linear_blocks[pc];
			const auto nodes = std::get<2>(node_block);

	        /* Adbrupt end.*/
			if (!nodes.size()) {
				break;
			}

			/* End */
			const auto jump_node = nodes.back();
			if (jump_node->lex->type == lexer_dec::inst_type::branch || jump_node->lex->type == lexer_dec::inst_type::branch_condition) {

				const auto jmp = jump_node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp + jump_node->address + 1u;
				const auto next_node_block = linear_blocks[jmp];

				auto current = ast->find_block(pc); /* Block too place all info in. */
				auto jump_block = ast->find_block(jmp); /* Jump taken block */
				auto nojump_block = ast->find_block(jump_node->address + jump_node->lex->dissassembly->len); /* Branch not taken jump. */

				/* Current is always available something bad happened. */
				if (current == nullptr) {
					throw std::runtime_error("Current block is null.");
				}


				switch (jump_node->lex->type) {

					case lexer_dec::inst_type::branch: {

						/* Construct branch taken. */
						if (jump_block == nullptr) {
							jump_block = std::make_shared<ast_dec::block>();
							jump_block->node_start = jmp;
							jump_block->node_end = std::get<0>(linear_blocks[jmp]);
							jump_block->nodes = std::get<2>(linear_blocks[jmp]);
						}

						/* Add jump taken. */
						current->branches.emplace_back(jump_block);
						break;
					}

					case lexer_dec::inst_type::branch_condition: {

						/* Construct branch taken. */
						if (jump_block == nullptr) {
							jump_block = std::make_shared<ast_dec::block>();
							jump_block->node_start = jmp;
							jump_block->node_end = std::get<0>(linear_blocks[jmp]);
							jump_block->nodes = std::get<2>(linear_blocks[jmp]);
						}

						/* Construct no branch taken. */
						if (nojump_block == nullptr) {
							nojump_block = std::make_shared<ast_dec::block>();
							nojump_block->node_start = jump_node->address + jump_node->lex->dissassembly->len;
							nojump_block->node_end = std::get<0>(linear_blocks[jump_node->address + jump_node->lex->dissassembly->len]);
							nojump_block->nodes = std::get<2>(linear_blocks[jump_node->address + jump_node->lex->dissassembly->len]);
						}


						/* Add jump taken. */
						current->branches.emplace_back(jump_block);
						current->branches.emplace_back(nojump_block);

						break;
					}

					default: {
						throw std::runtime_error("Unexpected jump inst type.");
					}

				}

			}

			/* Set pc */
			pc = std::get<1>(node_block);

		};

		

		return;
	}

}

std::shared_ptr<ast_dec::ast> ast_dec::gen_ast(Proto* proto) {

	/* Current ast. */
	auto retn = std::make_shared<ast>();
	std::vector<std::shared_ptr<ast>> protos_ast = { retn }; /* All protos including nested protos. */

	/* Set closure and proto. */
	retn->closure_type = closure_type::main;
	retn->p = proto;
	
	do {


		std::uintptr_t pc = 0u;
		auto current_proto = protos_ast[0];

		/* Set current proto dissasembly. */
		for (auto i = 0u; i < unsigned (proto->sizecode);) {
			auto dism = std::make_shared<LuaU_dissassembler::dissassembly>();
			LuaU_dissassembler::dissassemble(pc, current_proto->p, dism);
			current_proto->dissassembly.insert(std::make_pair(pc, dism));
			pc += dism->len;
			i += dism->len;
		}

		/* Set current proto blocks. */
		#if display_analysis
				std::printf("[AST] Initing blocks.\n");
		#endif
		blocks::set_blocks(current_proto);

		/* Gen children proto to analyze. */
		for (auto i = 0u; i < unsigned (current_proto->p->sizep); ++i) {
			
			/* Make child ast and add proto and get type. */
			auto child_ast = std::make_shared<ast>();
			child_ast->p = current_proto->p->p[i];
			ast_funcs::proto::set_closure_info(current_proto, i);

			/* Add child to get analyzed. */
			protos_ast.emplace_back(child_ast);
		}

		#if display_analysis
				std::printf("[AST] Initing ast.\n");
		#endif
		ast_funcs::init_ast(current_proto); /* Init ast */


		#if display_analysis
				std::printf("[AST] Post processing ast.\n");
		#endif
		ast_funcs::post_ast(current_proto); /* Post ast */


		#if display_analysis
				std::printf("[AST] Setting end pc.\n");
		#endif
		current_proto->pc_end = current_proto->main_block->visit_all().back()->address;
		

		#if display_analysis
				std::printf("[AST] Finished with current ast.\n");
		#endif
		std::cout << current_proto->tree_str() << std::endl;

		/* Remove current. */
		protos_ast.erase(std::remove(protos_ast.begin(), protos_ast.end(), current_proto), protos_ast.end());


	} while (protos_ast.size());

	return retn;
}