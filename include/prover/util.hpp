#pragma once
#ifndef PROVER_UTIL
#define PROVER_UTIL


#include <memory>
#include "encoder.hpp"


namespace prover {
	
	class QuantifiedVariableGenerator {
		private:
			std::shared_ptr<Encoder> encoder;
			std::size_t counter = 0;

		public:
			QuantifiedVariableGenerator(std::shared_ptr<Encoder> encoder);
			Symbol GetNext(const cola::Type& type);
	};

	std::vector<Symbol> MakeQuantifiedVariables(std::shared_ptr<Encoder> encoder, std::vector<std::reference_wrapper<const cola::Type>> types);

	template<typename... T>
	std::array<Symbol, sizeof...(T)> MakeQuantifiedVariables(std::shared_ptr<Encoder> encoder, const T&... types) {
		QuantifiedVariableGenerator generator(std::move(encoder));
		std::array<Symbol, sizeof...(T)> result = { generator.GetNext(types)... };
		return result;
	}

} // namespace prover

#endif