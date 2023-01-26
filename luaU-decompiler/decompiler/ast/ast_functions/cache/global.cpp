#include "../ast_functions.hpp"

std::unordered_map<std::shared_ptr<ast_dec::node> /* Branch */, std::shared_ptr<ast_dec::node> /* End */> global_cache::scopes::cached_ends;

void global_cache::clear() {

	global_cache::scopes::cached_ends.clear();

	return;
}

