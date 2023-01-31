#pragma once
#include "../../dissassembler/Dissassembler.hpp"
#include "../debug.hpp"
#include "../lexer/lexer_dec.hpp"
#include "../transpiler/transpiler_data.hpp"
#include "ast_config.hpp"
#include "ast_dec.hpp"
#include "ast_functions/ast_functions_macros.hpp"
#include <algorithm>
#include <inttypes.h>
#include <iostream>
#include <map>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>



/*

	Standard ast. Nothing too special.

*/

namespace ast_dec {

      /* Types of closures. */
      enum class closure_type : std::uint8_t {
            none,
            main,      /* Main proto (Nothing). */
            local,     /* local function test () */
            global,    /* function test () */
            newclosure /* (function()  end)*/
      };

      /* Expression type. {desc, intended usage/set for mostly} */
      enum class expr_type : std::uint8_t {
            lex, /* Specific type refer to lexer. [PLACEHOLDER] */

            arith,  /* r1 += r1 + r1 [AST] */
            arithK, /* r1 += r1 + 1 [AST] */

            /* Generic locvar */
            locvar,         /* Dest is locvar. [ALL] */
            locvar_upvalue, /* Dest locvar turns into a upvalue. [ALL] */
            locvar_multret, /* Dest is locvar(Multret). [ALL] */

            statement_begin, /* Begin statement line. [AST] */
            statement_end,   /* End statement line. [AST] */

            table, /* Table instruction. [AST] */

            /* Multret concat **Applicable too move, set(table/global/upv)** */
            call_mulret_start,  /* Call multret concat start [TRANSPILER] */
            call_mulret_member, /* Call multret concat member [TRANSPILER] */
            call_mulret_end,    /* Call multret concat end [TRANSPILER] */

            for_iv_start, /* for i,v in pairs ({ 1 }) do [ALL] */
            for_iv_end,   /* For(i,v) end(Jumpback) [AST] */
            for_start,    /* for ?? in ?? do [ALL] */
            for_end,      /* For end(Jumpback) [AST] */
            for_n_start,  /* for ?? in ?? do (numeral) [ALL] */
            for_n_end,    /* For(n) end(Jumpback) [AST] */
            for_prep,     /* For preperation instruction [AST] */

            register_scope_dest, /* Register set in scope but used outside of it before getting set. [ALL] */

            repeat_,   /* repeat [ALL] */
            while_,    /* while () follows condition(not jumpback). [ALL] */
            while_end, /* while () end(Jumpback) [AST] */
            until_,    /* until () follows condition(typically jumpback). [ALL] */
            break_,    /* break [ALL] */
            scope_end, /* scope end (generic) [ALL] */

            call_routine_start, /* Call routine start. [AST] */
            call_routine_end,   /* Call routine end. [AST] */

            concat_routine_start, /* Concat routine start. [AST] */
            concat_routine_end,   /* Concat routine end. [AST] */

            conditional_expression_predicted, /* Indication that this may be the end of a conditional_expression. (Does not garunteed one) [AST] */
            conditional_expression_start,     /* Conditional expression routine start. [AST] */
            conditional_expression_end,       /* Conditional expression routine end. (Compare flag gets sets in dest(Hard coded else for LOP_LOADB/LOP_GETIMPORT(no src reg))) [AST] */
            conditional_expression_end_emit,  /* Same as conditonal_expression_end but it emits data too the end of compare flag [TRANSPILER]*/

            if_,                     /* if () [ALL] */
            elseif_,                 /* elseif () [ALL] */
            else_,                   /* else [ALL] */
            condition_nonmutable,    /* Condition that cannot be converted into while/if/elseif etc. [AST] */
            condition_concat_start,  /* Concat a condition(universal) (start). [AST] */
            condition_concat_member, /* Concat a condition(universal) (member). [AST] */
            condition_concat_end,    /* Concat a condition(universal) (end will get written too compare flag). [AST] */
            condition_true,          /* Sets compare flag too true garunteing that while true expressions get set as true (expr type). [ALL] */
            condition_flag,          /* Writes result to flag. [TRANSPILER] */
            condition_break,         /* Conditon leads too break. (Safer than break, end exprs. Garunteeds "break; \n end" emit) [TRANSPILER] */
            condition_emit_next,     /* Emits compare data too dest register in next instruction. [TRANSPILER] */
            /* Different from concat condition doesn't garunteed actual concatation just a hint. */
            condition_logical_start, /* Start of a logical operation. [AST] */
            condition_logical,       /* Apart of a logical operation. [AST] */
            condition_logical_end,   /* End of a logical operation. [AST] */
            condition_append_source, /* Appends source too flag compare flag. [TRANSPILER] */

            /* Will get emmited to condition flag post compare. */
            condition_and,       /* if/elseif/nested(and) appends to if_statements (Can be applied to until or while) [ALL] */
            condition_or,        /* if/elseif/nested(or)  appends to if_statements (Can be applied to until or while) [ALL] */
            condition_close,     /* Conditon flag:  %s ) [ALL] */
            condition_open,      /*  Conditon flag: ( %s   [ALL](PRE) */
            condition_open_post, /*  Conditon flag: ( %s   [ALL](POST) */

            /* These only apply to routines outside of other routines like, call, concat, table, etc. (Will account for call parameters) [AST] */
            condition_routine, /* Means instruction is apart of a conditional routine (will only account for valid data not known args or vars, some will get in like if instruction before branch is locvar) **Does not mean it can't be an arguement!!** [AST] */
            condition_routine_start,
            condition_routine_end,

            /* Jump */
            jump_elseif, /* Jump leads too elseif [AST] */
            jump_else,   /* Jump leads too else [AST] */

            table_start,        /* Table { [ALL] */
            table_element,      /* Element in table. (Not usable for setlist cause of concatation) [ALL] */
            table_end,          /* Table } (Will get ignored and use SETLIST instruction integral operand amt if it hits SETLIST.) [ALL] */
            table_index,        /* Extra expr used for certain things (Will be appended when everything is done). [ALL] */

            closure_local,      /* local function ?? () **Can be mutated by lv set if it is a lv will be local else newclosure** [ALL] */
            closure_global,     /* function ?? () **Can be mutated by lv set if it is a lv instead of a global will turn into local** [ALL] */
            closure_newclosure, /* (function()  end) [ALL] */

            bad_instruction,  /* Instruction will never get executed no matter watch branch is taken or not. [AST] */
            dead_instruction, /* Instruction gets ignored. *Will run exprs but not instruction in transpiler. [TRANSPILER] */
            conditional       /* Condition flag will get written too dest. (Used for branching opcodes including loadb +jmp **Will clear compare flag if conditional is not loadb) [ALL] */
      };

      enum class element {
            front,
            back
      };

      enum class str_type : std::uint8_t {
            dissassembly,
            all
      };

      struct node {

            std::uintptr_t address = 0u; /* Address. */

            std::vector<std::pair<expr_type, std::size_t /* Count usally used for ends, repeat, etc. */>> expr = {{expr_type::lex, 0u}}; /* Expression types. (Follows order) *All will get emmited(str). */

            /* Convert destination to local? */
            struct dest_loc {
                  bool set_prefix = false;                /* Used in transpiler to set suffix to local variable name. */
                  std::string name = "";                  /* Locvar name (Suffix, actuall variable name if is_upvalue is true, closure names wont get set here)  */
                  std::vector<std::string> multret_names; /* Locvar names for multret follows name. (All variable names will be here including first, name is for ones without multret  (Garunteed)) */
                  std::size_t multret_amount = 0u;        /* Locvar mulret count for regs. */
            } dest_loc;

            /* Extra information for branch. */
            struct branch_extra {
                  bool opposite = false; /* Opposite compare from opcode. */
            } branch_extra;

            /* Extra information for tables. */
            struct table_extra {
                  std::uintptr_t end_table = 0u; /* End table node address(not scopped). */
            } table_extra;

            /* Extra information for loops. */
            struct loop_extra {
                  std::shared_ptr<ast_dec::node> start_node = nullptr;                                 /* Start of loop. */
                  std::shared_ptr<ast_dec::node> end_node = nullptr;                                   /* Used for prologue and epilogue of loop. */
                  std::uint16_t start_reg = 0u;                                                        /* Start register (Format, End of loop) */
                  std::uint16_t end_reg = 0u;                                                          /* End register (Format, End of loop) */
                  std::unordered_map<std::uint16_t /* Reg */, std::string /* Name */> iteration_names; /* Override iteration variable names. */
            } loop_extra;

