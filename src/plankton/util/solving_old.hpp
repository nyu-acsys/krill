	//
	// Solving
	//

	bool implies_nonnull(const ConjunctionFormula& formula, const cola::Expression& expr);
	bool implies_nonnull(const SimpleFormula& formula, const cola::Expression& expr);
	
	bool implies(const ConjunctionFormula& formula, const cola::Expression& condition);
	bool implies(const SimpleFormula& formula, const cola::Expression& condition);

	bool implies(const ConjunctionFormula& formula, const ConjunctionFormula& implied);
	bool implies(const ConjunctionFormula& formula, const SimpleFormula& implied);
	bool implies(const SimpleFormula& formula, const ConjunctionFormula& implied);
	bool implies(const SimpleFormula& formula, const SimpleFormula& implied);

	bool implies(const Annotation& annotation, const Annotation& implied);

	/** Returns an annotation F such that the formulas 'annotation ==> F' and 'other ==> F' are tautologies.
	  * Opts for the strongest such annotation F.
	  */
	std::unique_ptr<Annotation> unify(std::unique_ptr<Annotation> annotation, std::unique_ptr<Annotation> other);

	/** Returns an annotation F such that the formula 'H ==> F' is a tautology for all passed annotations H.
	  * Opts for the strongest such annotation F.
	  */
	std::unique_ptr<Annotation> unify(std::vector<std::unique_ptr<Annotation>> annotations);

	//
	// Iterative Solving
	//

	/** Remains valid only as long as the premises it was constructued with remain allocated.
	  */
	struct IterativeImplicationSolver {
		virtual ~IterativeImplicationSolver() = default;
		virtual bool implies(const ConjunctionFormula& implied) = 0;
		virtual bool implies(const SimpleFormula& implied) = 0;
		virtual bool implies(const cola::Expression& implied) = 0;
	};

	std::unique_ptr<IterativeImplicationSolver> make_solver_from_premise(const ConjunctionFormula& premise);
	std::unique_ptr<IterativeImplicationSolver> make_solver_from_premise(const SimpleFormula& premise);