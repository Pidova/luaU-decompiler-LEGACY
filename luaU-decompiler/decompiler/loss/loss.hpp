#pragma once
#include "../ast/ast_dec.hpp"

namespace loss {

	/* Attempts too calculate loss against decompiled compiled assembly and original assembly (Not 100% accurate compiler changes can alter it **Assumes compilers are the same**) )*/
	float loss(const std::string& decompiled);
}