            /* Extra information for closure. */
            struct closure_extra {
                  std::size_t closure_idx = 0u; /* Index of relating closure too ast->proto. */
                  std::shared_ptr<node> setglobal_node = nullptr;
                  std::pair<std::shared_ptr<node> /* Begin */, std::shared_ptr<node> /* End */> idx_nodes; /* Set index nodes for closure. */
            } closure_extra;

            std::shared_ptr<lexer_dec::lexerme> lex; /* Node lexer data. Has all the detailed information. */

            /* Special */
            std::shared_ptr<node> sub_node = nullptr;             /* When a move instruction is hit this will be the source node(mutable by for loop scopes(preps or jumptoo if no prep)). */
            std::vector<std::shared_ptr<node>> dest_nodes_init;   /* Where dests where intialy initialized(mutable by for loop scopes). (MAY BE UNSORTED) */
            std::vector<std::shared_ptr<node>> source_nodes_init; /* Where sources where intialy initialized(mutable by for loop scopes). (MAY BE UNSORTED)  */
            std::vector<std::shared_ptr<node>> source_nodes;      /* If a instruction has source/reg operands with regs it will get when those regs where last set(Can be init, not arg). (MAY BE UNSORTED) */

            /* Node functions */

            /* Appends expr */
            template <expr_type type>
            void add_expr(const std::size_t count = 1u, const element ele = element::back, const std::size_t pos = 0u /* Optional positon overrides ele. */) {
                    
                  if (!count) {
                        return;
                  }

                  /* Replace only lex with type. */
                  if (this->expr.size() && this->expr.front().first == ast_dec::expr_type::lex /* Used as place holder. */) {
                        this->expr.front().first = type;
                        this->expr.front().second = count;
                  } else {

                        const auto pair = std::make_pair(type, count);

                        if (pos) {
                              this->expr.insert(this->expr.begin() + pos, pair);
                        } else {

                              if (ele == element::back) {
                                    this->expr.emplace_back(pair);
                              } else {
                                    this->expr.insert(this->expr.begin(), pair);
                              }
                        }
                  }

                  return;
            }

            /* Appends with nonconsant type. */
            void add_expr_tt(const expr_type type, const std::size_t count = 1u, const element ele = element::back, const std::size_t pos = 0u /* Optional positon overrides elee. */) {

                  if (!count) {
                        return;
                  }

                  /* Replace only lex with type. */
                  if (this->expr.size() && this->expr.front().first == ast_dec::expr_type::lex /* Used as place holder. */) {
                        this->expr.front().first = type;
                        this->expr.front().second = count;
                  } else {

                        const auto pair = std::make_pair(type, count);

                        if (pos) {
                              this->expr.insert(this->expr.begin() + pos, pair);
                        } else {

                              if (ele == element::back) {
                                    this->expr.emplace_back(pair);
                              } else {
                                    this->expr.insert(this->expr.begin(), pair);
                              }
                        }
                  }

                  return;
            }

            /* Remove expr */
            template <expr_type type>
            void remove_expr() {

                  /* Replace only lex with type. */
                  if (this->expr.size() == 1u && this->expr.front().first == type) {
                        this->expr.front().first = ast_dec::expr_type::lex;
                        this->expr.front().second = 1u;
                  } else {

                        for (auto i = 0u; i < this->expr.size(); i++) {

                              const auto expr = this->expr[i];

                              if (expr.first == type) {
                                    this->expr.erase(this->expr.begin() + i);
                                    break;
                              }
                        }
                  }

                  return;
            }

            /* Remove expr */
            template <expr_type type>
            std::pair<expr_type, std::size_t> get_expr() {

                  for (const auto &expr : this->expr)
                        if (expr.first == type) {
                              return expr;
                        }

#if display_warnings
                  std::printf("[WARNING] Returning dead expr for get_expr.\n");
#endif

                  return std::make_pair(expr_type::lex, 1u);
            }

            /* Adds expression if type isn't a expr. **Used when their should be one garunteed expr of one type.**  */
            template <expr_type type>
            void add_existance(const std::size_t count = 1u, const element ele = element::back) {

                  if (!this->has_expr(type))
                        this->add_expr<type>(count, ele);

                  return;
            }

            /* Replaces next target from 0 with type and count. */
            template <expr_type target, expr_type type>
            void replace_next(const std::size_t count = 1u) {

                  for (auto &expr : this->expr)
                        if (expr.first == target) {
                              expr.first = type;
                              expr.second = count;
                              break;
                        }

                  return;
            }

            /* Removal all target. */
            template <expr_type target>
            void remove_all_expr() {

                  while (this->has_expr(target)) {
                        this->remove_expr<target>();
                  }

                  return;
            }

            /* Expr exists? */
            bool has_expr(expr_type type) {
                  for (const auto &i : this->expr)
                        if (i.first == type)
                              return true;
                  return false;
            }

            /* Counts expr count total. */
            template <expr_type type>
            std::uintptr_t count_expr() {
                  std::uintptr_t count = 0u;
                  for (const auto &i : this->expr)
                        if (i.first == type)
                              count += i.second;
                  return count;
            }

            /* Counts expr count total with non static type. */
            std::uintptr_t count_expr(const expr_type type) {
                  std::uintptr_t count = 0u;
                  for (const auto &i : this->expr)
                        if (i.first == type)
                              count += i.second;
                  return count;
            }

            /* Counts expr member total. */
            template <expr_type type>
            std::uintptr_t count_expr_member() {
                  std::uintptr_t count = 0u;
                  for (const auto &i : this->expr)
                        if (i.first == type)
                              count++;
                  return count;
            }

            /* Collapses expr by count. */
            void collapse_expr() {

                  for (auto i = 0u; i < this->expr.size(); ++i) {

                        auto expr = this->expr[i];

                        while (expr.second > 1u) {

                              this->add_expr_tt(expr.first, 1u, ast_dec::element::back, i);
                              --expr.second;
                        }

                  }

                  return;
            }

            /* Gets final sub node. */
            std::shared_ptr<node> get_sub_node() {

                  auto sub = this->sub_node;

                  while (sub != nullptr)
                        if (sub->sub_node != nullptr && sub->sub_node != sub) {
                              sub = sub->sub_node;
                        } else {
                              break;
                        }

                  return sub;
            }

            

#if node_debug

