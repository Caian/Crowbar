/*--------------------------------------------------------------------------*/
/*                                                                          */
/* Crowbar Code Refactoring Tool                                            */
/* author: Caian Benedicto                                                  */
/* contact: caianbene@gmail.com (with a [Crowbar] tag in the subject)       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

#pragma once

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
#include <vector>
#include <unordered_map>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;
using namespace std;

typedef int64_t int64;

struct FILERANGE
{
	int64 begin, end;
};

struct CALLSITE
{
	FILERANGE location;
	CharSourceRange range;
	SourceManager* sm;

	int redirect;
};

struct METHOD
{
	string pre;
	string name;
	string post;
	
	FILERANGE location;
	CharSourceRange range;
	SourceManager* sm;

	int repeats;
	
	vector<CALLSITE*> calls;
};

struct CALLTREE
{
	unordered_map<string,METHOD*> methods;
};

int BuildCallTreeMethods(ClangTool& tool, const LangOptions* lopt, CALLTREE** ppTree);
int BuildCallTreeCalls(ClangTool& tool, const LangOptions* lopt, CALLTREE* ppTree);
void DestroyCallTree(CALLTREE** ppTree);
