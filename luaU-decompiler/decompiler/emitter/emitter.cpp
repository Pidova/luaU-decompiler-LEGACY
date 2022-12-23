#include "emitter.hpp"


void emitter::compare(const LuauOpcode op, const bool opposite, const bool nested, std::string& dest, const char* const compare_type, const std::string& compare_1, const std::string& compare_2) {

    const char* cmp;

    switch (op) {

        case LuauOpcode::LOP_JUMPIFLT: {
            cmp = (!opposite) ? " < " : " > ";
            break;
        }

        case LuauOpcode::LOP_JUMPIFEQ: {
            cmp = (!opposite) ? " == " : " ";
            break;
        }

        case LuauOpcode::LOP_JUMPXEQKNIL: {
            cmp = (!opposite) ? " == " : " ";
            break;
        }

        case LuauOpcode::LOP_JUMPXEQKB: {
            cmp = (!opposite) ? " == " : " ";
            break;
        }

        case LuauOpcode::LOP_JUMPXEQKS: {
            cmp = (!opposite) ? " == " : " ";
            break;
        }

        case LuauOpcode::LOP_JUMPXEQKN: {
            cmp = (!opposite) ? " == " : " ";
            break;
        }

        case LuauOpcode::LOP_JUMPIFLE: {
            cmp = (!opposite) ? " <= " : " >= ";
            break;
        }

        case LuauOpcode::LOP_JUMPIFNOTEQ: {
            cmp = (!opposite) ? " ~= " : " == ";
            break;
        }

        case LuauOpcode::LOP_JUMPIFNOTLE: {
            cmp = (!opposite) ? " >= " : " <= ";
            break;
        }

        case LuauOpcode::LOP_JUMPIFNOTLT: {
            cmp = (!opposite) ? " >= " : " <= ";
            break;
        }

        case LuauOpcode::LOP_JUMPIF: {
            cmp = (!opposite) ? " " : " not ";
            break;
        }

        case LuauOpcode::LOP_JUMPIFNOT: {
            cmp = (!opposite) ? " not " : " ";
            break;
        }

        default: {
            throw std::runtime_error("Unkown opcode when trying too emit compare.");
        }
  
    }

    dest += (nested) ? (compare_1 + cmp + compare_2) : (std::string (compare_type) + " (" + compare_1 + cmp + compare_2 + ") then\n");

	return;
}

void emitter::str(std::string& dest, const std::string& src) {
	dest += src;
	return;
}

void emitter::for_g_loop(std::string& dest, const std::string& vars, const std::string& iter) {
    dest += "for " + vars + " in " + iter + " do\n";
    return;
}

void emitter::for_n_loop(std::string& dest, const std::string& var, const std::string& iter) {
    dest += "for " + var + " = " + iter + " do\n";
    return;
}

void emitter::loop(std::string& dest, const std::string& type, const std::string& data, const char* const end) {
    dest += type + "( " + data + " )" + end;
    return;
}


void emitter::end_of_line(std::string& dest) {
    dest += ";\n";
    return;
}

void emitter::vararg_equal(std::string& dest, const std::string& vararg, const std::string& src) {
    dest += vararg + " = " + src;
    emitter::end_of_line(dest);
    return;
}

void emitter::new_vararg_equal(std::string& dest, const std::string& vararg, const std::string& src) {
    dest += "local " + vararg + " = " + src;
    emitter::end_of_line(dest);
    return;
}

void emitter::write_line(std::string& dest, const std::string& src) {
    dest += src;
    emitter::end_of_line(dest);
    return;
}

void emitter::expandable_comment(std::string& dest, const std::string& src) {
    dest += "--[[\n" + src + ((src.back() != '\n') ? "]]\n" : "]]\n");
    return;
}

void emitter::arith(const LuauOpcode op, const bool assignment, std::string& dest, const std::string& src1, const std::string& src2) {

    switch (op) {
        
        case LuauOpcode::LOP_ADDK:
        case LuauOpcode::LOP_ADD: {

            if (!assignment)
                dest += src1 + " + " + src2;
            else {
                dest += src1 + " += " + src2;
                emitter::end_of_line(dest);
            }
            
            break;
        }

        case LuauOpcode::LOP_MODK:
        case LuauOpcode::LOP_MOD: {

            if (!assignment)
                dest += src1 + " % " + src2;
            else {
                dest += src1 + " %= " + src2;
                emitter::end_of_line(dest);
            }

            break;
        }

        case LuauOpcode::LOP_SUBK:
        case LuauOpcode::LOP_SUB: {

            if (!assignment)
                dest += src1 + " - " + src2;
            else {
                dest += src1 + " -= " + src2;
                emitter::end_of_line(dest);
            }

            break;
        }

        case LuauOpcode::LOP_DIVK:
        case LuauOpcode::LOP_DIV: {

            if (!assignment)
                dest += src1 + " / " + src2;
            else {
                dest += src1 + " /= " + src2;
                emitter::end_of_line(dest);
            }

            break;
        }

        case LuauOpcode::LOP_MULK:
        case LuauOpcode::LOP_MUL: {         

            if (!assignment)
                dest += src1 + " * " + src2;
            else {
                dest += src1 + " *= " + src2;
                emitter::end_of_line(dest);
            }

            break;
        }

        case LuauOpcode::LOP_POWK:
        case LuauOpcode::LOP_POW: {
            
            if (!assignment)
                dest += src1 + " ^ " + src2;
            else {
                dest += src1 + " ^= " + src2;
                emitter::end_of_line(dest);
            }

            break;
        }

        case LuauOpcode::LOP_ANDK:
        case LuauOpcode::LOP_AND: {
           
            if (!assignment)
                dest += src1 + " and " + src2;
            else {
                dest += src1 + " = " + src1 + " and " + src2;
                emitter::end_of_line(dest);
            }

            break;
        }

        case LuauOpcode::LOP_ORK:
        case LuauOpcode::LOP_OR: {

            if (!assignment)
                dest += src1 + " or " + src2;
            else {
                dest += src1 + " = " + src1 + " or " + src2;
                emitter::end_of_line(dest);
            }

            break;
        }

        default: {
            throw std::runtime_error("Unkown opcode when trying too emit arithmetic.");
        }

    }

    return;
}

void emitter::unary(const LuauOpcode op, std::string& dest, const std::string& src1) {

    switch (op) {

        case LuauOpcode::LOP_NOT: {
            dest += "not " + src1;
            break;
        }

        case LuauOpcode::LOP_MINUS: {
            dest += "-" + src1;
            break;
        }

        case LuauOpcode::LOP_LENGTH: {
            dest += "#" + src1;
            break;
        }

        default: {
            throw std::runtime_error("Unkown opcode when trying too emit unary.");
        }

    }

    return;
}