            /* Turns expr pair into a string. */
            std::string expr_str(const std::pair<expr_type, std::size_t> &p) {

                  std::string retn = "";

                  switch (p.first) {

                        case expr_type::lex: {
                              retn += "lex";
                              break;
                        }

                        case expr_type::arith: {
                              retn += "arith";
                              break;
                        }
                        case expr_type::arithK: {
                              retn += "arithK";
                              break;
                        }

                        case expr_type::locvar: {
                              retn += "locvar";
                              break;
                        }
                        case expr_type::locvar_upvalue: {
                              retn += "locvar_upvalue";
                              break;
                        }
                        case expr_type::locvar_multret: {
                              retn += "locvar_multret";
                              break;
                        }

                        case expr_type::statement_begin: {
                              retn += "statement_begin";
                              break;
                        }
                        case expr_type::statement_end: {
                              retn += "statement_end";
                              break;
                        }

                        case expr_type::table: {
                              retn += "table";
                              break;
                        }

                        case expr_type::call_mulret_start: {
                              retn += "call_mulret_start";
                              break;
                        }
                        case expr_type::call_mulret_member: {
                              retn += "call_mulret_member";
                              break;
                        }
                        case expr_type::call_mulret_end: {
                              retn += "call_mulret_end";
                              break;
                        }

                        case expr_type::for_iv_start: {
                              retn += "for_iv_start";
                              break;
                        }
                        case expr_type::for_iv_end: {
                              retn += "for_iv_end";
                              break;
                        }
                        case expr_type::for_start: {
                              retn += "for_start";
                              break;
                        }
                        case expr_type::for_end: {
                              retn += "for_end";
                              break;
                        }
                        case expr_type::for_n_start: {
                              retn += "for_n_start";
                              break;
                        }
                        case expr_type::for_n_end: {
                              retn += "for_n_end";
                              break;
                        }
                        case expr_type::for_prep: {
                              retn += "for_prep";
                              break;
                        }

                        case expr_type::register_scope_dest: {
                              retn += "register_scope_dest";
                              break;
                        }

                        case expr_type::repeat_: {
                              retn += "repeat";
                              break;
                        }
                        case expr_type::while_: {
                              retn += "while";
                              break;
                        }
                        case expr_type::while_end: {
                              retn += "while_end";
                              break;
                        }
                        case expr_type::until_: {
                              retn += "until";
                              break;
                        }
                        case expr_type::break_: {
                              retn += "break";
                              break;
                        }
                        case expr_type::scope_end: {
                              retn += "scope_end";
                              break;
                        }

                        case expr_type::call_routine_start: {
                              retn += "call_routine_start";
                              break;
                        }
                        case expr_type::call_routine_end: {
                              retn += "call_routine_end";
                              break;
                        }

                        case expr_type::concat_routine_start: {
                              retn += "concat_routine_start";
                              break;
                        }
                        case expr_type::concat_routine_end: {
                              retn += "concat_routine_end";
                              break;
                        }

                        case expr_type::conditional_expression_predicted: {
                              retn += "conditional_expression_predicted";
                              break;
                        }
                        case expr_type::conditional_expression_start: {
                              retn += "conditional_expression_start";
                              break;
                        }
                        case expr_type::conditional_expression_end: {
                              retn += "conditional_expression_end";
                              break;
                        }
                        case expr_type::conditional_expression_end_emit: {
                              retn += "conditional_expression_end_emit";
                              break;
                        }

                        case expr_type::if_: {
                              retn += "if";
                              break;
                        }
                        case expr_type::elseif_: {
                              retn += "elseif";
                              break;
                        }
                        case expr_type::else_: {
                              retn += "else";
                              break;
                        }
                        case expr_type::condition_and: {
                              retn += "and";
                              break;
                        }
                        case expr_type::condition_or: {
                              retn += "or";
                              break;
                        }
                        case expr_type::condition_nonmutable: {
                              retn += "condition_nonmutable";
                              break;
                        }
                        case expr_type::condition_concat_start: {
                              retn += "condition_concat_start";
                              break;
                        }
                        case expr_type::condition_concat_member: {
                              retn += "condition_concat_member";
                              break;
                        }
                        case expr_type::condition_concat_end: {
                              retn += "condition_concat_end";
                              break;
                        }
                        case expr_type::condition_true: {
                              retn += "condition_true";
                              break;
                        }
                        case expr_type::condition_routine: {
                              retn += "condition_routine";
                              break;
                        }
                        case expr_type::condition_routine_start: {
                              retn += "condition_routine_start";
                              break;
                        }
                        case expr_type::condition_routine_end: {
                              retn += "condition_routine_end";
                              break;
                        }
                        case expr_type::condition_flag: {
                              retn += "condition_flag";
                              break;
                        }
                        case expr_type::condition_break: {
                              retn += "condition_break";
                              break;
                        }
                        case expr_type::condition_emit_next: {
                              retn += "condition_emit_next";
                              break;
                        }
                        case expr_type::condition_logical_start: {
                              retn += "condition_logical_start";
                              break;
                        }
                        case expr_type::condition_logical: {
                              retn += "condition_logical";
                              break;
                        }
                        case expr_type::condition_logical_end: {
                              retn += "condition_logical_end";
                              break;
                        }
                        case expr_type::condition_append_source: {
                              retn += "condition_append_source";
                              break;
                        }

                        case expr_type::condition_close: {
                              retn += "condition_close";
                              break;
                        }
                        case expr_type::condition_open: {
                              retn += "condition_open";
                              break;
                        }
                        case expr_type::condition_open_post: {
                              retn += "condition_open_post";
                              break;
                        }

                        case expr_type::jump_elseif: {
                              retn += "jump_elseif";
                              break;
                        }
                        case expr_type::jump_else: {
                              retn += "jump_else";
                              break;
                        }

                        case expr_type::table_start: {
                              retn += "table_start";
                              break;
                        }
                        case expr_type::table_element: {
                              retn += "table_element";
                              break;
                        }
                        case expr_type::table_end: {
                              retn += "table_end";
                              break;
                        }
                        case expr_type::table_index: {
                              retn += "table_index";
                              break;
                        }

                        case expr_type::closure_local: {
                              retn += "closure_local";
                              break;
                        }
                        case expr_type::closure_global: {
                              retn += "closure_global";
                              break;
                        }
                        case expr_type::closure_newclosure: {
                              retn += "closure_newclosure";
                              break;
                        }

                        case expr_type::bad_instruction: {
                              retn += "bad_instruction";
                              break;
                        }
                        case expr_type::dead_instruction: {
                              retn += "dead_instruction";
                              break;
                        }
                        case expr_type::conditional: {
                              retn += "conditional";
                              break;
                        }

                        default: {
                              throw std::runtime_error("Unkown expr for expr string.");
                        }
                  }

                  retn = '[' + retn + "]: " + std::to_string(p.second);

                  return retn;
            }

#endif

            /* Debug */

#if debug_functions

            /* Prints dissassembly */
            void debug_print_dissassembly(const char *const state = " ") {
                  std::printf("[Node-Debug(%s)] %" PRIuPTR " %s\n", state, this->lex->dissassembly->addr, this->lex->dissassembly->data.c_str());
                  return;
            }

            /* Prints everything */
            void debug_print_all(const char *const state = " ") {
                  std::printf("[Node-Debug(%s)] %" PRIuPTR " %s", state, this->lex->dissassembly->addr, this->lex->dissassembly->data.c_str());
                  for (const auto &i : this->expr)
                        std::printf("%s", this->expr_str(i).c_str());
                  std::printf("\n");
                  return;
            }

#endif

            template <str_type type = str_type::dissassembly>
            std::string str() {

                  switch (type) {

                        case str_type::all: {

                              auto retn = std::to_string(this->lex->dissassembly->addr) + " " + this->lex->dissassembly->data;

                              for (const auto &i : this->expr)
                                    retn += this->expr_str(i);

                              return retn;
                        }

                        case str_type::dissassembly: {
                              return std::string(std::to_string(this->lex->dissassembly->addr) + " " + this->lex->dissassembly->data);
                        }

                        default: {
                              return "";
                        }

                  }

            }

      };

      struct block {

            std::uintptr_t node_start = 0u; /* PC start. */
            std::uintptr_t node_end = 0u;   /* PC final instruction. */

            std::vector<std::shared_ptr<node>> nodes;     /* Nodes in block. (Body) */
            std::vector<std::shared_ptr<block>> branches; /* 2 elements; first is branch taken second is not, 1 there is only a jump (calls\for don't count, jumpbacks will refer to other nodes that have been skipped else wont have anything(may get fragmented), may be extras if dead instruction is next), 0 no jumps.  */

            /* All visits gets sorted automatically by address. */

            /* Visits all blocks in ast.  */
            std::vector<std::shared_ptr<node>> visit_all() {

                  /* Return cached */
                  if (!cached.all_nodes.empty()) {
                        return cached.all_nodes;
                  }

                  std::vector<std::shared_ptr<node>> retn;
                  std::vector<block *> scopes = {this};

                  do {

                        auto current_block = scopes.front();

                        retn.insert(retn.end(), current_block->nodes.begin(), current_block->nodes.end());

                        /* Add nested blocks. */
                        for (const auto &i : current_block->branches)
                              scopes.emplace_back(i.get());

                        /* Remove current. */
                        scopes.erase(std::remove(scopes.begin(), scopes.end(), current_block), scopes.end());

                        this->remove_dupes(scopes); /* Remove duplicates. */

                  } while (scopes.size());

                  if (retn.empty()) {
                        throw std::runtime_error("Returning no data for visit_all.");
                  }

                  this->remove_dupes(retn);
                  this->sort_addr(retn);

                  /* Set cache */
                  cached.all_nodes = retn;

                  return retn;
            }

