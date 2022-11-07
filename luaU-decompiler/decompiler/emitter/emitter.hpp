#pragma once
#include "../lexer/lexer_dec.hpp"

/*

	Anything that needs to be emitted will be passed through here.

*/

namespace emitter {

	/* Compare emitting. */
    void compare(const LuauOpcode op, const bool opposite, const bool nested, std::string& dest, const char* const compare_type, const std::string& compare_1, const std::string& compare_2);

	/* String to string emitting. */
	void str(std::string& dest, const std::string& src);

	/* End of line emitting. */
	void end_of_line(std::string& dest);

	/* Variable or argument equal emitting. */
	void vararg_equal(std::string& dest, const std::string& vararg, const std::string& src);

	/* Arithmetic emitting.*/
	void arith(const LuauOpcode op, const bool assignment, std::string& dest, const std::string& src1, const std::string& src2);

	/* Unary emitting. */
	void unary(const LuauOpcode op, std::string& dest, const std::string& src1);

}