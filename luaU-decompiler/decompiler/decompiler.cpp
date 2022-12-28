#include "decompiler.hpp"
#include "clean up/clean.hpp"

std::string luaU_decompiler::decompile(Proto* proto, const std::shared_ptr<transpiler_data::transpiler_config>& config) {
	auto decom = transpiler::transpile(ast_dec::gen_ast(proto, config), config);
	clean_up::clean(decom);
	clean_up::buetify(decom);
	return decom;
}