            /* Visits first/all opcode value block. */
            template <LuauOpcode op>
            std::variant<std::vector<std::shared_ptr<node>>, std::shared_ptr<node>> visit_inst(const bool all /* All nodes with instruction. */) {

                  std::vector<std::shared_ptr<node>> retn;

                  /* Iterate through block nodes and find given instruction. */
                  const auto all_nodes = this->visit_all();
                  for (const auto &i : all_nodes) {

                        if (i->lex->dissassembly->op == op) {

                              if (all) { /* Has all so emblace node. */
                                    retn.emplace_back(i);
                              } else { /* Not all so return node. */
                                    return i;
                              }
                        }
                  }

                  return retn;
            }

            /* Visits first/all opcode value in scope. */
            template <LuauOpcode op>
            std::variant<std::vector<std::shared_ptr<node>>, std::shared_ptr<node>> visit_inst_scope(const std::uintptr_t addr, const bool all /* All nodes with instruction. */) {

                  std::vector<std::shared_ptr<node>> retn;

                  /* Iterate through block nodes and find given instruction. */
                  const auto all_nodes = this->visit_rest_scope_addr(addr);
                  for (const auto &i : all_nodes) {

                        if (i->lex->dissassembly->op == op) {

                              if (all) { /* Has all so emblace node. */
                                    retn.emplace_back(i);
                              } else { /* Not all so return node. */
                                    return i;
                              }
                        }
                  }

                  return retn;
            }

            /* Visits first/all types in a block. */
            template <lexer_dec::inst_type type>
            std::variant<std::vector<std::shared_ptr<node>>, std::shared_ptr<node>> visit_type(const bool all /* All nodes with instruction. */) {

                  std::vector<std::shared_ptr<node>> retn;

                  /* Iterate through all nodes and find given type. */
                  const auto all_nodes = this->visit_all();
                  for (const auto &i : all_nodes) {

                        if (i->lex->type == type) {

                              if (all) { /* Passed all so emplace node. */
                                    retn.emplace_back(i);
                              } else { /* Not all so return node. */
                                    return i;
                              }
                        }
                  }

#if display_warnings
                  if (retn.empty()) {
                        std::printf("[WARNING] Visit_type returned empty for all.\n");
                  }
#endif

                  return retn;
            }

            /* Visits next/all types in a block. (Ignores current) */
            template <lexer_dec::inst_type type>
            std::variant<std::vector<std::shared_ptr<node>>, std::shared_ptr<node>> visit_type_next_addr(const bool all /* All nodes with instruction. */, const std::uintptr_t addr) {

                  std::vector<std::shared_ptr<node>> retn;

                  /* Iterate through all nodes and find given type. */
                  const auto all_nodes = this->visit_rest(addr);
                  for (const auto &i : all_nodes) {

                        if (i->lex->type == type) {

                              if (all) { /* Passed all so emplace node. */
                                    retn.emplace_back(i);
                              } else { /* Not all so return node. */
                                    return i;
                              }
                        }
                  }

#if display_warnings
                  if (retn.empty()) {
                        std::printf("[WARNING] Visit_type_next_addr returned empty for all.\n");
                  }
#endif

                  return retn;
            }

            /* Visits previous type from an address. (Includes current) */
            template <lexer_dec::inst_type type>
            std::shared_ptr<node> visit_prev_type_current(const std::uintptr_t target) {

                  std::shared_ptr<node> retn = nullptr;

                  /* Iterate through all nodes and find given type. */
                  const auto all_nodes = this->visit_all();
                  for (const auto &i : all_nodes) {

                        if (i->lex->type == type && i->address <= target) {
                              retn = i;
                        }
                  }

                  return retn;
            }

            /* Visits next/all opcode from addr. (Ignores current.) */
            template <LuauOpcode op>
            std::variant<std::vector<std::shared_ptr<node>>, std::shared_ptr<node>> visit_next_inst(const std::uintptr_t addr, const bool all /* All nodes with instruction. */) {

                  std::vector<std::shared_ptr<node>> retn;

                  /* Iterate through all nodes and find given instruction. */
                  const auto all_nodes = this->visit_all();
                  for (const auto &i : all_nodes) {

                        if (i->address > addr && i->lex->dissassembly->op == op) {

                              if (all) { /* Has all so emblace node. */
                                    retn.emplace_back(i);
                              } else { /* Not all so return node. */
                                    return i;
                              }
                        }
                  }

                  /* Nothing. */
                  if (retn.empty()) {
                        throw std::runtime_error("Returning no data for visit_next_inst.");
                  }

                  return retn;
            }

            /* See if next opcode from addr exists. (Ignores current) */
            template <LuauOpcode op>
            bool has_next_inst(const std::uintptr_t addr) {

                  /* Iterate through block nodes and find given instruction. */
                  const auto all_nodes = this->visit_all();
                  for (const auto &i : all_nodes) {

                        if (i->address > addr && i->lex->dissassembly->op == op) {
                              return true;
                        }
                  }

                  return false;
            }

            /* Visits next. */
            std::shared_ptr<node> visit_next(const std::shared_ptr<node> node) {

                  return this->visit_addr(node->address + node->lex->dissassembly->len);
            }

            /* Visit node with address. */
            std::shared_ptr<node> visit_addr(const std::uintptr_t addr) {

                  /* Iterate through block nodes and find given instruction. */
                  const auto all_nodes = this->visit_all();
                  for (const auto &i : all_nodes) {

                        if (i->address == addr)
                              return i;
                  }

#if display_warnings
                  std::printf("[WARNING] Returning no data for visit_addr.\n");
#endif

                  return nullptr;
            }

            /* Visit node with expression. */
            template <expr_type type>
            std::variant<std::vector<std::shared_ptr<node>>, std::shared_ptr<node>> visit_expr(const bool all) {

                  std::vector<std::shared_ptr<node>> retn;

                  /* Iterate through block nodes and find given instruction. */
                  const auto all_nodes = this->visit_all();
                  for (const auto &i : all_nodes) {

                        if (i->has_expr(type)) {

                              if (all) {
                                    retn.emplace_back(i);
                              } else {
                                    return i;
                              }
                        }
                  }

                  return retn;
            }

            /* Visit next node with expression. (Ignores current address) */
            template <expr_type type>
            std::variant<std::vector<std::shared_ptr<node>>, std::shared_ptr<node>> visit_next_expr(const std::uintptr_t address, const bool all) {

                  std::vector<std::shared_ptr<node>> retn;

                  /* Iterate through block nodes and find given instruction. */
                  const auto all_nodes = this->visit_all();
                  for (const auto &i : all_nodes) {

                        if (i->address > address && i->has_expr(type)) {

                              if (all) {
                                    retn.emplace_back(i);
                              } else {
                                    return i;
                              }
                        }
                  }

                  return retn;
            }

            /* Visit next node with expression. (Includes current address) */
            template <expr_type type>
            std::variant<std::vector<std::shared_ptr<node>>, std::shared_ptr<node>> visit_next_expr_current(const std::uintptr_t address, const bool all) {

                  std::vector<std::shared_ptr<node>> retn;

                  /* Iterate through block nodes and find given instruction. */
                  const auto all_nodes = this->visit_all();
                  for (const auto &i : all_nodes) {

                        if (i->address >= address && i->has_expr(type)) {

                              if (all) {
                                    retn.emplace_back(i);
                              } else {
                                    return i;
                              }
                        }
                  }

                  return retn;
            }

            /* Visit next node with expression. (Ignores current address) */
            template <expr_type type>
            std::variant<std::vector<std::shared_ptr<node>>, std::shared_ptr<node>> visit_next_expr_scope(const std::uintptr_t address, const bool all) {

                  std::vector<std::shared_ptr<node>> retn;

                  /* Iterate through block nodes and find given instruction. */
                  const auto all_nodes = this->visit_rest_scope_addr(address);
                  for (const auto &i : all_nodes) {

                        if (i->has_expr(type)) {

                              if (all) {
                                    retn.emplace_back(i);
                              } else {
                                    return i;
                              }
                        }
                  }

                  return retn;
            }

            /* Visit node with previous address. */
            std::shared_ptr<node> visit_previous_addr(const std::uintptr_t addr) {

                  /* Iterate through block nodes and find given instruction. */
                  const auto all_nodes = this->visit_all();
                  for (const auto &i : all_nodes) {

                        if ((i->address + i->lex->dissassembly->len) == addr)
                              return i;
                  }

#if display_warnings
                  std::printf("[WARNING] Nullptr returning for previous addr.");
#endif

                  return nullptr;
            }

