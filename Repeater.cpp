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
int random(int l, int u)
{
	int r = rand();
	return l + (int)round((double)(u - l) * (double)r / (double)RAND_MAX);
}


/*--------------------------------------------------------------------------*/
/* Matcher for the methods                                                  */
/*--------------------------------------------------------------------------*/
class TreeRepeater : public MatchFinder::MatchCallback
{
private:

	const LangOptions* lopt;
	const CALLTREE* tree;
	Replacements* replacements;

	int runFD(const FunctionDecl *md, SourceManager &sm)
	{
		if (md->isImplicit())
			return 0;

		string name = md->getNameAsString();

		auto method = this->tree->methods.find(name);
		if (method == this->tree->methods.end())
			return 0;

		auto m = method->second;

		if (m->repeats == 0)
			return 0;
		
		DeclarationNameInfo info = md->getNameInfo();

		FullSourceLoc b(md->getLocStart(), sm), _e(md->getLocEnd(), sm);
		FullSourceLoc e(clang::Lexer::getLocForEndOfToken(_e, 0, sm, *this->lopt), sm);

		stringstream ss;
		string pre, post;

		if (!md->isThisDeclarationADefinition())
		{
			// It turns out you need to change the declarations in order 
			// to scramble the calls after... bummer...
			
			FullSourceLoc b(md->getLocStart(), sm), _e(md->getLocEnd(), sm);
			FullSourceLoc e(clang::Lexer::getLocForEndOfToken(_e, 0, sm, *this->lopt), sm);
			
			SourceRange nameRange = SourceRange(info.getLoc());
			FullSourceLoc d(nameRange.getBegin(), sm), _f(nameRange.getEnd(), sm);
			FullSourceLoc f(clang::Lexer::getLocForEndOfToken(_f, 0, sm, *this->lopt), sm);

			string start(sm.getCharacterData(b), sm.getCharacterData(d)-sm.getCharacterData(b));
			string end(sm.getCharacterData(f), sm.getCharacterData(e)-sm.getCharacterData(f));

			pre = start;
			post = end + ";";

			//pre = "A";
			//post = "A";
		}
		else
		{
			// Only log the repetition of the body
			cout << name << ',' << m->repeats << endl;

			pre = m->pre;
			post = m->post;

			// Recursive methods are a corner case, if the recursive call of 
			// a repetition r(n) is selected for redirection to a repetition r(n+k) 
			// then this may generate a "Undefined method" error if there is no 
			// prototype above. To fix this, generate prototypes for every repetition.
			
			/*bool first = true;
			stringstream sp;
			sp << " (";

			for (auto& p : md->params())
			{
				if (first) first = false;
				else sp << ", ";

				SourceLocation bs = p->getLocStart();
				SourceLocation ne = p->getLocEnd();

				FullSourceLoc b(bs, sm), _e(ne, sm);
				FullSourceLoc e(clang::Lexer::getLocForEndOfToken(_e, 0, sm, *this->lopt), sm);
			
				string sparam(sm.getCharacterData(b), sm.getCharacterData(e)-sm.getCharacterData(b));
				sp << sparam;
			}

			sp << ")";
			string prototype = sp.str();*/
			
			SourceLocation bs = md->getBody()->getLocStart();
			SourceLocation ne = info.getLocEnd();

			FullSourceLoc _b(ne, sm), e(bs, sm);
			FullSourceLoc b(clang::Lexer::getLocForEndOfToken(_b, 0, sm, *this->lopt), sm);

			string prototype(sm.getCharacterData(b), sm.getCharacterData(e)-sm.getCharacterData(b));

			// Furiously generate prototypes
			
			//pre = "B";
			//post = "B";

			ss << pre << name << prototype << ";" << endl;
			for (int i = 1; i <= m->repeats; i++)
			{
				ss << pre << "r" << i << "_" <<
					name << prototype << ";" << endl;
			}
		}

		ss << pre << name << post << endl;
		for (int i = 1; i <= m->repeats; i++)
		{
			ss << pre << "r" << i << "_" << 
				name << post << endl;
		}
	
		CharSourceRange range = CharSourceRange::
			getTokenRange(SourceRange(b, e));

		string s = ss.str();
		this->replacements->insert(Replacement(sm, range, s));
		return 0;
	}

public:

	TreeRepeater(const LangOptions* lopt, const CALLTREE* tree, Replacements* repl) : 
		lopt(lopt),
		tree(tree),
		replacements(repl)
	{
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
int RepeatCallTree(RefactoringTool& tool, const LangOptions* lopt, 
		const CALLTREE* tree, int seed, int maxselect, int maxrepeat)
{
	srand(seed);

	vector<METHOD*> methods;

	Replacements& replacements = tool.getReplacements();

	for (auto& m : tree->methods)
	{
		m.second->repeats = 0;
		methods.push_back(m.second);
	}

	if (maxselect < 0)
	{
		maxselect = (int)round((double)maxselect / 100.0 * 
				(double)tree->methods.size());
	}
	else
	{
		maxselect = (maxselect > (int)tree->methods.size()) ? 
			tree->methods.size() : maxselect;
	}

	for (int i = 0; i < maxselect; i++)
	{
		int p = random(0, (int)methods.size()-1);
		int r = random(0, maxrepeat);

		auto e = methods.begin() + p;
		METHOD* m = *e;
		m->repeats = r;

		methods.erase(e);
	}

	MatchFinder matchFinder;
	TreeRepeater treeRepeater(lopt, tree, &replacements);

	DeclarationMatcher methodMatcher = functionDecl().bind("id");
	matchFinder.addMatcher(methodMatcher, &treeRepeater);

	assert_tool(tool.runAndSave(newFrontendActionFactory(&matchFinder).get()));

	return 0;
}

