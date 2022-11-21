#pragma once
#include <iostream>
#include <regex>
#include <string>
#include "clean.hpp"


void replace_string(std::string& dest, const char* const srch, const char* const repl) {
	std::size_t pos = 0u;
	while ((pos = dest.find(srch, pos)) != std::string::npos) {
		dest.replace(pos, std::strlen(srch), repl);
		pos += std::strlen(repl);
	}
	return;
}

void clean_up::clean(std::string& decom) {

	/* Clean comments. */
    replace_string(decom, "]]\n--[[", "");
    replace_string(decom, "--[[", "--[[\n");
    replace_string(decom, "]]", "\n]]");

	return;
}


void clean_up::buetify(std::string& decom) {

	return;
}

