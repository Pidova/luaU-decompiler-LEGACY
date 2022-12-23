#include "../luau-master/VM/src/lstate.h"
#include "Dissassembler.hpp"
#include <iostream>
enum set_action : std::uint8_t {
	instruction, /* Sets all instruction info, op, mnenomic, hint. */
	operands /* Sets all operands including details about it. */
};

template <set_action n>
void set_data(std::shared_ptr<LuaU_dissassembler::dissassembly>& buffer, const TValue* k, const op_table::optable op_table) {
	
	switch (n) {

		case set_action::instruction: {

			buffer->op = op_table.op;
		
			switch (op_table.op) {

				case LuauOpcode::LOP_ADD: {
					buffer->mnenomic = "add";
					buffer->hint = "Addition.";
					break;
				}

				case LuauOpcode::LOP_ADDK: {
					buffer->mnenomic = "addk";
					buffer->hint = "Addition with constant.";
					break;
				}

				case LuauOpcode::LOP_AND: {
					buffer->mnenomic = "and";
					buffer->hint = "And.";
					break;
				}

				case LuauOpcode::LOP_ANDK: {
					buffer->mnenomic = "andk";
					buffer->hint = "And with constant.";
					break;
				}

				case LuauOpcode::LOP_BREAK: {
					buffer->mnenomic = "break";
					buffer->hint = "Break.";
					break;
				}

				case LuauOpcode::LOP_CALL: {
					buffer->mnenomic = "call";
					buffer->hint = "Call routine.";
					break;
				}

				case LuauOpcode::LOP_CAPTURE: {
					buffer->mnenomic = "capture";
					buffer->hint = "Capture operation.";
					break;
				}

				case LuauOpcode::LOP_CLOSEUPVALS: {
					buffer->mnenomic = "closeupvals";
					buffer->hint = "Close upvalues.";
					break;
				}

				case LuauOpcode::LOP_CONCAT: {
					buffer->mnenomic = "concat";
					buffer->hint = "Concat strings.";
					break;
				}

				case LuauOpcode::LOP_DEP_FORGLOOP_INEXT: {
					buffer->mnenomic = "forgloop_inext";
					buffer->hint = "For loop with (i)next (Depricated).";
					break;
				}

				case LuauOpcode::LOP_DEP_FORGLOOP_NEXT: {
					buffer->mnenomic = "forgloop_next";
					buffer->hint = "For loop with next (Depricated).";
					break;
				}

				case LuauOpcode::LOP_FORGPREP_INEXT: {
					buffer->mnenomic = "forgloop_inext";
					buffer->hint = "For loop with (i)next (Depricated).";
					break;
				}

				case LuauOpcode::LOP_FORGPREP_NEXT: {
					buffer->mnenomic = "forgloop_next";
					buffer->hint = "For loop with next (Depricated).";
					break;
				}

				case LuauOpcode::LOP_DEP_JUMPIFEQK: {
					buffer->mnenomic = "jumpifeqk";
					buffer->hint = "Jump if equal constant (Depricated).";
					break;
				}

				case LuauOpcode::LOP_DEP_JUMPIFNOTEQK: {
					buffer->mnenomic = "jumpifbnoteqk";
					buffer->hint = "Jump if not equal constant (Depricated).";
					break;
				}

				case LuauOpcode::LOP_DIV: {
					buffer->mnenomic = "div";
					buffer->hint = "Divide.";
					break;
				}

				case LuauOpcode::LOP_DIVK: {
					buffer->mnenomic = "div";
					buffer->hint = "Divide with constant.";
					break;
				}

				case LuauOpcode::LOP_DUPCLOSURE: {
					buffer->mnenomic = "dupclosure";
					buffer->hint = "Dupe closure.";
					break;
				}

				case LuauOpcode::LOP_DUPTABLE: {
					buffer->mnenomic = "duptable";
					buffer->hint = "Dupe table.";
					break;
				}

				case LuauOpcode::LOP_FASTCALL: {
					buffer->mnenomic = "fastcall";
					buffer->hint = "Fastcall.";
					break;
				}

				case LuauOpcode::LOP_FASTCALL1: {
					buffer->mnenomic = "fastcall1";
					buffer->hint = "Fastcall.";
					break;
				}

				case LuauOpcode::LOP_FASTCALL2: {
					buffer->mnenomic = "fastcall2";
					buffer->hint = "Fastcall.";
					break;
				}

				case LuauOpcode::LOP_FASTCALL2K: {
					buffer->mnenomic = "fastcall2k";
					buffer->hint = "Fastcall with constant.";
					break;
				}

				case LuauOpcode::LOP_FORGLOOP: {
					buffer->mnenomic = "forgloop";
					buffer->hint = "For generic loop.";
					break;
				}

				case LuauOpcode::LOP_FORGPREP: {
					buffer->mnenomic = "forgprep";
					buffer->hint = "For generic prepare.";
					break;
				}

				case LuauOpcode::LOP_FORNLOOP: {
					buffer->mnenomic = "fornloop";
					buffer->hint = "For numeral loop.";
					break;
				}

				case LuauOpcode::LOP_FORNPREP: {
					buffer->mnenomic = "fornprep";
					buffer->hint = "For numeral loop.";
					break;
				}

				case LuauOpcode::LOP_GETGLOBAL: {
					buffer->mnenomic = "getglobal";
					buffer->hint = "Get global.";
					break;
				}

				case LuauOpcode::LOP_GETIMPORT: {
					buffer->mnenomic = "getimport";
					buffer->hint = "Get import.";
					break;
				}

				case LuauOpcode::LOP_GETTABLE: {
					buffer->mnenomic = "gettable";
					buffer->hint = "Get table.";
					break;
				}

				case LuauOpcode::LOP_GETTABLEKS: {
					buffer->mnenomic = "gettableks";
					buffer->hint = "Get table constant.";
					break;
				}

				case LuauOpcode::LOP_GETTABLEN: {
					buffer->mnenomic = "gettablen";
					buffer->hint = "Get tabble numeral.";
					break;
				}

				case LuauOpcode::LOP_GETUPVAL: {
					buffer->mnenomic = "getupval";
					buffer->hint = "Get upvalue.";
					break;
				}

				case LuauOpcode::LOP_GETVARARGS: {
					buffer->mnenomic = "getvarargs";
					buffer->hint = "Get variable args.";
					break;
				}

				case LuauOpcode::LOP_JUMP: {
					buffer->mnenomic = "jump";
					buffer->hint = "Jump.";
					break;
				}

				case LuauOpcode::LOP_JUMPBACK: {
					buffer->mnenomic = "jumpback";
					buffer->hint = "Jumpback.";
					break;
				}

				case LuauOpcode::LOP_JUMPIF: {
					buffer->mnenomic = "jumpif";
					buffer->hint = "Jump if.";
					break;
				}

				case LuauOpcode::LOP_JUMPIFEQ: {
					buffer->mnenomic = "jumpifeq";
					buffer->hint = "Jump if equal.";
					break;
				}

				case LuauOpcode::LOP_JUMPIFLE: {
					buffer->mnenomic = "jumpifle";
					buffer->hint = "Jump if less then or equal.";
					break;
				}

				case LuauOpcode::LOP_JUMPIFLT: {
					buffer->mnenomic = "jumpiflt";
					buffer->hint = "Jump if less then.";
					break;
				}

				case LuauOpcode::LOP_JUMPIFNOT: {
					buffer->mnenomic = "jumpifnot";
					buffer->hint = "Jump if not.";
					break;
				}

				case LuauOpcode::LOP_JUMPIFNOTEQ: {
					buffer->mnenomic = "jumpifnoteq";
					buffer->hint = "Jump if not equal.";
					break;
				}

				case LuauOpcode::LOP_JUMPIFNOTLE: {
					buffer->mnenomic = "jumpifnotle";
					buffer->hint = "Jump if not less then or equal.";
					break;
				}

				case LuauOpcode::LOP_JUMPIFNOTLT: {
					buffer->mnenomic = "jumpifnotlt";
					buffer->hint = "Jump if not less then.";
					break;
				}

				case LuauOpcode::LOP_JUMPX: {
					buffer->mnenomic = "jumpx";
					buffer->hint = "Jump long.";
					break;
				}

				case LuauOpcode::LOP_JUMPXEQKB: {
					buffer->mnenomic = "jumpxeqkb";
					buffer->hint = "Jump long if equal (constant, bool).";
					break;
				}

				case LuauOpcode::LOP_JUMPXEQKN: {
					buffer->mnenomic = "jumpxeqkn";
					buffer->hint = "Jump long if equal (constant, numeral).";
					break;
				}

				case LuauOpcode::LOP_JUMPXEQKNIL: {
					buffer->mnenomic = "jumpxeqknil";
					buffer->hint = "Jump long if equal (constant, nil).";
					break;
				}

				case LuauOpcode::LOP_JUMPXEQKS: {
					buffer->mnenomic = "jumpxeqks";
					buffer->hint = "Jump long if equal (constant, string).";
					break;
				}

				case LuauOpcode::LOP_LENGTH: {
					buffer->mnenomic = "lenght";
					buffer->hint = "Lenght.";
					break;
				}

				case LuauOpcode::LOP_LOADB: {
					buffer->mnenomic = "loadb";
					buffer->hint = "Load boolean.";
					break;
				}

				case LuauOpcode::LOP_LOADK: {
					buffer->mnenomic = "loadk";
					buffer->hint = "Load constant.";
					break;
				}

				case LuauOpcode::LOP_LOADKX: {
					buffer->mnenomic = "loadkx";
					buffer->hint = "Load extra constant.";
					break;
				}

				case LuauOpcode::LOP_LOADN: {
					buffer->mnenomic = "loadn";
					buffer->hint = "Load numeral.";
					break;
				}

				case LuauOpcode::LOP_LOADNIL: {
					buffer->mnenomic = "loadnil";
					buffer->hint = "Load nil.";
					break;
				}

				case LuauOpcode::LOP_MINUS: {
					buffer->mnenomic = "minus";
					buffer->hint = "Minus.";
					break;
				}

				case LuauOpcode::LOP_MOD: {
					buffer->mnenomic = "mod";
					buffer->hint = "Mod.";
					break;
				}

				case LuauOpcode::LOP_MODK: {
					buffer->mnenomic = "modk";
					buffer->hint = "Mod constant.";
					break;
				}

				case LuauOpcode::LOP_MOVE: {
					buffer->mnenomic = "move";
					buffer->hint = "Move.";
					break;
				}

				case LuauOpcode::LOP_MUL: {
					buffer->mnenomic = "mul";
					buffer->hint = "Mul.";
					break;
				}

				case LuauOpcode::LOP_MULK: {
					buffer->mnenomic = "mulk";
					buffer->hint = "Mul constant.";
					break;
				}

				case LuauOpcode::LOP_NAMECALL: {
					buffer->mnenomic = "namecall";
					buffer->hint = "Namecall.";
					break;
				}

				case LuauOpcode::LOP_NEWCLOSURE: {
					buffer->mnenomic = "newclosure";
					buffer->hint = "New closure.";
					break;
				}

				case LuauOpcode::LOP_NEWTABLE: {
					buffer->mnenomic = "newtable";
					buffer->hint = "New table.";
					break;
				}

				case LuauOpcode::LOP_NOP: {
					buffer->mnenomic = "nop";
					buffer->hint = "Nop.";
					break;
				}

				case LuauOpcode::LOP_NOT: {
					buffer->mnenomic = "not";
					buffer->hint = "Not.";
					break;
				}

				case LuauOpcode::LOP_OR: {
					buffer->mnenomic = "or";
					buffer->hint = "Or.";
					break;
				}

				case LuauOpcode::LOP_ORK: {
					buffer->mnenomic = "ork";
					buffer->hint = "Or constant.";
					break;
				}

				case LuauOpcode::LOP_POW: {
					buffer->mnenomic = "pow";
					buffer->hint = "Pow.";
					break;
				}

				case LuauOpcode::LOP_POWK: {
					buffer->mnenomic = "powk";
					buffer->hint = "Pow constant.";
					break;
				}

				case LuauOpcode::LOP_PREPVARARGS: {
					buffer->mnenomic = "prepvarargs";
					buffer->hint = "Prepare variable args.";
					break;
				}

				case LuauOpcode::LOP_RETURN: {
					buffer->mnenomic = "return";
					buffer->hint = "Return.";
					break;
				}

				case LuauOpcode::LOP_SETGLOBAL: {
					buffer->mnenomic = "setglobal";
					buffer->hint = "Set global.";
					break;
				}

				case LuauOpcode::LOP_SETLIST: {
					buffer->mnenomic = "setlist";
					buffer->hint = "Set list table.";
					break;
				}

				case LuauOpcode::LOP_SETTABLE: {
					buffer->mnenomic = "settable";
					buffer->hint = "Set table.";
					break;
				}

				case LuauOpcode::LOP_SETTABLEKS: {
					buffer->mnenomic = "settableks";
					buffer->hint = "Set table constant.";
					break;
				}

				case LuauOpcode::LOP_SETTABLEN: {
					buffer->mnenomic = "settablen";
					buffer->hint = "Settable numeral.";
					break;
				}

				case LuauOpcode::LOP_SETUPVAL: {
					buffer->mnenomic = "setupval";
					buffer->hint = "Set upvalue.";
					break;
				}

				case LuauOpcode::LOP_SUB: {
					buffer->mnenomic = "sub";
					buffer->hint = "Sub.";
					break;
				}

				case LuauOpcode::LOP_SUBK: {
					buffer->mnenomic = "subk";
					buffer->hint = "Sub constant.";
					break;
				}

				default: {
					throw std::runtime_error("Unkown opcode in dissassembler.");
				}

			}

			break;
		}

		case set_action::operands: {

			const auto op_count = op_table.operands.size();

			for (auto i = 0u; i < op_count; ++i) {

				auto current_operand = std::make_shared<LuaU_dissassembler::operand>();

				std::intptr_t operand_value = 0;

				const auto operand = op_table.operands[i]; 
				const auto type = op_table.types[i];
				
				const auto split = ((i + 1u) == op_count) ? " " : ", ";

				/* Set operand value. */
				switch (operand) {

					case op_table::operands::A: {
						operand_value = decode_A(buffer->code[0]);
						break;
					}

					case op_table::operands::B: {
						operand_value = decode_B(buffer->code[0]);
						break;
					}

					case op_table::operands::C: {
						operand_value = decode_C(buffer->code[0]);
						break;
					}

					case op_table::operands::C_dec: {
						operand_value = decode_C(buffer->code[0]) - 1;
						break;
					}

					case op_table::operands::C_inc: {
						operand_value = decode_C(buffer->code[0]) + 1;
						break;
					}

					case op_table::operands::D: {
						operand_value = decode_D(buffer->code[0]);
						break;
					}

					case op_table::operands::E: {
						operand_value = decode_E(buffer->code[0]);
						break;
					}

					case op_table::operands::AUX: {
						buffer->code += 1u;
						operand_value = buffer->code[0];
						break;
					}

					case op_table::operands::AUX_24: {
						buffer->code += 1u;
						operand_value = buffer->code[0] & 0xffffff;
						break;
					}

					default: {
						throw std::runtime_error("Unkown operand type.");
					}

				}

				/* Set type and operand. */
				current_operand->type = type;
				current_operand->operand = operand;

				/* Set type. */
				switch (type) {
				
					/* Capture_ref follows. */
					case op_table::type::capture_idx: {
						
						const auto on = buffer->code[0];
						const auto tt = decode_A(on);

						current_operand->capture_ref = tt;

						/* Upvalue. */
						if (tt == 2u)
							buffer->data += std::to_string(tt);
						else
							buffer->data += std::to_string(tt);
										
						break;
					}

					case op_table::type::capture_ref: {

						const auto on = buffer->code[0];
						const auto tt = decode_A(on);
						const auto v = decode_B(on);


						current_operand->capture_reg = v;

						/* Upvalue. */
						if (tt == 2u)
							buffer->data += "upvalue_" + std::to_string(v);
						else				
							buffer->data += "r" + std::to_string(v);

						break;
					}

					case op_table::type::fastcall_idx: {
						buffer->data += std::string (op_table::fastcall_array[operand_value]) + split;
						current_operand->fastcall_idx = std::uint8_t(operand_value);
						break;
					}

					case op_table::type::table_size: {
						const auto size = (!operand_value ? 0 : (1 << (operand_value - 1)));
						buffer->data += std::to_string(size) + split;
						current_operand->table_size = size;
						break;
					}

					case op_table::type::proto: {
						buffer->data += "proto_" + std::to_string(operand_value) + split;
						current_operand->proto = operand_value;
						break;
					}

					case op_table::type::table: {
						buffer->data += "table_" + std::to_string(operand_value) + split;
						current_operand->table = operand_value;
						break;
					}

					case op_table::type::upvalue: {
						buffer->data += "upvalue_" + std::to_string(operand_value) + split;
						current_operand->upvalue = operand_value;
						break;
					}

					case op_table::type::slot: {
						buffer->data += std::to_string(operand_value) + split;
						current_operand->slot = operand_value;
						break;
					}

					case op_table::type::aux: {
						buffer->data += std::to_string(operand_value) + split;
						current_operand->aux = operand_value;
						break;
					}

					case op_table::type::jmp: {

						buffer->data += std::to_string(operand_value) + split;
						current_operand->jmp = operand_value;
						current_operand->jmp_addr = current_operand->jmp + buffer->addr + 1u;

						break;
					}

					case op_table::type::reg: {
						buffer->data += 'r' + std::to_string(operand_value) + split;
						current_operand->reg = std::uint8_t(operand_value);
						break;
					}

					case op_table::type::val : {
						buffer->data += std::to_string(operand_value) + split;
						current_operand->val = operand_value;
						break;
					}

					case op_table::type::val_dec: {
						--operand_value;
						buffer->data += std::to_string(operand_value) + split;
						current_operand->val = operand_value;
						break;
					}

					case op_table::type::import_idx: {

						const auto source = buffer->code[1];
						const std::int32_t brco = source >> 30;

						const auto id1 = (brco > 0) ? std::int32_t(source >> 20) & 1023 : -1;
						const auto id2 = (brco > 1) ? std::int32_t(source >> 10) & 1023 : -1;
						const auto id3 = (brco > 2) ? std::int32_t(source) & 1023 : -1;

						if (id1 >= 0) 
							current_operand->k_value += gco2ts(k[id1].value.gc)->data;
						if (id2 >= 0)
							current_operand->k_value += std::string (".") + std::string(gco2ts(k[id2].value.gc)->data);
						if (id3 >= 0)
							current_operand->k_value += std::string(".") + std::string(gco2ts(k[id3].value.gc)->data);

						buffer->data += current_operand->k_value + split;
						current_operand->import_idx = id3;

						break;
					}

					case op_table::type::k_value_nstr: {

						const auto kv = k[operand_value];
						current_operand->k_value =  std::string(kv.value.gc->ts.data);

						buffer->data += current_operand->k_value + split;

						current_operand->k_idx = operand_value;

						break;
					}

					case op_table::type::k_idx: {
						
						const auto kv = k[operand_value];
						
						switch (kv.tt) {

							case LUA_TNIL: {
								current_operand->k_value = "nil";
								break;
							}

							case LUA_TBOOLEAN: {
								current_operand->k_value = std::to_string (kv.value.b);
								break;
							}

							case LUA_TNUMBER: {
								if (std::floor(kv.value.n) == kv.value.n)
									current_operand->k_value = std::to_string(std::intptr_t(kv.value.n));
								else
									current_operand->k_value = std::to_string(kv.value.n);
								break;
							}

							case LUA_TSTRING: {
								current_operand->k_value = std::string ("\"") + kv.value.gc->ts.data + '\"';
								break;
							}

							case LUA_TLIGHTUSERDATA: {
								current_operand->k_value = "lightuserdata_" + std::to_string(operand_value);
								break;
							}

							case LUA_TFUNCTION: {
								current_operand->k_value = "closure_" + std::to_string(operand_value);
								break;
							}

							case LUA_TTABLE: {
								current_operand->k_value = "table_" + std::to_string(operand_value);
								break;
							}

							case LUA_TUSERDATA: {
								current_operand->k_value = "userdata_" + std::to_string(operand_value);
								break;
							}

							case LUA_TUPVAL: {
								current_operand->k_value = "upval_" + std::to_string(operand_value);
								break;
							}

						}

						buffer->data += current_operand->k_value + split;
						current_operand->k_idx = operand_value;
						break;
					}

				}

				buffer->operands.emplace_back(current_operand);
			}

			break;
		}

	}

	return;
}


void LuaU_dissassembler::dissassemble(const std::uintptr_t pc, const Proto* p, std::shared_ptr<LuaU_dissassembler::dissassembly>& buffer) {

	/* Get intruction and set instruction. */
	const auto code = op_table::op_table[decode_opcode(p->code[pc])];
	set_data<set_action::instruction>(buffer, p->k, code);

	/* Set code. */
	const auto start_pc = p->code + pc;
	buffer->code = start_pc;
	buffer->addr = pc;

	/* Clear for next. */
	buffer->operands.clear();

	/* Init data. */
	buffer->data = std::string (buffer->mnenomic) + ' ';
	set_data<set_action::operands>(buffer, p->k, code);

	/* Calulate lenght. */
	buffer->len = (std::uint8_t(reinterpret_cast<const std::uintptr_t>(buffer->code) - reinterpret_cast<const std::uintptr_t>(start_pc)) / sizeof(Instruction)) + 1u;

	return;
}