            /* Visits next/all inst type. */
            template <lexer_dec::inst_type inst>
            std::variant<std::vector<std::shared_ptr<node>>, std::shared_ptr<node>> visit_next_type(const bool all /* All nodes with instruction. */) {

                  std::vector<std::shared_ptr<node>> retn;

                  /* Iterate through block nodes and find given instruction. */
                  const auto all_nodes = this->visit_all();
                  for (const auto &i : all_nodes) {

                        if (i->lex->type == inst) {

                              if (all) { /* Has all so emblace node. */
                                    retn.emplace_back(i);
                              } else { /* Not all so return node. */
                                    return i;
                              }
                        }
                  }

                  return retn;
            }

            /* Visits next/all inst type from address. Doesn't include current. */
            template <lexer_dec::inst_type inst>
            std::variant<std::vector<std::shared_ptr<node>>, std::shared_ptr<node>> visit_next_type_addr(const std::uintptr_t addr, const bool all /* All nodes with instruction. */) {

                  std::vector<std::shared_ptr<node>> retn;

                  /* Iterate through block nodes and find given instruction. */
                  const auto all_nodes = this->visit_all();
                  for (const auto &i : all_nodes) {

                        if (i->address > addr && i->lex->type == inst) {

                              if (all) { /* Has all so emblace node. */
                                    retn.emplace_back(i);
                              } else { /* Not all so return node. */
                                    return i;
                              }
                        }
                  }

                  if (all) {
                        return retn;
                  } else {
                        return nullptr;
                  }
            }

            /* Visits all inst type in range. (Includes being, end) */
            template <lexer_dec::inst_type inst>
            std::vector<std::shared_ptr<node>> visit_range_type(const std::uintptr_t begin, const std::uintptr_t end) {

                  std::vector<std::shared_ptr<node>> retn;

                  /* Iterate through block nodes and find given instruction. */
                  const auto all_nodes = this->visit_all();
                  for (const auto &i : all_nodes) {

                        if (i->address >= begin && i->address <= end && i->lex->type == inst) {
                              retn.emplace_back(i);
                        }
                  }

                  return retn;
            }

            /* Visits next inst type in range. (Includes being, end) */
            template <lexer_dec::inst_type inst>
            std::shared_ptr<node> visit_range_type_next(const std::uintptr_t begin, const std::uintptr_t end) {

                  /* Iterate through block nodes and find given instruction. */
                  const auto all_nodes = this->visit_all();
                  for (const auto &i : all_nodes) {

                        if (i->address >= begin && i->address <= end && i->lex->type == inst) {
                              return i;
                        }
                  }

#if display_warnings
                  std::printf("[WARNING] Nothing will be returned for visit_range_type_next.\n");
#endif

                  return nullptr;
            }

            /* Visits node with previous node with given register as dest. */
            std::shared_ptr<node> visit_previous_dest_register(const std::uintptr_t on_address, const std::uint16_t target_reg) {

                  std::shared_ptr<ast_dec::node> retn = nullptr;

                  /* Iterate through all nodes and find given dest register thats before on_address. */
                  const auto all_nodes = this->visit_all();
                  for (const auto &i : all_nodes) {

                        /* yeah */
                        if (i->address < on_address && i->lex->operands.size() && i->lex->operands.front() == lexer_dec::operand_types::dest && i->lex->dissassembly->operands.front()->reg == target_reg) {

                              if (retn == nullptr) /* First */
                                    retn = i;
                              else if (retn->address < i->address /* Nearest */)
                                    retn = i;
                        }
                  }

                  /* Node is null. */
                  if (retn == nullptr)
                        throw std::runtime_error("Couldn't find previous dest based on register.");

                  return retn;
            }

            /* Visits rest of nodes for all blocks. (Ignores current) */
            std::vector<std::shared_ptr<node>> visit_rest(const std::uintptr_t on_address) {

                  std::vector<std::shared_ptr<node>> retn;

                  /* Iterate through block nodes and find given instruction. */
                  const auto all_nodes = this->visit_all();
                  for (const auto &i : all_nodes) {

                        if (i->address > on_address)
                              retn.emplace_back(i);
                  }

                  if (retn.empty()) {
                        throw std::runtime_error("Returning no data for visit_addr.");
                  }

                  return retn;
            }

            /* Visits rest of nodes for all blocks. (Includes current) */
            std::vector<std::shared_ptr<node>> visit_rest_curr(const std::uintptr_t on_address) {

                  std::vector<std::shared_ptr<node>> retn;

                  /* Iterate through block nodes and find given instruction. */
                  const auto all_nodes = this->visit_all();
                  for (const auto &i : all_nodes) {

                        if (i->address >= on_address)
                              retn.emplace_back(i);
                  }

                  if (retn.empty()) {
                        throw std::runtime_error("Returning no data for visit_addr.");
                  }

                  return retn;
            }

            /* Visits rest of nodes for all blocks backwards with start being start passed. (Includes current) */
            std::vector<std::shared_ptr<node>> visit_rest_curr_flip(const std::uintptr_t start) {

                  std::vector<std::shared_ptr<node>> vect = {this->visit_addr(start)};

                  auto prev = this->visit_previous_addr(start);
                  while (prev != nullptr) {
                        vect.emplace_back(prev);
                        prev = this->visit_previous_addr(prev->address);
                  }

                  return vect;
            }

            /* Visits relative node to op being target and args being addatives till op hits = dec and args = inc(singular) and its 0.  */
            template <LuauOpcode op>
            std::shared_ptr<node> visit_relative_inst(const std::vector<LuauOpcode> rel) {

                  auto count = 0u;

                  /* Iterate through block nodes and find given instruction. */
                  const auto all_nodes = this->visit_all();
                  for (const auto &i : all_nodes) {

                        /* Inc for relative. */
                        if (std::find(rel.begin(), rel, i->lex->dissassembly->op) != rel.end())
                              ++count;

                        /* If found op dec if count isnt 0. If it is 0 then return node. */
                        if (i->lex->dissassembly->op == op)
                              if (!count)
                                    return i;
                              else
                                    --count;
                  }

                  throw std::runtime_error("Returning no data for visit_relative_inst.");
            }

            /* Visit alls nodes inside scope with passed addr. (Ignores current) */
            std::vector<std::shared_ptr<node>> visit_rest_scope_addr(const std::uintptr_t on_address) {

                  std::vector<std::shared_ptr<node>> nodes;

                  std::intptr_t scope_ = 0;
                  const auto next = this->visit_rest(on_address);
                  for (const auto &i : next) {

                        /* Scope */
                        scope_ += (i->count_expr<ast_dec::expr_type::repeat_>() +
                                   i->count_expr<ast_dec::expr_type::while_>() +
                                   i->count_expr<ast_dec::expr_type::for_start>() +
                                   i->count_expr<ast_dec::expr_type::for_iv_start>() +
                                   i->count_expr<ast_dec::expr_type::for_n_start>() +
                                   i->count_expr<ast_dec::expr_type::if_>());
                        scope_ -= i->count_expr<ast_dec::expr_type::scope_end>();

                        /* Out of scope */
                        if (scope_ < 0) {
                              break;
                        }

                        nodes.emplace_back(i);
                  }

                  return nodes;
            }

            /* Visit alls nodes inside scope with passed addr. (Includes current) */
            std::vector<std::shared_ptr<node>> visit_rest_scope_addr_current(const std::uintptr_t on_address) {

                  std::vector<std::shared_ptr<node>> nodes;

                  std::intptr_t scope_ = 0;
                  const auto next = this->visit_rest_curr(on_address);
                  for (const auto &i : next) {

                        /* Scope */
                        scope_ += (i->count_expr<ast_dec::expr_type::repeat_>() +
                                   i->count_expr<ast_dec::expr_type::while_>() +
                                   i->count_expr<ast_dec::expr_type::for_start>() +
                                   i->count_expr<ast_dec::expr_type::for_iv_start>() +
                                   i->count_expr<ast_dec::expr_type::for_n_start>() +
                                   i->count_expr<ast_dec::expr_type::if_>());
                        scope_ -= i->count_expr<ast_dec::expr_type::scope_end>();

                        /* Out of scope */
                        if (scope_ < 0) {
                              break;
                        }

                        nodes.emplace_back(i);
                  }

                  return nodes;
            }

