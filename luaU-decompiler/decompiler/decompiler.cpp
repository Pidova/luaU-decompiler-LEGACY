
#include "decompiler.hpp"
#include "clean up/clean.hpp"
#include "emitter/emitter.hpp"
#include "loss/loss.hpp"

std::string luaU_decompiler::decompile(Proto *proto, const std::shared_ptr<transpiler_data::transpiler_config> &config) {

      const auto ast = ast_dec::gen_ast(proto, config);
      auto decom = transpiler::transpile(ast, config);

      if (config->post_loss) {

            bool error = false;
            const auto loss = loss::loss(ast, decom, error);

            if (error) {
                  emitter::expandable_comment_pre(decom, "Unable to compile decompilation!");
            } else {
                  emitter::expandable_comment_pre(decom, ((loss < 0) ? std::string("Loss: " + std::to_string(loss) + "%") : (loss != 0) ? std::string("Gain: " + std::to_string(loss) + "%")
                                                                                                                                        : std::string("Perfect decompilation!")));
            }
      }

      clean_up::clean(decom);
      clean_up::buetify(decom);

      return decom;
}