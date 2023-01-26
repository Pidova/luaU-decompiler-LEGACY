#include "ast_dec.hpp"
#include "../emitter/emitter.hpp"
#include "../generic/generic.hpp"
#include "ast_functions/ast_functions.hpp"
#include "ast_macros.hpp"
#include "post_ast.hpp"
#include <algorithm>

namespace ast_init {

      void init_ast(std::shared_ptr<ast_dec::ast> &ast) {

#if display_analysis
            std::printf("[AST] Return instruction(s) exprs.\n");
#endif
            ast_funcs::instructions::set_return_exprs(ast);

#if display_analysis
            std::printf("[AST] Setting table exprs.\n");
#endif
            ast_funcs::tables::set_table_exprs(ast);

#if display_analysis
            std::printf("[AST] For prep exprs.\n");
#endif
            ast_funcs::loops::set_for_prep_exprs(ast);

#if display_analysis
            std::printf("[AST] Setting for routines.\n");
#endif
            ast_funcs::loops::set_for_routines(ast);

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
            std::printf("[AST] Setting arguments for children proto.\n");
#endif
            ast_funcs::arguments::set(ast);

#if display_analysis
            std::printf("[AST] Setting logical operations.\n");
#endif
            ast_funcs::locvars::set_logical_operations(ast);

#if display_analysis
            std::printf("[AST] Setting logical expressions.\n");
#endif
            ast_funcs::locvars::set_logical_expression(ast);

#if display_analysis
            std::printf("[AST] Setting if/elseif/else routines.\n");
#endif
            ast_funcs::branches::set_branch_statements(ast);

#if display_analysis
            std::printf("[AST] Setting locvars.\n");
#endif
            ast_funcs::locvars::set_lv(ast, ast->arg_regs.size());

#if display_analysis
            std::printf("[AST] Setting upvalues.\n");
#endif
            ast_funcs::upvalues::set(ast);

#if display_analysis
            std::printf("[AST] Sorting loops.\n");
#endif
            ast_funcs::loops::sort_loops(ast);

#if display_analysis
            std::printf("[AST] Setting call multret routines.\n");
#endif
            ast_funcs::calls::set_multret_routines(ast);

            return;
      }

      void post_ast(std::shared_ptr<ast_dec::ast> &ast) {

#if display_analysis
            std::printf("[POST-AST] Setting table node ends.\n");
#endif
            ast_post::table::set_node_end(ast);

#if display_analysis
            std::printf("[POST-AST] Setting table indexes exprs.\n");
#endif
            ast_post::table::set_indexs(ast);

#if display_analysis
            std::printf("[POST-AST] Setting arith exprs.\n");
#endif
            ast_post::arith::set_arith_exprs(ast);

            return;
      }

} // namespace ast_init

namespace blocks {