            /* Visits next relative node to op being target and args being addatives till op hits = dec and args = inc(singular) and its 0.  (Ignores current) */
            template <LuauOpcode op>
            std::shared_ptr<node> visit_relative_next_inst(const std::uintptr_t on_address, const std::vector<LuauOpcode> rel) {

                  auto count = 0u;

                  /* Iterate through block nodes and find given instruction. */
                  const auto all_nodes = this->visit_all();
                  for (const auto &i : all_nodes) {

                        /* If current node address isnt bigger repeat till it is.*/
                        if (i->address <= on_address)
                              continue;

                        /* Inc for relative. */
                        if (std::find(rel.begin(), rel.end(), i->lex->dissassembly->op) != rel.end())
                              ++count;

                        /* If found op dec if count isnt 0. If it is 0 then return node. */
                        if (i->lex->dissassembly->op == op)
                              if (!count)
                                    return i;
                              else
                                    --count;
                  }

                  throw std::runtime_error("Returning no data for visit_relative_inst.");
            }

            /* Visits next relative node to expr being target and args being addatives till exprs(rel arg) hits = dec and exper_target(template target) = inc(singular) and its 0.  (Ignores current) */
            template <expr_type target>
            std::shared_ptr<node> visit_relative_next_expr(const std::uintptr_t on_address, const std::vector<expr_type> rel) {

                  auto count = 0;

                  /* Iterate through block nodes and find given instruction. */
                  const auto all_nodes = this->visit_rest(on_address);
                  for (const auto &i : all_nodes) {

                        /* Inc for relative. */
                        for (const auto r : rel)
                              if (i->has_expr(r)) {
                                    count += i->count_expr(r);
                              }

                        /* If found op dec if count isnt 0. If it is 0 then return node. */
                        if (i->has_expr(target)) {

                              count -= i->count_expr<target>();

                              if (count <= 0) {
                                    return i;
                              }
                        }
                  }

                  throw std::runtime_error("Returning no data for visit_relative_next_expr_scope_current.");
            }

            /* Visits next relative node to expr being target and args being addatives till exprs(rel arg) hits = dec and exper_target(template target) = inc(singular) and its 0.  (Includes current) */
            template <expr_type target>
            std::shared_ptr<node> visit_relative_next_expr_scope_current(const std::uintptr_t on_address, const std::vector<expr_type> rel) {

                  std::intptr_t count = 0;

                  /* Iterate through block nodes and find given instruction. */
                  const auto all_nodes = this->visit_rest_curr(on_address);
                  for (const auto &i : all_nodes) {

                        /* Inc for relative. */
                        for (const auto r : rel)
                              if (i->has_expr(r)) {
                                    count += i->count_expr(r);
                              }

                        /* If found op dec if count isnt 0. If it is 0 then return node. */
                        if (i->has_expr(target)) {

                              count -= i->count_expr<target>();

                              if (count <= 0) {
                                    return i;
                              }
                        }
                  }

                  throw std::runtime_error("Returning no data for visit_relative_next_expr.");
            }

            /* Visits next relative node in scope to expr being target and args being addatives till exprs(rel arg) hits = dec and exper_target(template target) = inc(singular) and its 0.  (Ignores current) */
            template <expr_type target>
            std::shared_ptr<node> visit_relative_next_expr_scope(const std::uintptr_t on_address, const std::vector<expr_type> rel) {

                  auto count = 0u;

                  /* Iterate through block nodes and find given instruction. */
                  const auto all_nodes = this->visit_rest_scope_addr(on_address);
                  for (const auto &i : all_nodes) {

                        /* Inc for relative. */
                        for (const auto r : rel)
                              if (i->has_expr(r)) {
                                    ++count;
                                    break;
                              }

                        /* If found op dec if count isnt 0. If it is 0 then return node. */
                        if (i->has_expr(target)) {

                              if (!count) {
                                    return i;
                              } else {
                                    --count;
                              }
                        }
                  }

                  throw std::runtime_error("Returning no data for visit_relative_next_expr.");
            }

            /* Visits expr routines (doesn't count for target).  */
            template <expr_type target, expr_type close>
            std::variant<std::vector<std::pair<std::shared_ptr<node> /* Begin */, std::shared_ptr<node> /* End */>>, std::pair<std::shared_ptr<node> /* Begin */, std::shared_ptr<node> /* End */>> visit_expr_routine(const bool all /* All nodes with instruction. */) {

                  bool inside = false;
                  std::shared_ptr<node> begin = nullptr;
                  std::vector<std::pair<std::shared_ptr<node> /* Begin */, std::shared_ptr<node> /* End */>> retn;

                  const auto all_nodes = this->visit_all();
                  for (const auto &i : all_nodes) {

                        if (i->has_expr(target)) {
                              inside = true;
                              begin = i;
                        }

                        /* If found op dec if count isnt 0. If it is 0 then return node. */
                        if (inside && i->has_expr(close)) {

                              if (all) { /* Has all so emblace node. */
                                    retn.emplace_back(std::make_pair(begin, i));
                              } else { /* Not all so return node. */
                                    return std::make_pair(begin, i);
                              }

                              inside = false;
                        }
                  }

#if display_warnings
                  if (retn.empty()) {
                        std::printf("[WARNING] Returning no data for visit_expr_routine.\n");
                  }
#endif

                  return retn;
            }

            /* See if address is inside rotuine. (Doesnt count close). */
            template <expr_type target, expr_type close>
            bool inside_routine(const std::uintptr_t addr) {

                  std::int32_t count = 0;

                  const auto all_nodes = this->visit_all();
                  for (const auto &i : all_nodes) {

                        /* Inc */
                        if (i->has_expr(target)) {
                              ++count;
                        }

                        /* Dec */
                        if (count != 0 && i->has_expr(close)) {
                              --count;
                        }

                        /* Inside */
                        if (count && i->address == addr) {
                              return true;
                        }
                  }

                  return false;
            }

            /* Visits expr routines in range (includes start and end). */
            template <expr_type target, expr_type close>
            std::vector<std::pair<std::shared_ptr<node> /* Begin */, std::shared_ptr<node> /* End */>> visit_expr_routine_range(const std::uintptr_t start, const std::uintptr_t end) {

                  std::vector<std::pair<std::shared_ptr<node>, std::shared_ptr<node>>> retn;

                  const auto routines = std::get<std::vector<std::pair<std::shared_ptr<node>, std::shared_ptr<node>>>>(this->visit_expr_routine<target, close>(true));
                  for (const auto &i : routines) {

                        if (i.first->address >= start && i.first->address <= end && i.second->address >= start && i.second->address <= end)
                              retn.emplace_back(i);
                  }

                  this->remove_dupes(retn);
                  this->sort_addr(retn);

                  return retn;
            }

            /* Visits expr routines in range that are touching and concats them to one object. (includes start and end).*/
            template <expr_type target, expr_type close>
            std::vector<std::pair<std::shared_ptr<node> /* Begin */, std::shared_ptr<node> /* End*/>> visit_expr_routine_range_touching(const std::uintptr_t start, const std::uintptr_t end) {

                  std::shared_ptr<node> begin = nullptr;
                  std::shared_ptr<node> last = nullptr;
                  std::vector<std::pair<std::shared_ptr<node>, std::shared_ptr<node>>> retn;

                  const auto routines = this->visit_expr_routine_range<target, close>(start, end);
                  for (auto idx = 0u; idx < routines.size(); idx++) {

                        const auto curr = routines[idx];

                        if (begin == nullptr) {
                              begin = curr.first;
                        }
                        last = curr.second;

                        /* End of idx */
                        if (idx == routines.size() - 1u) {
                              retn.emplace_back(std::make_pair(begin, curr.second));
                              begin = nullptr;
                              break;
                        }

                        /* Check next */
                        if (routines[idx + 1u].first->address != (curr.second->address + curr.second->lex->dissassembly->len)) {
                              retn.emplace_back(std::make_pair(begin, curr.second));
                              begin = nullptr;
                        }
                  }

                  if (begin != nullptr) {
                        retn.emplace_back(std::make_pair(begin, last));
                  }

                  this->sort_addr(retn);

                  return retn;
            }

