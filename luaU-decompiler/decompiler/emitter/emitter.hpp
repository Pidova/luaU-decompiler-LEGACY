#pragma once
#include <stdexcept>
#include "../lexer/lexer_dec.hpp"

/*

	Anything that needs to be emitted will be passed through here.
	Append
*/
namespace emitter {

	/* Compare emitting. */
    void compare(const LuauOpcode op, const bool opposite, const bool nested, std::string& dest, const char* const compare_type, const std::string& compare_1, const std::string& compare_2);

	/* String to string emitting. */
	void str(std::string& dest, const std::string& src);

	/* End of line emitting. */
	void end_of_line(std::string& dest);

	/* Writes line with eol. */
	void write_line(std::string& dest, const std::string& src);

	/* Variable or argument equal emitting. */
	void vararg_equal(std::string& dest, const std::string& vararg, const std::string& src);

	/* New variable or argument equal emitting. */
	void new_vararg_equal(std::string& dest, const std::string& vararg, const std::string& src);

	/* Arithmetic emitting.*/
	void arith(const LuauOpcode op, const bool assignment, std::string& dest, const std::string& src1, const std::string& src2);

	/* Unary emitting. */
	void unary(const LuauOpcode op, std::string& dest, const std::string& src1);

	/* Expandable comment emmitting. */
	void expandable_comment(std::string& dest, const std::string& src);

	/* For gloop */
	void for_g_loop(std::string& dest, const std::string& vars, const std::string& iter);

	/* For nloop */
	void for_n_loop(std::string& dest, const std::string& var, const std::string& iter);

	/* Loop express(while, until) **use compare for final compare and compare flag for concatation** */
	void loop(std::string& dest, const std::string& type, const std::string& data, const char* const end = " do\n" /* Defualt while loop. */);

	/* Function */
	void function(std::string& dest, const std::string& type, const std::string& name, const std::string& args, const std::string data, const std::string& close);

	/* Emits locvar name too dest with prefix and suffix and suffix chars to turn suffix digits into english characters. */
	void locvar_name(std::string& dest, const std::string& prefix, const std::size_t suffix, const bool suffix_chars);

	/* Overides dest. */
	namespace override {

		/* Emits locvar name too dest with prefix and suffix and suffix chars to turn suffix digits into english characters. */
		void locvar_name(std::string& dest, const std::string& prefix, const std::size_t suffix, const bool suffix_chars);

	}

	/* Create new string instance and emit too it. */
	namespace create {

		/* Emits locvar name too dest with prefix and suffix and suffix chars to turn suffix digits into english characters. */
		std::string locvar_name(const std::string& prefix, const std::size_t suffix, const bool suffix_chars);

	}

}