      /* Set singular block *Jumpbacks act as end. */
      std::tuple<std::vector<std::shared_ptr<ast_dec::node>>, std::uintptr_t /* Start next pc. */, std::uintptr_t /* Final instruction. */> init_current_block(std::shared_ptr<ast_dec::ast> &ast, std::uintptr_t pc, std::vector<std::uintptr_t> &branch_ends) {

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

#if display_dissassembly
                  std::printf("[AST-dissassembly] %" PRIuPTR " %s ", pc, current_dissassembly->data.c_str());
                  if (node->lex->type == lexer_dec::inst_type::branch || node->lex->type == lexer_dec::inst_type::branch_condition) {
                        std::printf(" - %" PRIuPTR "\n", node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr);
                  } else {
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
      void set_blocks(std::shared_ptr<ast_dec::ast> &ast) {

            std::uintptr_t pc = 0u;
            std::vector<std::uintptr_t> branch_ends;
            std::vector<std::uintptr_t> branch_ends_clone;                                                                                                                                                                      /* Same as branch ends but a clone used for certain things. */
            std::unordered_map<std::uintptr_t /* PC(start) */, std::tuple<std::uintptr_t /* PC(end) */, std::uintptr_t /* PC(end(end + curr->len)) */, std::vector<std::shared_ptr<ast_dec::node>> /* Nodes */>> linear_blocks; /* Block data for scopes. */

            /* Set each end as every branch taken pc. */
            for (auto &dism : ast->dissassembly) {

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

                        branch_ends.emplace_back(temp_lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr);
                  }
            }

            /* Clone branch ends. */
            branch_ends_clone.reserve(branch_ends.size());
            std::copy(branch_ends.begin(), branch_ends.end(), branch_ends_clone.begin());

            /* Set main block. */
            ast->main_block = std::make_shared<ast_dec::block>();

            /* Append blocks */
            do {

#if display_analysis_blocks
                  std::printf("[AST] Begin block.\n");
#endif

                  const auto block = init_current_block(ast, pc, branch_ends_clone);
                  linear_blocks.insert(std::make_pair(pc, std::make_tuple(std::get<2>(block), std::get<1>(block), std::get<0>(block))));
                  pc = std::get<1>(block);

#if display_analysis_blocks
                  std::printf("[AST] End block.\n");
                  std::printf("[AST] Next pc: %" PRIuPTR " size: %" PRIi32 "\n", pc, ast->p->sizecode);
#endif

            } while (pc < ast->p->sizecode);

            /* Reset PC. */
            pc = 0u;

            /* Init main (Block search is dependent on main so needs to be seperate from everything). */
            ast->main_block->node_start = pc;
            ast->main_block->node_end = std::get<0>(linear_blocks[pc]);
            ast->main_block->nodes = std::get<2>(linear_blocks[pc]);
            ast->add_block(ast->main_block);

            std::vector<std::uintptr_t> analyzed_scopes; /* Used to prevent infinite loops when doing visits. */

            /* Assemble blocks */
            while (pc < ast->p->sizecode /* Pc didn't exceed sizecode. */) {

                  const auto node_block = linear_blocks[pc];
                  const auto nodes = std::get<2>(node_block);

                  /* Adbrupt end */
                  if (!nodes.size()) {
                        break;
                  }

                  analyzed_scopes.emplace_back(pc);

                  /* End */
                  const auto jump_node = nodes.back();
                  if (jump_node->lex->type == lexer_dec::inst_type::branch || jump_node->lex->type == lexer_dec::inst_type::branch_condition) {

                        const auto jmp = jump_node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr;
                        const auto next_node_block = linear_blocks[jmp];

                        auto current = ast->find_block(pc);                                                          /* Block too place all info in. */
                        auto jump_block = ast->find_block(jmp);                                                      /* Jump taken block */
                        auto nojump_block = ast->find_block(jump_node->address + jump_node->lex->dissassembly->len); /* Branch not taken jump. */

                        /* Current is always available something bad happened. */
                        if (current == nullptr) {

/* Either something bad has happen or dead instruction. (Will never get executed no matter what branch is taken or not) */
#if display_warnings
                              std::printf("[WARNING] Dead instruction: %" PRIuPTR " %s\n", ast->dissassembly[pc]->addr, ast->dissassembly[pc]->data.c_str());
#endif

                              /* Get previous from pc */
                              std::uintptr_t key = 0u;
                              for (const auto &i : ast->dissassembly) {
                                    if (i.first < pc && key < i.first) {
                                          key = i.first;
                                    }
                              }

#if display_warnings
                              std::printf("[WARNING] Appending dead instruction to block with: pc = %" PRIuPTR " at instruction : %s\n", key, ast->dissassembly[key]->data.c_str());
#endif

                              current = std::make_shared<ast_dec::block>();
                              current->node_start = pc;
                              current->node_end = pc;
                              current->nodes = std::get<2>(linear_blocks[pc]);

                              ast->find_block_addr(key)->branches.emplace_back(current);

                              /* Propegate with bad instruction expr. */
                              for (const auto &node : current->nodes) {
                                    node->add_expr<ast_dec::expr_type::bad_instruction>();
                              }
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
                                    if (jump_node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp > 0 || std::find(analyzed_scopes.begin(), analyzed_scopes.end(), jmp) == analyzed_scopes.end()) {
                                          current->branches.emplace_back(jump_block);
                                          ast->add_block(jump_block);
                                    }

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
                                    if (jump_node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp > 0 || std::find(analyzed_scopes.begin(), analyzed_scopes.end(), jmp) == analyzed_scopes.end()) {
                                          current->branches.emplace_back(jump_block);
                                          ast->add_block(jump_block);
                                    }

                                    current->branches.emplace_back(nojump_block);
                                    ast->add_block(nojump_block);

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

} // namespace blocks

std::shared_ptr<ast_dec::ast> ast_dec::gen_ast(Proto *proto, const std::shared_ptr<transpiler_data::transpiler_config> &config) {

      std::uintptr_t pc = 0u;

      /* Current ast. */
      auto retn = std::make_shared<ast>();
      std::vector<std::shared_ptr<ast>> protos_ast = {retn}; /* All protos including nested protos. */

      /* Set closure and proto. */
      retn->closure_type = closure_type::main;
      retn->p = proto;
      retn->transpiler_config = config;

#if display_analysis
      std::printf("[AST] Initing main dissassembly.\n");
#endif
      /* Set current proto dissasembly. */
      for (auto i = 0u; i < unsigned(proto->sizecode);) {
            auto dism = std::make_shared<LuaU_dissassembler::dissassembly>();
            LuaU_dissassembler::dissassemble(pc, proto, dism);
            retn->dissassembly.insert(std::make_pair(pc, dism));
            pc += dism->len;
            i += dism->len;
            retn->total += dism->total;
      }

/* Set current proto blocks. */
#if display_analysis
      std::printf("[AST] Initing main blocks.\n");
#endif
      blocks::set_blocks(retn);

      do {

            debug_init("AST");

            auto current_proto = protos_ast.front();
            auto ast_id = current_proto->ast_id;

#if display_analysis
            std::printf("[AST] Initing proto [%" PRIuPTR "].\n", reinterpret_cast<std::uintptr_t>(current_proto.get()));
#endif

/* Gen children proto to analyze. */
#if display_analysis
            std::printf("[AST] Setting child proto information.\n");
#endif
            for (auto i = 0u; i < unsigned(current_proto->p->sizep); ++i) {

                  /* Make child ast and add proto and get type. */
                  auto child_ast = std::make_shared<ast>();
                  child_ast->p = current_proto->p->p[i];
                  child_ast->transpiler_config = config;

                  current_proto->protos.emplace_back(child_ast);
                  ast_funcs::proto::set_closure_info(current_proto, i);

                  /* Add child to get analyzed. */
                  protos_ast.emplace_back(child_ast);

                  child_ast->ast_id = ++ast_id;
            }

/* Set dism of child proto. */
#if display_analysis
            std::printf("[AST] Initing child protos.\n");
#endif
            for (auto &child : current_proto->protos) {

#if display_analysis
                  std::printf("[AST] Initing child proto [%" PRIuPTR "].\n", reinterpret_cast<std::uintptr_t>(child.get()));
#endif

                  if (child->dissassembly.empty()) {

#if display_analysis
                        std::printf("[AST] Setting child dissasembly.\n");
#endif

                        pc = 0u;

                        /* Set current proto dissasembly. */
                        for (auto i = 0u; i < unsigned(child->p->sizecode);) {
                              auto dism = std::make_shared<LuaU_dissassembler::dissassembly>();
                              LuaU_dissassembler::dissassemble(pc, child->p, dism);
                              child->dissassembly.insert(std::make_pair(pc, dism));
                              pc += dism->len;
                              i += dism->len;
                              retn->total += dism->total;
                        }
                  }

                  /* Set child proto blocks. */
                  if (child->main_block == nullptr || child->main_block->nodes.empty()) {
#if display_analysis
                        std::printf("[AST] Initing child proto main blocks.\n");
#endif
                        blocks::set_blocks(child);
                  }
            }

/* Init ast */
#if display_analysis
            std::printf("[AST] Initing ast.\n");
#endif
            ast_init::init_ast(current_proto);

/* Post ast */
#if display_analysis
            std::printf("[AST] Post processing ast.\n");
#endif
            ast_init::post_ast(current_proto);

/* Set end pc and display tree. */
#if display_analysis
            std::printf("[AST] Setting end pc.\n");
#endif
            current_proto->pc_end = current_proto->main_block->visit_all().back()->address;

#if display_analysis
            std::printf("[AST] Finished with current ast.\n");
#endif

#if display_data
            std::cout << current_proto->proto_information() << std::endl;
            std::cout << "Tree:\n"
                      << current_proto->tree_str() << std::endl;
#endif

            /* Remove current. */
            protos_ast.erase(std::remove(protos_ast.begin(), protos_ast.end(), current_proto), protos_ast.end());

            debug_close("AST");

      } while (protos_ast.size());

      /* Clear cache for new asts. */
      global_cache::clear();

      return retn;
}