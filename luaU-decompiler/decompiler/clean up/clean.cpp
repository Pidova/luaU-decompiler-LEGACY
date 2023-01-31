#include "clean.hpp"
#include "clean_macro.hpp"
#include <iostream>
#include <regex>
#include <string>

void replace_string(std::string &dest, const char *const srch, const char *const repl) {
      std::size_t pos = 0u;
      while ((pos = dest.find(srch, pos)) != std::string::npos) {
            dest.replace(pos, std::strlen(srch), repl);
            pos += std::strlen(repl);
      }
      return;
}

void clean_up::clean(std::string &decom) {

      /* Clean comments. */
      replace_string(decom, "]]\n--[[", "");
      replace_string(decom, "--[[", "--[[\n");

      return;
}

#define spacing "   "

static const char *const new_scope[] = {
    "if",
    "repeat",
    "function",
    "local function",
    "for",
    "while",
    "--[[",
    "(function"};

/* Dec indent for scope return to normal after. */
static const char *const prev_scope_expr[] = {
    "elseif",
    "else"};

static const char *const end_scope[] = {
    "end",
    "until",
    "]]"};

void clean_up::buetify(std::string &decom) {

      const auto indent = std::string(spacing);

      /* How many indentations we currently want. */
      std::int64_t multiplier = 0;
      std::int64_t table_multiplier = 0;
      std::int64_t parenthesis_counter = 0;

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

            /* Start */
            if (clean) {

                  if (ch == '(') {
                        ++parenthesis_counter;
                  }

                  if (ch == ')' && parenthesis_counter != 0) {
                        --parenthesis_counter;
                  }

                  if (ch == '{') {
                        ++table_multiplier;
                        parenthesis_counter = 0;
                  }

                  if (ch == '}' && table_multiplier != 0) {
                        --table_multiplier;
                  }

                  if (table_multiplier != multiplier && ch == ',' && !parenthesis_counter) {

                      #if buetify_indent_table

                        decom.insert(pos + 1, "\n");

                        for (auto o = 0u; o < table_multiplier; ++o)
                              decom.insert(pos + 2, indent);

                     #endif

                  }

            }



            /* See if new line and were not in a string. */
            if ((ch == '\n' || table_multiplier != multiplier) && clean) {

                  /* Set multiplier. */
                  for (const auto i : new_scope)
                        if (!decom.compare(pos + 1u, std::strlen(i), i)) {
                              ++multiplier;
                              ++table_multiplier;
                              curr = true;
                        }

                  /* Set multiplier. */
                  for (const auto i : prev_scope_expr)
                        if (!decom.compare(pos + 1u, std::strlen(i), i)) {
                              curr = true;
                        }

                  for (const auto i : end_scope)
                        if (!decom.compare(pos + 1u, std::strlen(i), i)) {
                              --multiplier;
                              --table_multiplier;
                        }

                  /* Fix mul */
                  if (multiplier < 0)
                        multiplier = 0;

                  /* Newclosures look weird so reverse it. */
                  if (!decom.compare(pos + 1u, (sizeof("(function") - 1u), "(function") && table_multiplier == multiplier) {
                        decom.erase(pos, 1u);
                        continue;
                  }

                  /* Format */

                  if (ch == '\n') {

                        auto nmul = multiplier;
                        if (nmul && curr) /* else, elseif need to dec. */
                              --nmul;

                        for (auto o = 0u; o < nmul; ++o)
                              decom.insert(pos + 1, indent);
                  }

            }
      }

      return;
}
