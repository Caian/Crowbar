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

#include <string>
#include <iostream>
#include <unordered_map>
#include <stdexcept>
#include <sstream>
#include <stdlib.h>
#include <math.h> 

#include "Crowbar.h"
#include "CallTree.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;
using namespace std;


/*--------------------------------------------------------------------------*/
/* Random within range                                                      */
/*--------------------------------------------------------------------------*/
int random(int l, int u);

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
/* Matcher for the calls                                                    */
/*--------------------------------------------------------------------------*/
class TreeRedirector : public MatchFinder::MatchCallback
{
private:

	const LangOptions* lopt;
	const CALLTREE* tree;
	const unordered_map<int64, CALLSITE*> callmap;
	Replacements* replacements;

	int runRE(const DeclRefExpr *md, SourceManager &sm)
	{
		DeclarationNameInfo info = md->getNameInfo();
		std::string name = info.getAsString();
		
		// Maybe it's calling external code, if it has no 
		// local body then the call is irrelevant
		auto method = this->tree->methods.find(name);
		if (method == this->tree->methods.end())
			return 0;
			
		int64 begin, end;
		getAbsoluteLocation(FullSourceLoc(md->getLocStart(), sm), 
				FullSourceLoc(md->getLocEnd(), sm),
				&begin, &end, *this->lopt);

		stringstream ss;
		ss << begin << '-' << end;
		string sloc = ss.str();

		auto call = this->callmap.find(begin);
		if (call == this->callmap.end())
		{
			// The call was not selected to be redirected or
			// it is a reference to the function without 
			// being a call
			return 0;
		}

		int r = call->second->redirect;
		if (r == 0) return 0;

		CharSourceRange range = CharSourceRange::
			getTokenRange(SourceRange(info.getLoc()));

		ss.str(std::string());
		ss << 'r' << r << '_' << name;

		string newname = ss.str();
		this->replacements->insert(Replacement(sm, range, newname));

		cout << name << ',' << sloc << ',' << r << endl;

		return 0;
	}

public:

	TreeRedirector(const LangOptions* lopt, const CALLTREE* tree, 
			const unordered_map<int64, CALLSITE*> callmap, 
			Replacements* repl) : 
		lopt(lopt),
		tree(tree),
		callmap(callmap),
		replacements(repl)
	{
	}

	virtual void run(const MatchFinder::MatchResult &Result) 
	{
		SourceManager &sm = Result.Context->getSourceManager();
		if (const DeclRefExpr *md = Result.Nodes.getNodeAs<clang::DeclRefExpr>("id"))
			this->runRE(md, sm);
	}
};

/*--------------------------------------------------------------------------*/
/* Redirect calls in the tree                                               */
/*--------------------------------------------------------------------------*/
int RedirectCallTree(RefactoringTool& tool, const LangOptions* lopt, 
		const CALLTREE* tree, int seed, int maxredirect)
{
	srand(seed);

	unordered_map<int64, CALLSITE*> callmap;

	Replacements& replacements = tool.getReplacements();

	for (auto& m : tree->methods)
	{
		vector<CALLSITE*> calls;

		for (auto& c : m.second->calls)
		{
			c->redirect = 0;
			calls.push_back(c);
		}

		int reps = m.second->repeats;
		int redirs;
	   
		if (maxredirect < 0)
		{
			redirs = (int)round((double)maxredirect / 100.0 * 
					(double)m.second->calls.size());
		}
		else
		{
			redirs = (maxredirect > (int)m.second->calls.size()) ?
				(int)m.second->calls.size() : maxredirect;
		}

		for (int i = 0; i < redirs; i++)
		{
			int p = random(0, (int)calls.size()-1);
			int r = random(0, reps);

			auto e = calls.begin() + p;
			CALLSITE* c = *e;
			c->redirect = r;

			callmap[c->location.begin] = c;

			calls.erase(e);
		}
	}

	MatchFinder matchFinder;
	TreeRedirector treeRedirector(lopt, tree, callmap, &replacements);

	StatementMatcher refMatcher = declRefExpr().bind("id");
	matchFinder.addMatcher(refMatcher, &treeRedirector);

	assert_tool(tool.runAndSave(newFrontendActionFactory(&matchFinder).get()));

	return 0;
}

