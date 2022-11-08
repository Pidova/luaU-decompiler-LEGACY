#include <algorithm>
#include "ast_dec.hpp"

namespace ast_funcs {

	namespace concats {

		void set_routines(std::shared_ptr<ast_dec::ast>& ast) {

			const auto concats = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_CONCAT>(true));

			/* Set concat info. */
			for (const auto& node : concats) {
				ast->main_block->visit_previous_dest_register(node->address, node->lex->dissassembly->operands.front()->reg)->add_expr<ast_dec::expr_type::concat_routine_start>(1u); /* Concat start. */
				node->add_expr<ast_dec::expr_type::concat_routine_end>(1u); /* Concat end. */
			}

			return;
		}

	}

	namespace calls {

		void set_routines(std::shared_ptr<ast_dec::ast>& ast) {

			const auto calls = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_CALL>(true));
			
			/* Set call info. */
			for (const auto& node : calls) {
					ast->main_block->visit_previous_dest_register(node->address, node->lex->dissassembly->operands.front()->reg)->add_expr<ast_dec::expr_type::call_routine_start>(1u); /* Call start. */
					node->add_expr<ast_dec::expr_type::call_routine_end>(1u); /* Call end. */
			}

			return;
		}

	}

	namespace loops {

		void set_for_routines(std::shared_ptr<ast_dec::ast>& ast) {

			const auto forgloops = std::get<std::vector<std::shared_ptr<ast_dec::node>>> (ast->main_block->visit_inst<LuauOpcode::LOP_FORGLOOP>(true));
			const auto fornloops = std::get<std::vector<std::shared_ptr<ast_dec::node>>> (ast->main_block->visit_inst<LuauOpcode::LOP_FORNLOOP>(true));
		
			/* Forgloops. */
			for (const auto& forloop : forgloops) {

				const auto jump_node = ast->main_block->visit_addr(forloop->address + forloop->lex->dissassembly->operands[1]->jmp);
				const auto jump_inst = jump_node->lex->dissassembly->op;

				/* for i,v in ipairs/pairs */
				if (jump_inst == LuauOpcode::LOP_FORGPREP_INEXT || jump_inst == LuauOpcode::LOP_FORGPREP_NEXT)
					jump_node->add_expr<ast_dec::expr_type::for_iv_start>(1u);
				else
					jump_node->add_expr<ast_dec::expr_type::for_start>(1u);

				forloop->add_expr<ast_dec::expr_type::for_end>(1u);

			}

			/* Fornloops. */
			for (const auto& forloop : fornloops) {

				const auto jump_node = ast->main_block->visit_addr(forloop->address + forloop->lex->dissassembly->operands[1]->jmp);
				const auto jump_inst = jump_node->lex->dissassembly->op;

				jump_node->add_expr<ast_dec::expr_type::for_start>(1u);
				forloop->add_expr<ast_dec::expr_type::for_end>(1u);

			}

			return;
		}

		void set_whilerep_routines(std::shared_ptr<ast_dec::ast>& ast) {

			const auto jump_backs = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_JUMPBACK>(true));
			const auto jump_conds = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_next_inst<lexer_dec::inst_type::branch_condition>(true));


			/* Check typical jumpbacks *Previous inst is condition its until else end for while. */
			for (const auto& jmp_back : jump_backs) {

				if (ast->main_block->visit_previous_addr(jmp_back->address)->lex->type == lexer_dec::inst_type::branch_condition)
					jmp_back->add_expr<ast_dec::expr_type::until_>(1u); /* Until end. */
				else
					jmp_back->add_expr<ast_dec::expr_type::end_>(1u); /* While loop end. */

			}

			/* See if jump memaddrs are negatives usally means there until. */
			for (const auto& jmp_back : jump_conds) 
				if (jmp_back->lex->dissassembly->operands[std::find(jmp_back->lex->operands.begin(), jmp_back->lex->operands.end(), lexer_dec::operand_types::memaddr) - jmp_back->lex->operands.begin()]->jmp < 0) /* See if mem address of jump is negative (We need to get idx of memaddr operand). */
					jmp_back->add_expr<ast_dec::expr_type::until_>(1u); /* Until end. */

			return;
		}
		
	}

	namespace tables {

		void set_routines(std::shared_ptr<ast_dec::ast>& ast) {

			auto table_add_info = [&](const std::shared_ptr<ast_dec::node> node, auto size, auto predicted_size) -> void {

				if (node->lex->dissassembly->op == LuauOpcode::LOP_NEWTABLE) {

					auto x = node->lex->dissassembly->operands[1]->table_size;
					size += x;

					--x;
					x = x | (x >> 1);
					x = x | (x >> 2);
					x = x | (x >> 4);
					x = x | (x >> 8);
					x = x | (x >> 16);
					predicted_size += x - (x >> 1);

				}
				else {

					size += std::stoi(node->lex->dissassembly->operands[1]->k_value.c_str());

				}

				return;
			};

		    auto tables = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_NEWTABLE>(true));
			const auto duptables = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(ast->main_block->visit_inst<LuauOpcode::LOP_DUPTABLE>(true));		
			tables.insert(tables.end(), duptables.begin(), duptables.end());

			for (const auto& table : tables) {

				std::uintptr_t table_size = 0u; 
				std::uintptr_t predicted_size = 0u;

				table->add_expr<ast_dec::expr_type::table_start>(1u);

				if (table->lex->dissassembly->op != LuauOpcode::LOP_NEWTABLE || table->lex->dissassembly->operands[1]->table_size) {
					/* Has table members. */

					/* Set size. */
					auto set_size = [&](const std::shared_ptr<ast_dec::node>& node) mutable -> void {

						if (node->lex->dissassembly->op == LuauOpcode::LOP_NEWTABLE) {

							auto x = node->lex->dissassembly->operands[1]->table_size;
							table_size += x;

							--x;
							x = x | (x >> 1);
							x = x | (x >> 2);
							x = x | (x >> 4);
							x = x | (x >> 8);
							x = x | (x >> 16);
							predicted_size += x - (x >> 1);

						}
						else {

							table_size += std::stoi(node->lex->dissassembly->operands[1]->k_value.c_str());

						}

						return;
					};
					set_size(table);

					const auto nodes = ast->main_block->visit_rest(table->address);

					auto reg = 0u; /* Previous register dest. */

					for (const auto& node : nodes) {

						/* bruh */
						if (node->lex->dissassembly->op == LuauOpcode::LOP_SETLIST)
							table_size -= node->lex->dissassembly->operands[2]->val;
			

						/* Set previous dest register. */
						if (node->lex->has_operand_expr<lexer_dec::operand_types::dest>())
							reg = node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg;
						
					}

				}
				else {
					/* No table members. */
					table->add_expr<ast_dec::expr_type::table_start>(1u);
					table->add_expr<ast_dec::expr_type::table_end>(1u);
				}

			}

			return;
		}

	}

	namespace locvars {

		/* Sets locvar by each register dest +1 with certain specifications. */
		void set_lv(std::shared_ptr<ast_dec::ast>& ast, const std::uint16_t start_reg) {

			std::vector<std::uint16_t> registers = { start_reg }; /* Registers for scope. */
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
				for (auto i = 0u; i < node->count_expr <ast_dec::expr_type::end_>(); ++i)
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

					auto bad = false; /* Failed any checks. (Can also be used if node is alr set. */
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

						const auto node_end = ast->main_block->visit_relative_next_expr<ast_dec::expr_type::call_routine_start>(concat->address, { ast_dec::expr_type::concat_routine_start });

						/* Target dest reg used in call routine dest. */
						for (const auto& concat_node : ast->main_block->visit_range(concat->address, node_end->address))
							if (concat_node->lex->has_operand_expr<lexer_dec::operand_types::dest>() && concat_node->lex->operand_expr<lexer_dec::operand_types::dest>().front()->reg == target)
								bad = true;

					}
					if (bad)
						continue;



				}

			}

			return;
		}

	}

	void init_ast(std::shared_ptr<ast_dec::ast>& ast) {
		ast_funcs::calls::set_routines(ast);
		ast_funcs::concats::set_routines(ast);
		ast_funcs::loops::set_for_routines(ast);
		ast_funcs::loops::set_whilerep_routines(ast);
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
			if (std::binary_search(branch_ends.begin(), branch_ends.end(), pc))
				break;

			/* Set current node. */
			auto node = std::make_shared<ast_dec::node>();
			retn.emplace_back(node);

			/* Set node data. */
			node->address = pc;
			node->lex = lexer_dec::lexer(current_dissassembly);
			
			/* Branch/end so break. */
			if (node->lex->type == lexer_dec::inst_type::branch || node->lex->type == lexer_dec::inst_type::branch_condition || pc == ast->p->sizecode)
				break;		

			pc += current_dissassembly->len;

		} while (true /* Earlier code will exit if hit a branch or passed branch ends. */);


		return std::make_tuple(retn, pc + retn.back()->lex->dissassembly->len /* Skip current instruction. */, pc);
	}


	/* Set blocks for current ast. */
	void set_blocks(std::shared_ptr<ast_dec::ast>& ast) {

		std::uintptr_t pc = 0u;
		std::vector<std::uintptr_t> branch_ends;
		std::unordered_map <std::uintptr_t /* PC(start) */, std::tuple <std::uintptr_t  /* PC(end) */, std::uintptr_t  /* PC(end(end + curr->len)) */, std::vector<std::shared_ptr<ast_dec::node>> /* Nodes */>> linear_blocks; /* Block data for scopes. */


		/* Set each end as every branch taken pc. */
		for (auto& dism : ast->dissassembly) {
			
			const auto temp_lex = lexer_dec::lexer(dism.second);

			/* Don't log if jumpback. */
			if (temp_lex->dissassembly->op == LuauOpcode::LOP_JUMPBACK)
				continue;

			if (temp_lex->type == lexer_dec::inst_type::branch || temp_lex->type == lexer_dec::inst_type::branch_condition) {

				/* Jump with no memaddr operand idk how this has happened. */
				if (!temp_lex->has_operand_expr<lexer_dec::operand_types::memaddr>())
					throw std::exception("Jump with no memaddr operand in lexer at set_blocks.");

				branch_ends.emplace_back(dism.first + temp_lex->operand_expr<lexer_dec::operand_types::memaddr>().front ()->jmp);
				
			}
		
		}


		/* Set main block. */
		ast->main_block = std::make_shared<ast_dec::block>();


		/* Append blocks */
		do {

			const auto block = init_current_block(ast, pc, branch_ends);
			linear_blocks.insert(std::make_pair(pc, std::make_tuple(std::get<2>(block), std::get<1>(block), std::get<0>(block))));

			pc = std::get<1>(block);

		} while (pc < ast->p->sizecode /* Pc didn't exceed sizecode. */);


		/* Reset PC. */
		pc = 0u;
		

		/* Init main. (Block search is dependent on main so needs to be seperate from everything). */
		ast->main_block->node_start = pc;
		ast->main_block->node_end = std::get<0>(linear_blocks[pc]);
		ast->main_block->nodes = std::get<2>(linear_blocks[pc]);


		/* Set pc. */
		pc = std::get<1>(linear_blocks[pc]);;


		/* Assemble blocks. */
		while (pc < ast->p->sizecode /* Pc didn't exceed sizecode. */) {

			const auto block = linear_blocks[pc];

		};

		return;
	}

}

