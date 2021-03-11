#pragma once
#ifndef SOLVER_ENCODING
#define SOLVER_ENCODING


#include <memory>
#include <string>
#include <ostream>
#include "cola/ast.hpp"
#include "heal/logic.hpp"


namespace solver {

	struct EncodedTerm {
		virtual ~EncodedTerm() = default;
		virtual bool operator==(const EncodedTerm& other) const = 0;
		
		virtual void Print(std::ostream& stream) const = 0;
		[[nodiscard]] virtual std::shared_ptr<EncodedTerm> Negate() const = 0;
		[[nodiscard]] virtual std::shared_ptr<EncodedTerm> And(std::shared_ptr<EncodedTerm> term) const = 0;
		[[nodiscard]] virtual std::shared_ptr<EncodedTerm> Or(std::shared_ptr<EncodedTerm> term) const = 0;
		[[nodiscard]] virtual std::shared_ptr<EncodedTerm> XOr(std::shared_ptr<EncodedTerm> term) const = 0;
		[[nodiscard]] virtual std::shared_ptr<EncodedTerm> Implies(std::shared_ptr<EncodedTerm> term) const = 0;
		[[nodiscard]] virtual std::shared_ptr<EncodedTerm> Iff(std::shared_ptr<EncodedTerm> term) const = 0;
		[[nodiscard]] virtual std::shared_ptr<EncodedTerm> Equal(std::shared_ptr<EncodedTerm> term) const = 0;
		[[nodiscard]] virtual std::shared_ptr<EncodedTerm> Distinct(std::shared_ptr<EncodedTerm> term) const = 0;
	};

	struct EncodedSymbol {
		virtual ~EncodedSymbol() = default;
		virtual bool operator==(const EncodedSymbol& other) const = 0;
		virtual void Print(std::ostream& stream) const = 0;
        [[nodiscard]] virtual std::shared_ptr<EncodedTerm> ToTerm() const = 0;
	};


	struct Term final {
		std::shared_ptr<EncodedTerm> repr;
		explicit Term(std::shared_ptr<EncodedTerm> repr_) : repr(std::move(repr_)) { assert(repr); }

		inline bool operator==(const Term& other) const { return *repr == *other.repr; }
		inline void Print(std::ostream& stream) const { repr->Print(stream); }

		[[nodiscard]] inline Term Negate() const { return Term(repr->Negate()); }
		[[nodiscard]] inline Term And(const Term& term) const { return Term(repr->And(term.repr)); }
		[[nodiscard]] inline Term Or(const Term& term) const { return Term(repr->Or(term.repr)); }
		[[nodiscard]] inline Term XOr(const Term& term) const { return Term(repr->XOr(term.repr)); }
		[[nodiscard]] inline Term Implies(const Term& term) const { return Term(repr->Implies(term.repr)); }
		[[nodiscard]] inline Term Iff(const Term& term) const { return Term(repr->Iff(term.repr)); }
		[[nodiscard]] inline Term Equal(const Term& term) const { return Term(repr->Equal(term.repr)); }
		[[nodiscard]] inline Term Distinct(const Term& term) const { return Term(repr->Distinct(term.repr)); }
	};

	struct Symbol final {
		std::shared_ptr<EncodedSymbol> repr;
		explicit Symbol(std::shared_ptr<EncodedSymbol> repr_) : repr(std::move(repr_)) { assert(repr); }

		inline bool operator==(const Symbol& other) const { return *repr == *other.repr; }
		inline void Print(std::ostream& stream) const { repr->Print(stream); }
		inline operator Term() const { return Term(repr->ToTerm()); }

		[[nodiscard]] inline Term Negate() const { return Term(repr->ToTerm()->Negate()); }
		[[nodiscard]] inline Term And(const Term& term) const { return Term(repr->ToTerm()->And(term.repr)); }
		[[nodiscard]] inline Term Or(const Term& term) const { return Term(repr->ToTerm()->Or(term.repr)); }
		[[nodiscard]] inline Term XOr(const Term& term) const { return Term(repr->ToTerm()->XOr(term.repr)); }
		[[nodiscard]] inline Term Implies(const Term& term) const { return Term(repr->ToTerm()->Implies(term.repr)); }
		[[nodiscard]] inline Term Iff(const Term& term) const { return Term(repr->ToTerm()->Iff(term.repr)); }
		[[nodiscard]] inline Term Equal(const Term& term) const { return Term(repr->ToTerm()->Equal(term.repr)); }
		[[nodiscard]] inline Term Distinct(const Term& term) const { return Term(repr->ToTerm()->Distinct(term.repr)); }
	};

	struct Selector final {
		const cola::Type& type;
		std::string fieldname;

		Selector(const cola::Type& type, std::string fieldname) : type(type), fieldname(fieldname) {
			assert(type.has_field(fieldname));
		}
		inline void Print(std::ostream& stream) const { stream << "{" << type.name << "}->" << fieldname; }
		inline bool operator==(const Selector& other) const { return &type == &other.type && fieldname == other.fieldname; }
		inline bool operator<(const Selector& other) const {
			if (&type < &other.type) return true;
			else if (&type > &other.type) return false;
			else return fieldname < other.fieldname;
		}
	};


	inline std::ostream& operator<<(std::ostream& stream, const EncodedSymbol& symbol) {
		symbol.Print(stream);
		return stream;
	}

	inline std::ostream& operator<<(std::ostream& stream, const EncodedTerm& term) {
		term.Print(stream);
		return stream;
	}

	inline std::ostream& operator<<(std::ostream& stream, const Symbol& symbol) {
		symbol.Print(stream);
		return stream;
	}

	inline std::ostream& operator<<(std::ostream& stream, const Term& term) {
		term.Print(stream);
		return stream;
	}

	inline std::ostream& operator<<(std::ostream& stream, const Selector& selector) {
		selector.Print(stream);
		return stream;
	}

} // namespace solver

#endif