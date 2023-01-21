#pragma once
#include "transpiler.hpp"


/* Flag */
#define flag_first -1
#define flag_compare -1
#define flag_mulret -2


/* Macros */
#define char_valid(ch) ((ch >= 0x30 /* '0' */ && ch <= 0x39 /* '9' */) || (ch >= 0x41 /* 'A' */ && ch <= 0x5A /* 'Z' */) || (ch >= 0x61 /* 'a' */ && ch <= 0x7A /* 'z' */))

/* Sees if idx needs string type idx. */
#define str_idx(idx) std::isdigit(idx.front()) || std::find_if(idx.begin(), idx.end(), [](const char c) { return (!std::isalpha(c) && !std::isdigit(c)); }) != idx.end()

/* Config has char suffix? */
#define config_char(config) config->upvalue_suffix_char || config->arg_suffix_char || config->function_suffix_char || config->iteration_suffix_char || config->var_suffix_char
