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

#define spacing "   "

static const char* const new_scope[] = {
	"elseif",
	"else",
	"if",
	"repeat",
	"function",
	"local function",
	"for",
	"while",
	"--[[",
	"(function" 
};

static const char* const end_scope[] = {
	"end",
	"until",
	"]]"
};

void clean_up::buetify(std::string& decom) {

	const auto indent = std::string(spacing);

	/* How many indentations we currently want. */
	std::int64_t multiplier = 0u;

	/* Makes sures it isn't in string. */
	bool clean = true;

	/* Add line ending at the front to signal compare. */
	decom.insert(decom.begin(), '\n');

	for (auto pos = 0u; pos < decom.length(); ++pos) {
		
		bool curr = false; /* Currently on indent. */
		const auto ch = decom[pos];

		/* Not inside string. */
		if (ch == '\"' || ch == '\'')
			clean ^= true;

		/* See if new line and were not in a string. */
		if (ch == '\n' && clean) {

			/* Set multiplier. */
			for (const auto i : new_scope)
				if (!decom.compare(pos + 1u, std::strlen(i), i)) {
					++multiplier;
					curr = true;
				}

			for (const auto i : end_scope)
				if (!decom.compare(pos + 1u, std::strlen(i), i))
					--multiplier;

			/* Fix mul */
			if (multiplier < 0)
				multiplier = 0;

			/* Newclosures look weird so reverse it. */
			if (!decom.compare(pos + 1u, (sizeof("(function") - 1u), "(function"))
				decom.erase(pos, 1u);


			/* Format */
			auto nmul = multiplier;
			if (nmul && curr) /* else, elseif need to dec. */
				--nmul;

			for (auto o = 0u; o < nmul; ++o)
				decom.insert(pos + 1, indent);

		}

	}

	return;
}