namespace proto {

	void set_closure_info(const std::shared_ptr<ast_dec::ast>& current_proto, const std::size_t child_proto_id) {

		std::shared_ptr<ast_dec::node> closure_node = nullptr;

		/* Newclosure, Dupclosures node. */
		auto closures = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(current_proto->main_block->visit_inst<LuauOpcode::LOP_NEWCLOSURE>(true));
		const auto dupclosures = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(current_proto->main_block->visit_inst<LuauOpcode::LOP_DUPCLOSURE>(true));
		
		/* All closures. */
		closures.insert(closures.end(), dupclosures.begin(), dupclosures.end());
		
		/* Iterate through closures and get expression for it relative to proto given. */
		for (const auto i : closures) {

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
					throw std::exception("Unkown opcode for closure_type.");
				}

			}

		}

		/* Turn expression to closure type. */
		for (const auto e : closure_node->expr) {

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
					throw std::exception("Unkown expression for closure_type.");
				}
			
			}

		}

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
		for (auto i = 0u; i < unsigned (proto->sizecode); ++i) {		
			auto dism = std::make_shared<LuaU_dissassembler::dissassembly>();
			LuaU_dissassembler::dissassemble(pc, current_proto->p, dism);

			current_proto->dissassembly.insert(std::make_pair(pc, dism));
			pc += dism->len;
		}

		/* Set current proto blocks. */
		blocks::set_blocks(current_proto);

		/* Gen children proto to analyze. */
		for (auto i = 0u; i < unsigned (current_proto->p->sizep); ++i) {
			
			/* Make child ast and add proto and get type. */
			auto child_ast = std::make_shared<ast>();
			child_ast->p = current_proto->p->p[i];
			proto::set_closure_info(current_proto, i);

			/* Add child to get analyzed. */
			protos_ast.emplace_back(child_ast);
		}

		/* Init ast. */
		ast_funcs::init_ast(current_proto);

		/* Remove current. */
		protos_ast.erase(std::remove(protos_ast.begin(), protos_ast.end(), current_proto), protos_ast.end());


	} while (protos_ast.size());

	return retn;
}