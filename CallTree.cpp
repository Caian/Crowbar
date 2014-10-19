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
		const LangOptions& lopt)
{
	const SourceManager& sm = l.getManager();

	// I need to know the starting point of the file
	FileID f = l.getFileID();
	SourceLocation fl = sm.getLocForStartOfFile(f);

	SourceRange lr = SourceRange(l);
	SourceLocation b = SourceLocation(lr.getBegin());
	SourceLocation _e = SourceLocation(lr.getEnd());
	FullSourceLoc e(clang::Lexer::getLocForEndOfToken(_e, 0, sm, lopt), sm);

	const char* fstart = sm.getCharacterData(fl);
	const char* lbegin = sm.getCharacterData(b);
	const char* lend = sm.getCharacterData(e);

	*pBegin = (int64)(lbegin-fstart);
	*pEnd = (int64)(lend-fstart);
}


/*--------------------------------------------------------------------------*/
/* Convert a location to the absolute position inside the source file       */
/*--------------------------------------------------------------------------*/
void getAbsoluteLocation(const FullSourceLoc& lb, const FullSourceLoc& le, 
		int64* pBegin, int64* pEnd, const LangOptions& lopt)
{
	const SourceManager& sm = lb.getManager();

	// I need to know the starting point of the file
	FileID f = lb.getFileID();
	SourceLocation fl = sm.getLocForStartOfFile(f);
	
	FullSourceLoc e(clang::Lexer::getLocForEndOfToken(le, 0, sm, lopt), sm);

	const char* fstart = sm.getCharacterData(fl);
	const char* lbegin = sm.getCharacterData(lb);
	const char* lend = sm.getCharacterData(e);

	*pBegin = (int64)(lbegin-fstart);
	*pEnd = (int64)(lend-fstart);
}


/*--------------------------------------------------------------------------*/
/* Matcher for the methods and calls                                        */
/*--------------------------------------------------------------------------*/
class TreeFinder : public MatchFinder::MatchCallback
{
private:

	const LangOptions* lopt;
	unordered_map<string, METHOD*> methods;

	int runFD(const FunctionDecl *md, SourceManager &sm)
	{
		if (!md->hasBody())
			return 0;

		if (!md->isThisDeclarationADefinition())
			return 0;

		DeclarationNameInfo info = md->getNameInfo();
		
		FullSourceLoc b(md->getLocStart(), sm), _e(md->getLocEnd(), sm);
		FullSourceLoc e(clang::Lexer::getLocForEndOfToken(_e, 0, sm, *this->lopt), sm);
		
		SourceRange nameRange = SourceRange(info.getLoc());
		FullSourceLoc d(nameRange.getBegin(), sm), _f(nameRange.getEnd(), sm);
		FullSourceLoc f(clang::Lexer::getLocForEndOfToken(_f, 0, sm, *this->lopt), sm);

		string start(sm.getCharacterData(b), sm.getCharacterData(d)-sm.getCharacterData(b));
		string name(sm.getCharacterData(d), sm.getCharacterData(f)-sm.getCharacterData(d));
		string end(sm.getCharacterData(f), sm.getCharacterData(e)-sm.getCharacterData(f));

		if (this->methods.find(name) != this->methods.end())
		{
			message("Redefinition of method " + name);
			return 1;
		}

		METHOD* m = new METHOD();
		m->pre = start;
		m->name = name;
		m->post = end;

		getAbsoluteLocation(FullSourceLoc(md->getLocStart(), sm), 
				FullSourceLoc(md->getLocEnd(), sm), 
				&m->location.begin, &m->location.end, *this->lopt);

		m->sm = &sm;
		m->range = CharSourceRange::
			getTokenRange(SourceRange(info.getLoc()));

		this->methods[name] = m;

		// For debugging purposes
		// cout << start << "|" << name << "|" << end << endl;

		return 0;
	}

	int runCE(const CallExpr *md, SourceManager &sm)
	{
		const FunctionDecl* dcallee = md->getDirectCallee();

		// Only care about callees that can be determined at 
		// compile time (no black magic with function pointers)
		if (dcallee == 0)
			return 0;

		DeclarationNameInfo info = dcallee->getNameInfo();
		std::string name = info.getAsString();

		// Maybe it's calling external code, if it has no 
		// local body then the call is irrelevant
		auto method = this->methods.find(name);
		if (method == this->methods.end())
			return 0;

		const Decl* callee = md->getCalleeDecl();

		CALLSITE* s = new CALLSITE();
		
		getAbsoluteLocation(FullSourceLoc(md->getLocStart(), sm), 
				FullSourceLoc(md->getLocEnd(), sm),
				&s->location.begin, &s->location.end, *this->lopt);

		s->sm = &sm;
		s->range = CharSourceRange::
			getTokenRange(SourceRange(callee->getLocation()));

		method->second->calls.push_back(s);

		// For debugging purposes
		// cout << name << endl;

		return 0;
	}

public:

	TreeFinder(const LangOptions* lopt) : lopt(lopt)
	{

	}

	const unordered_map<string, METHOD*>& getMethods()
	{
		return this->methods;
	}

	void setMethods(unordered_map<string, METHOD*>& methods)
	{
		this->methods = methods;
	}

	virtual void run(const MatchFinder::MatchResult &Result) 
	{
		SourceManager &sm = Result.Context->getSourceManager();
		if (const FunctionDecl *md = Result.Nodes.getNodeAs<clang::FunctionDecl>("id"))
			this->runFD(md, sm);
		else if (const CallExpr *md = Result.Nodes.getNodeAs<clang::CallExpr>("id"))
			this->runCE(md, sm);
	}
};

int BuildCallTreeMethods(ClangTool& tool, const LangOptions* lopt, CALLTREE** ppTree)
{
	MatchFinder matchFinder;
	TreeFinder treeFinder(lopt);

	DeclarationMatcher methodMatcher = functionDecl().bind("id");
	matchFinder.addMatcher(methodMatcher, &treeFinder);

	assert_tool(tool.run(newFrontendActionFactory(&matchFinder).get()));

	// Dump everything to ppTree
	
	*ppTree = new CALLTREE();
	(*ppTree)->methods = treeFinder.getMethods();

	return 0;
}

int BuildCallTreeCalls(ClangTool& tool, const LangOptions* lopt, CALLTREE* ppTree)
{
	MatchFinder matchFinder;
	TreeFinder treeFinder(lopt);

	// Use the existing tree	
	treeFinder.setMethods(ppTree->methods);

	StatementMatcher callMatcher = callExpr().bind("id");
	matchFinder.addMatcher(callMatcher, &treeFinder);

	assert_tool(tool.run(newFrontendActionFactory(&matchFinder).get()));
	
	return 0;
}

void DestroyCallTree(CALLTREE** ppTree)
{
	for(auto& method : (*ppTree)->methods)
	{
		for(auto& call : method.second->calls)
			delete call;

		delete method.second;
	}

	delete *ppTree;
	*ppTree = NULL;
}

