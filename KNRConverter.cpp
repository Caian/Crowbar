/*--------------------------------------------------------------------------*/
/*                                                                          */
/* Crowbar Code Refactoring Tool                                            */
/* author: Caian Benedicto                                                  */
/* contact: caianbene@gmail.com (with a [Crowbar] tag in the subject)       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Lex/Lexer.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/Support/CommandLine.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include <string>
#include <iostream>
#include <unordered_map>
#include <stdexcept>
#include <sstream>
#include <stdlib.h>
#include <math.h> 
#include <list>

#include "Crowbar.h"
#include "CallTree.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;
using namespace std;


/*--------------------------------------------------------------------------*/
/* Convert a location to the absolute position inside the source file       */
/*--------------------------------------------------------------------------*/
void getAbsoluteLocation(const FullSourceLoc& l, int64* pBegin, int64* pEnd,
		const LangOptions& lopt);

/*--------------------------------------------------------------------------*/
/* Convert a location to the absolute position inside the source file       */
/*--------------------------------------------------------------------------*/
void getAbsoluteLocation(const FullSourceLoc& lb, const FullSourceLoc& le, 
		int64* pBegin, int64* pEnd, const LangOptions& lopt);

/*--------------------------------------------------------------------------*/
/* Matcher for the methods                                                  */
/*--------------------------------------------------------------------------*/
class TreeKNRConverter : public MatchFinder::MatchCallback
{
private:

	const LangOptions* lopt;
	list<Replacement> replacements;

	int runFD(const FunctionDecl *md, SourceManager &sm)
	{
		// Nope.avi
		if (md->isImplicit())
			return 0;

		// I don't think a declaration can be in K&R form
		if (!md->isThisDeclarationADefinition())
			return 0;

		stringstream ss;
		DeclarationNameInfo info = md->getNameInfo();

		FullSourceLoc b(md->getLocStart(), sm), _e(info.getEndLoc(), sm);
		FullSourceLoc e(clang::Lexer::getLocForEndOfToken(_e, 0, sm, *this->lopt), sm);
		string pre(sm.getCharacterData(b), sm.getCharacterData(e)-sm.getCharacterData(b));

		Stmt* body = md->getBody();
		SourceLocation bs = body->getLocStart();
		SourceLocation ne = body->getLocEnd();

		FullSourceLoc pb(bs, sm), _pe(ne, sm);
		FullSourceLoc pe(clang::Lexer::getLocForEndOfToken(_pe, 0, sm, *this->lopt), sm);
	
		string sbody(sm.getCharacterData(pb), sm.getCharacterData(pe)-sm.getCharacterData(pb));

		// Try to identify K&R

		string limbo(sm.getCharacterData(e), sm.getCharacterData(pb)-sm.getCharacterData(e));
		if (limbo.find(";") == string::npos)
			return 0;
		
		// Deal with it [glasses]

		bool first = true;
		ss << pre << " (";

		for (auto& p : md->params())
		{
			string pname = p->getNameAsString();
			TypeSourceInfo* si = p->getTypeSourceInfo();
	
			if (si == NULL && pname.size() == 0)
				continue;

			// Holy moly look at dis 1337 stuffz!
			// I stole it from the guts of clang 
			// source code...

			string s;
			QualType t = si->getType();
			SplitQualType st = t.split();
			PrintingPolicy pp(*this->lopt);
			raw_string_ostream sos(s);
			Twine ph(pname);
			QualType::print(st.Ty, st.Quals, 
					sos, pp, ph);

			/*string ptype = QualType::getAsString(t.split());
			string paux;

			auto pbrackets = ptype.rfind("[");
			if (pbrackets != string::npos)
			{
				paux = ptype.substr(pbrackets, string::npos);
				ptype = ptype.substr(0, pbrackets);
			}*/

			if (first) first = false;
			else ss << ", ";

			//ss << ptype << ' ' << pname << paux;
			ss << sos.str();
		}

		if (md->isVariadic())
		{
			ss << ", ...";
		}

		ss << ")" << endl << sbody;

		FullSourceLoc db(md->getLocStart(), sm), _de(md->getLocEnd(), sm);
		FullSourceLoc de(clang::Lexer::getLocForEndOfToken(_de, 0, sm, *this->lopt), sm);
		CharSourceRange range = CharSourceRange::
			getTokenRange(SourceRange(md->getLocStart(), de));

		string prototype = ss.str();
		this->replacements.push_front(Replacement(sm, range, prototype));

		return 0;
	}

public:

	TreeKNRConverter(const LangOptions* lopt) : 
		lopt(lopt)
	{
	}

	const list<Replacement>& getReplacements()
	{
		return this->replacements;
	}

	virtual void run(const MatchFinder::MatchResult &Result) 
	{
		SourceManager &sm = Result.Context->getSourceManager();
		if (const FunctionDecl *md = Result.Nodes.getNodeAs<clang::FunctionDecl>("id"))
			this->runFD(md, sm);
	}
};


/*--------------------------------------------------------------------------*/
/* Repeat the methods in the tree                                           */
/*--------------------------------------------------------------------------*/
int FixKNRNotation(RefactoringTool& tool, const LangOptions* lopt)
{
	MatchFinder matchFinder;
	TreeKNRConverter treeConverter(lopt);

	DeclarationMatcher methodMatcher = functionDecl().bind("id");
	matchFinder.addMatcher(methodMatcher, &treeConverter);

	assert_tool(tool.run(newFrontendActionFactory(&matchFinder).get()));

	IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts = new DiagnosticOptions();
	TextDiagnosticPrinter DiagnosticPrinter(llvm::errs(), &*DiagOpts);
	DiagnosticsEngine Diagnostics(
			IntrusiveRefCntPtr<DiagnosticIDs>(new DiagnosticIDs()),
			&*DiagOpts, &DiagnosticPrinter, false);
	SourceManager Sources(Diagnostics, tool.getFiles());
	Rewriter Rewrite(Sources, *lopt);

	bool result = true;
	auto replacements = treeConverter.getReplacements();
	for (auto r = replacements.crbegin(); r != replacements.crend(); r++)
	{
		if (r->isApplicable())
			result = r->apply(Rewrite) && result;
		else 
			result = false;
	}

	if (!result)
		error("Failed to apply replacements!");

	if (Rewrite.overwriteChangedFiles())
		error("Failed to update sources!");

	return 0;
}