            /* Visits all nodes between addresses (Ignores start, end) */
            std::vector<std::shared_ptr<node>> visit_range(const std::uintptr_t start, const std::uintptr_t end) {

                  std::vector<std::shared_ptr<node>> retn;

                  /* Iterate through block nodes. */
                  const auto all_nodes = this->visit_all();
                  for (const auto &i : all_nodes) {

                        /* Between addresses. */
                        if (i->address > start && i->address < end)
                              retn.emplace_back(i);
                  }

/* Nothing. */
#if display_warnings
                  if (retn.empty()) {
                        std::printf("[WARNING] No will be returned for visit_range.\n");
                  }
#endif

                  return retn;
            }

            /* Visits all nodes between addresses (Includes curren, end) */
            std::vector<std::shared_ptr<node>> visit_range_current(const std::uintptr_t start, const std::uintptr_t end) {

                  std::vector<std::shared_ptr<node>> retn;

                  /* Iterate through block nodes. */
                  const auto all_nodes = this->visit_all();
                  for (const auto &i : all_nodes) {

                        /* Between addresses. */
                        if (i->address >= start && i->address <= end)
                              retn.emplace_back(i);
                  }

                  /* Nothing. */
                  if (retn.empty()) {
                        throw std::runtime_error("Returning no data for visit_range.");
                  }

                  return retn;
            }

            /* Visits rest of nodes that have either element in expr vector passed from start address (includes current). */
            std::vector<std::shared_ptr<node>> visit_either_expr_vector_addr(const std::uintptr_t start, std::vector<ast_dec::expr_type> exprs) {

                  std::vector<std::shared_ptr<node>> retn;

                  const auto all = this->visit_rest_curr(start);
                  for (const auto &i : all)
                        for (const auto expr : exprs)
                              if (i->has_expr(expr)) {
                                    retn.emplace_back(i);
                                    break;
                              }

                  return retn;
            }

            /* Visits nodes that jump too address given with type (Must have memaddr operand) */
            template <lexer_dec::inst_type type>
            std::vector<std::shared_ptr<node>> visit_type_goto(const std::uintptr_t addr) {

                  std::vector<std::shared_ptr<node>> retn;
                  const auto all = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(this->visit_type<type>(true));

                  for (const std::shared_ptr<ast_dec::node> &i : all) {

                        const auto mems = i->lex->operand_expr<lexer_dec::operand_types::memaddr>();
                        for (const auto &m : mems)
                              if (m->jmp_addr == addr) {
                                    retn.emplace_back(i);
                                    break;
                              }
                  }

                  return retn;
            }

            /* Visits nodes that jump too address given (Must have memaddr operand) */
            std::vector<std::shared_ptr<node>> visit_goto(const std::uintptr_t addr) {

                  std::vector<std::shared_ptr<node>> retn;

                  const auto all_nodes = this->visit_all();
                  for (const auto &i : all_nodes) {

                        const auto mems = i->lex->operand_expr<lexer_dec::operand_types::memaddr>();
                        for (const auto &m : mems)
                              if (m->jmp_addr == addr) {
                                    retn.emplace_back(i);
                                    break;
                              }
                  }

                  return retn;
            }

            /* Visits nodes that jump too address. */
            std::vector<std::pair<std::uintptr_t /* Labels address. */, std::vector<std::shared_ptr<node>>> /* Goto addresses */> visit_all_goto() {

                  std::vector<std::pair<std::uintptr_t /* Labels address. */, std::vector<std::shared_ptr<node>>> /* Goto addresses */> retn;

                  const auto all_nodes = this->visit_all();
                  for (const auto &i : all_nodes) {

                        const auto mems = i->lex->operand_expr<lexer_dec::operand_types::memaddr>();
                        for (const auto &m : mems) {

                              const auto key = m->jmp_addr;

                              if (std::find_if(retn.begin(), retn.end(), [&](const std::pair<std::uintptr_t, std::vector<std::shared_ptr<node>>> &pair) { return pair.first == key; }) == retn.end()) {
                                    retn.emplace_back(std::make_pair(key, std::vector<std::shared_ptr<node>>({i})));
                              } else {
                                    std::find_if(retn.begin(), retn.end(), [&](const std::pair<std::uintptr_t, std::vector<std::shared_ptr<node>>> &pair) { return pair.first == key; })->second.emplace_back(i);
                              }
                        }
                  }

                  return retn;
            }

            /* Sees wether if a range start, sources(end) is filled with instructions dest all eventually lead too sources(arg). Start is ignored for first iteration. */
            bool filled(const std::shared_ptr<node> &start /* Start of anlyzation. */, const std::shared_ptr<node> &sources, const bool ignore_routines = false /* Ignores routine exprs until start is hit with -1 = 0. */) {

                  std::intptr_t routine = 0;

                  std::vector<std::uintptr_t> analyzed_addresses;
                  std::vector<std::shared_ptr<ast_dec::node>> vect;

                  /* Only one address check for dest. */
                  if ((start->address + start->lex->dissassembly->len) == sources->address) {
                        return sources->lex->has_operand_expr<lexer_dec::operand_types::dest>();
                  }

                  /* No dest */
                  if (!start->lex->has_operand_expr<lexer_dec::operand_types::dest>()) {
                        return false;
                  }

                  /* Go through each source too see if they fill wil single instruction. */
                  if (!sources->source_nodes.empty()) {

                        vect.insert(vect.begin(), sources->source_nodes.begin(), sources->source_nodes.end());

                        do {

                              auto curr_node = vect.front();

                              if (!ignore_routines) {
                                    
                                    routine_inc_safe(curr_node, routine);
                                    routine_dec_safe(curr_node, routine);

                                    if (routine) {
                                        goto skip;
                                    }

                              }

                              /* No dest */
                              if (!curr_node->lex->has_operand_expr<lexer_dec::operand_types::dest>()) {
                                    return false;
                              }

                              if (curr_node->address >= start->address && !curr_node->source_nodes.empty()) {
                                    vect.insert(vect.end(), curr_node->source_nodes.begin(), curr_node->source_nodes.end());
                              }

                              if (curr_node->address >= start->address && std::find(analyzed_addresses.begin(), analyzed_addresses.end(), curr_node->address) == analyzed_addresses.end()) {
                                    analyzed_addresses.emplace_back(curr_node->address);
                              }

                          skip:

                              /* Remove current. */
                              vect.erase(std::remove(vect.begin(), vect.end(), curr_node), vect.end());

                        } while (vect.size());

                  }

                  /* Check all anlyzed addresses make sure they fill. */
                  bool fill = false;
                  auto on = start;
                  std::sort(analyzed_addresses.begin(), analyzed_addresses.end());
                  for (const auto i : analyzed_addresses) {

                        on = this->visit_addr(on->address + on->lex->dissassembly->len);

                        if (!(fill = (i == on->address))) {
                              break;
                        }

                  }

                  return fill;
            }

          private:

            /* Removes scope dupes. */
            void remove_dupes(std::vector<block *> &scopes) {
                  std::sort(scopes.begin(), scopes.end());
                  scopes.erase(std::unique(scopes.begin(), scopes.end()), scopes.end());
                  return;
            }

            /* Removes node dupes from pair. (removes by address) */
            void remove_dupes(std::vector<std::pair<std::shared_ptr<node> /* Begin */, std::shared_ptr<node> /* End */>> &nodes) {
                  std::sort(nodes.begin(), nodes.end());
                  nodes.erase(std::unique(nodes.begin(), nodes.end()), nodes.end());
                  return;
            }

            /* Removes node dupes. (removes by address) */
            void remove_dupes(std::vector<std::shared_ptr<node>> &nodes) {
                  std::sort(nodes.begin(), nodes.end());
                  nodes.erase(std::unique(nodes.begin(), nodes.end()), nodes.end());
                  return;
            }

            /* Sorts nodes by addr. */
            void sort_addr(std::vector<std::shared_ptr<node>> &nodes) {
                  if (nodes.size())
                        std::sort(nodes.begin(), nodes.end(), [](const std::shared_ptr<node> &a, const std::shared_ptr<node> &b) -> bool { return a->address < b->address; });
                  return;
            }

            /* Sorts nodes by addr. */
            void sort_addr(std::vector<std::pair<std::shared_ptr<node> /* Begin */, std::shared_ptr<node> /* End */>> &nodes) {
                  if (nodes.size())
                        std::sort(nodes.begin(), nodes.end(), [](const std::pair<std::shared_ptr<node>, std::shared_ptr<node>> &a, const std::pair<std::shared_ptr<node>, std::shared_ptr<node>> &b) -> bool { return a.first->address < b.first->address; });
                  return;
            }

