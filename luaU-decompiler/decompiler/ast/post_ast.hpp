#pragma once
#include "ast_dec.hpp"

namespace ast_post {
	
	namespace table {
		
		/* Sets every table end in every table routine node too address ending of table. */
		void set_node_end(const std::shared_ptr<ast_dec::ast>& ast);

		/* Set indexs exprs for gettable instructions. */
		void set_indexs(const std::shared_ptr<ast_dec::ast>& ast);

	}

	namespace arith {

		/* Sets arith exprs. */
		void set_arith_exprs(const std::shared_ptr<ast_dec::ast>& ast);

	}

}