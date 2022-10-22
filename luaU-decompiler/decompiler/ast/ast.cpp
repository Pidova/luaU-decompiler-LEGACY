#include "ast.hpp"


namespace block {

	void set_blocks(std::shared_ptr<ast::ast>& current_proto) {

	}
}

namespace proto {

	void set_closure_info(const std::shared_ptr<ast::ast>& current_proto, const std::size_t child_proto_id) {

		std::shared_ptr<ast::node> closure_node = nullptr;

		/* Newclosure, Dupclosures node. */
		auto closures = std::get<std::vector<std::shared_ptr<ast::node>>>(current_proto->main_block->visit_inst<LuauOpcode::LOP_NEWCLOSURE>(true));
		const auto dupclosures = std::get<std::vector<std::shared_ptr<ast::node>>>(current_proto->main_block->visit_inst<LuauOpcode::LOP_DUPCLOSURE>(true));
		
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
					const auto proto = gco2cl(current_proto->p->k[i->lex->dissassembly->operands[1]->k_idx].value.gc)->l.p;
					if (current_proto->p->p[child_proto_id] == proto)
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

			switch (e) {
			
				/* Comes with closure name. */
				case ast::expr_type::closure_global: {
					proto->closure_type = ast::closure_type::global;
					proto->closure_name = std::get<std::shared_ptr<ast::node>>(current_proto->main_block->visit_next_inst<LuauOpcode::LOP_SETGLOBAL>(closure_node->address, false))->lex->dissassembly->operands[2]->k_value;
					break;
				}
			
				/* Closure name doesn't get compiled unless specified. */
				case ast::expr_type::closure_local: {
					proto->closure_type = ast::closure_type::local;
					break;
				}
			
				/* No closure name. */
				case ast::expr_type::closure_newclosure: {
					proto->closure_type = ast::closure_type::newclosure;
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

std::shared_ptr<ast::ast> ast::gen_ast(Proto* proto) {

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

		block::set_blocks()

		/* Gen children proto to analyze. */
		for (auto i = 0u; i < unsigned (current_proto->p->sizep); ++i) {
			
			/* Make child ast and add proto and get type. */
			auto child_ast = std::make_shared<ast>();
			child_ast->p = current_proto->p->p[i];
			proto::set_closure_info(current_proto, i);

			/* Add child to get analyzed. */
			protos_ast.emplace_back(child_ast);
		}


		/* Remove current. */
		protos_ast.erase(std::remove(protos_ast.begin(), protos_ast.end(), current_proto), protos_ast.end());


	} while (protos_ast.size());

	return retn;
}