            struct cached {

                  std::vector<std::shared_ptr<node>> all_nodes;

            } cached;

      };

      struct ast {

            float total = 0.0f;      /* Used for total only avialable for main ast. */
            std::size_t ast_id = 0u; /* ID */

            Proto *p;                                                                                           /* Proto for ast. */
            std::unordered_map<std::uintptr_t, std::shared_ptr<LuaU_dissassembler::dissassembly>> dissassembly; /* Dissassembly of proto (Useful for some stuff). { PC, dissassembly } ex. dissassembly of pc=15 dissassembly[15]. */
            std::uintptr_t pc_end = 0u;                                                                         /* Pc end */

            closure_type closure_type = closure_type::none; /* Closure type. */
            std::string closure_name = "";                  /* Closure name. (Suffix) */

            std::string closure_decompilation = ""; /* Handled by transpiler not ast used in transpiler for parent protos. */
            bool tanspiled = false;

            std::vector<std::pair<std::int16_t /* Reg */, std::string /* Name */>> arg_regs; /* Register for arguments to be placed in. *-1 means: ... */
            /* No node can refrence the same address all nodes are unique but branches can reference the same jump. */
            std::shared_ptr<block> main_block; /* Main block. */

            std::unordered_map<std::uintptr_t /* Idx */, std::pair<std::string /* Value */, std::int16_t /* Reg (-1 for upv) */>> upvalues;
            std::vector<std::shared_ptr<ast>> protos;                              /* Any children protos, relates to proto->p. */
            std::shared_ptr<transpiler_data::transpiler_config> transpiler_config; /* Linked transpiler config. */

            /* Ast config */
            struct ast_config {

                  /* Expands tables members if members exceed a certain amount. (Disabled for nested tables). -1 for disabled */
                  std::int32_t table_members_expand = -1;

                  /* Will be in use if var/arg/upvalue/etc suffix uses integer instead of char. (Makes incrementor scoped) */
                  bool scoped_global_incrementor = true;

            } ast_config;

            /* Copys current ast config to target. */
            void copy_config(std::shared_ptr<ast> &target) {
                  target->ast_config.scoped_global_incrementor = this->ast_config.scoped_global_incrementor;
                  target->ast_config.table_members_expand = this->ast_config.table_members_expand;
                  return;
            }

            /* Block */

            /* Finds block by start address. */
            std::shared_ptr<block> find_block(const std::uintptr_t addr) {

                  const auto found = block_map.find(addr);
                  if (found != block_map.end()) {
                        return found->second;
                  }

                  return nullptr;
            }

            /* Finds block by containing address. */
            std::shared_ptr<block> find_block_addr(const std::uintptr_t addr) {

                  for (const auto &current_block : block_map) {

                        if (current_block.second->node_start <= addr && current_block.second->node_end >= addr) {
                              return current_block.second;
                        }
                  }

                  return nullptr;
            }

            /* Add block to cache by start address. */
            void add_block(const std::shared_ptr<block> &block) {

                  if (block_map.find(block->node_start) == block_map.end()) {
                        block_map.insert(std::make_pair(block->node_start, block));
                  }

                  return;
            }

/* Turns ast into tree string. */
#if ast_debug

            std::string tree_str() {

                  std::unordered_map<std::uintptr_t /* addr */, std::size_t /* amt */> indent_multiplier;
                  std::string retn = "";
                  std::uintptr_t pc = 0u;
                  std::string indenting = "";
                  std::vector<std::uintptr_t> labels;

                  const auto branch = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(this->main_block->visit_next_type<lexer_dec::inst_type::branch>(true));
                  const auto contional = std::get<std::vector<std::shared_ptr<ast_dec::node>>>(this->main_block->visit_next_type<lexer_dec::inst_type::branch_condition>(true));

                  /* Append jump backs. */
                  for (const auto &node : branch) {
                        labels.emplace_back(node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr);
                  }

                  for (const auto &node : contional) {
                        if (node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp < 0) {
                              labels.emplace_back(node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr);
                        }
                  }

                  /* Append first */
                  indent_multiplier.insert(std::make_pair(pc, 0u));

                  do {

                        const auto block = this->find_block(pc);
                        const auto mult = indent_multiplier[pc];

                        /* Compile indent */
                        for (auto i = 0u; i < mult; ++i)
                              indenting += "";

                        /* Compile nodes str */
                        for (const auto &node : block->nodes) {

                              auto dism = indenting + std::to_string(node->address) + " " + node->lex->dissassembly->data;

                              /* Add label */
                              if (std::find(labels.begin(), labels.end(), node->address) != labels.end()) {
                                    retn += "label_" + std::to_string(node->address) + ":\n";
                              }

                              /* Goto */
                              if (node->lex->type == lexer_dec::inst_type::branch || (node->lex->type == lexer_dec::inst_type::branch_condition && (node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp < 0 || std::find(labels.begin(), labels.end(), node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr) != labels.end()))) {
                                    dism += " goto label_" + std::to_string(node->lex->operand_expr<lexer_dec::operand_types::memaddr>().front()->jmp_addr) + ";";
                              }

                              dism += " ( ";
                              for (const auto &p : node->expr)
                                    dism += node->expr_str(p) + " ";
                              dism += ")\n";

                              retn += dism;
                        }

                        /* Set mults */
                        if (block->branches.size() == 2u) {

                              const auto branch_taken = block->branches.front()->node_start;
                              const auto branch_not_taken = block->branches.back()->node_start;

                              /* Most greater relative too branch_taken. */
                              std::size_t greater = 0u;
                              for (const auto &i : indent_multiplier)
                                    if (i.first > greater && i.first < branch_taken) {
                                          greater = i.first;
                                    }

                              /* Add indent to greater. */
                              if (greater) {

                                    if (indent_multiplier.find(branch_taken) == indent_multiplier.end()) {
                                          indent_multiplier.insert(std::make_pair(branch_taken, indent_multiplier[greater]));
                                          labels.emplace_back(branch_taken);
                                    }
                              }

                              if (indent_multiplier.find(branch_taken) == indent_multiplier.end()) {
                                    indent_multiplier.insert(std::make_pair(branch_taken, mult));
                              }

                              if (indent_multiplier.find(branch_not_taken) == indent_multiplier.end()) {
                                    indent_multiplier.insert(std::make_pair(branch_not_taken, mult + 1u));
                              }
                        }

                        /* Set pc */
                        pc = block->node_end + block->visit_addr(block->node_end)->lex->dissassembly->len;
                        indenting.clear();

                  } while (pc < this->pc_end);

                  return retn;
            }

            std::string proto_information() {

                  std::string retn = "";

                  retn += "Proto:\n";
                  retn += "	* name: " + closure_name + "\n";
                  retn += "	* type: ";

                  switch (this->closure_type) {

                        case ast_dec::closure_type::main: {
                              retn += "main";
                              break;
                        }

                        case ast_dec::closure_type::local: {
                              retn += "local";
                              break;
                        }

                        case ast_dec::closure_type::newclosure: {
                              retn += "newclosure";
                              break;
                        }

                        case ast_dec::closure_type::global: {
                              retn += "global";
                              break;
                        }

                        default: {
                              retn += "none";
                              break;
                        }
                  }
                  retn += "\n";

                  retn += "	* arg count: " + std::to_string(this->arg_regs.size()) + "\n";
                  retn += "	* args: ";

                  for (const auto &i : this->arg_regs)
                        retn += "r" + std::to_string(i.first) + "(" + i.second + ") ";

                  retn += "\n";
                  retn += "	* upvalue count: " + std::to_string(this->upvalues.size()) + "\n";
                  retn += "	* upvalues: ";

                  for (const auto &i : this->upvalues)
                        retn += "r" + std::to_string(i.second.second) + "(" + i.second.first + ") ";

                  return retn;
            }

#endif

          private:
            std::map<std::uintptr_t /* Start */, std::shared_ptr<block>> block_map;
      };

      std::shared_ptr<ast> gen_ast(Proto *proto, const std::shared_ptr<transpiler_data::transpiler_config> &config);

} // namespace ast_dec