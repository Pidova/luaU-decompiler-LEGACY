#pragma once
#include "../lexer/lexer.hpp"

/*

	Anything that needs to be emitted will be passed through here.

*/

namespace emitter {

	/* Compare emitting. */
	__inline void compare(const LuauOpcode op, const bool opposite, const bool nested, std::string& dest, const std::string& compare_1, const std::string& compare_2);

	/* String to string emitting. */
	__inline void str(std::string& dest, const std::string& src);

	/* C-String emitting. */
	__inline void c_str(std::string& dest, const char* const src);

	/* End of line emitting. */
	__inline void end_of_line(std::string& dest);

	/* Variable or argument equal emitting. */
	__inline void vararg_equal(std::string& dest, const std::string& vararg, const std::string& src);

	/* Arithmetic emitting.*/
	__inline void arith(const LuauOpcode op, const bool assignment, std::string& dest, const std::string& src1, const std::string& src2);

	/* Unary emitting. */
	__inline void unary(const LuauOpcode op, std::string& dest, const std::string& src1);

}