/*--------------------------------------------------------------------------*/
/*                                                                          */
/* Crowbar Code Refactoring Tool                                            */
/* author: Caian Benedicto                                                  */
/* contact: caianbene@gmail.com (with a [Crowbar] tag in the subject)       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Lex/Lexer.h"
#include "clang/Basic/SourceManager.h"
#include "clang/AST/ASTContext.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"

#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>

#include <stdlib.h>
#include <time.h>

#include <unordered_set>
#include <vector>

#include "Crowbar.h"
#include "CallTree.h"
#include "Repeater.h"
#include "Redirector.h"
#include "KNRConverter.h"

using namespace std;
using namespace llvm;
using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

typedef int64_t int64;

/*--------------------------------------------------------------------------*/
/* Help Setup                                                               */
/*--------------------------------------------------------------------------*/

enum RedirectMode
{
	RM_Total,
	RM_PerMethod,
};

static cl::OptionCategory CrowbarCat("Crowbar", "Code Refactoring Tool");

static cl::opt<bool> ListOpt ("list", 
		cl::desc("List all methods with a body and their position"),
		cl::cat(CrowbarCat));

static cl::opt<bool> CallsOpt ("calls", 
		cl::desc("List all methods with a body and their call sites"),
		cl::cat(CrowbarCat));

static cl::opt<bool> KNROpt("knr", 
		cl::desc("Enable K&R header fix for methods"),
		cl::init(false), cl::value_desc("pattern"),
		cl::cat(CrowbarCat));

static cl::opt<string> SelectOpt("select", 
		cl::desc("Regex pattern to select methods"),
		cl::init("*"), cl::value_desc("pattern"),
		cl::cat(CrowbarCat));

static cl::opt<string> MaxSelectOpt("max-select", 
		cl::desc("Maximum number of methods to be repeated (absolute or %)"),
		cl::init("100%"), cl::cat(CrowbarCat));

static cl::opt<int> MaxRepeatOpt("max-repeat", 
		cl::desc("Maximum number of repetitions for selected methods"),
		cl::init(5), cl::cat(CrowbarCat));

static cl::opt<int> SRSeedOpt("srseed", 
		cl::desc("Seed used to select method repetitions"),
		cl::init(4), cl::cat(CrowbarCat)); /*Chosen by a fair dice roll*/

/*static cl::opt<RedirectMode> RedirectModeOpt("redirect-mode", 
		cl::desc("Select redirection mode:"),
		cl::values(
			clEnumValN(RM_Total, "total", "max-redirect applied over all methods"),
            clEnumValN(RM_PerMethod, "per-method", "max-redirect applied for each method individually"),
            clEnumValEnd),
		cl::init(RM_PerMethod), cl::cat(CrowbarCat));*/

static cl::opt<string> MaxRedirectOpt("max-redirect", 
		cl::desc("Maximum number of calls per method to be redirected (absolute or %)"),
		cl::init("50%"), cl::cat(CrowbarCat));

static cl::opt<int> RESeedOpt("reseed", 
		cl::desc("Seed used to select call redirections"),
		cl::init(4), cl::cat(CrowbarCat)); /*Chosen by a fair dice roll*/

static cl::opt<bool> GenOpt("gen", 
		cl::desc("Gentlemen"),
		cl::cat(CrowbarCat));

/*--------------------------------------------------------------------------*/
/* Print a method and its position in the source file                       */
/*--------------------------------------------------------------------------*/
void printMethod(const METHOD* m)
{
	cout << m->name << ',' << m->location.begin 
		<< '-' << m->location.end << endl;
}


/*--------------------------------------------------------------------------*/
/* Print the positions of all method's calls                                */
/*--------------------------------------------------------------------------*/
void printCalls(const METHOD* m)
{
	cout << m->name;

	for (const auto& call : m->calls)
		cout << ',' << call->location.begin << '-' << call->location.end;

	cout << endl;
}


/*--------------------------------------------------------------------------*/
/* Parse string arguments representing numbers or percentages               */
/*--------------------------------------------------------------------------*/
bool tryParseStringArg(string arg, int* v, bool* p)
{
	const char* pstart = arg.c_str();
	const char* pend = pstart + arg.size();

	if(*(arg.rbegin()) == '%')
	{
		*p = true;
		pend--;
	}
	else
	{
		*p = false;
	}

	char* e;
	*v = (int)strtol(pstart, &e, 10);

	return e == pend;
}


/*--------------------------------------------------------------------------*/
/* main                                                                     */
/*--------------------------------------------------------------------------*/

int main(int argc, const char **argv)
{    
	GenOpt.setHiddenFlag(cl::ReallyHidden);

	CommonOptionsParser optionsParser(argc, argv, CrowbarCat, 0);
	LangOptions lopt;

	optbase_0();

	vector<string> sources = optionsParser.getSourcePathList();
	CompilationDatabase& compilations = optionsParser.getCompilations();

	// Parse the arguments

	int maxselect;
	bool maxselectpc;

	if (!tryParseStringArg(MaxSelectOpt, &maxselect, &maxselectpc) ||
			(maxselectpc && maxselect > 100) || (maxselect < 0))
	{
		error("max-select is not in a raw number or percentage form");
		return 1;
	}

	if (maxselectpc)
		maxselect *= -1;

	int maxredirect;
	bool maxredirectpc;

	if (!tryParseStringArg(MaxRedirectOpt, &maxredirect, &maxredirectpc) || 
			(maxredirectpc && maxredirect > 100) || (maxredirect < 0))
	{
		error("max-redirect is not in a raw number or percentage form");
		return 2;
	}

	if (maxredirectpc)
		maxredirect *= -1;

	int maxrepeat = MaxRepeatOpt;

	if (maxrepeat < 0)
	{
		error("max-repeat is not in a valid number form");
		return 3;
	}

	int srseed = SRSeedOpt;
	int reseed = RESeedOpt;
	string pattern = SelectOpt;

	// Dump the options for the record
	
	cout << "!options," 
		 << (maxselect > 0 ? maxselect : -maxselect) 
		 << (maxselect > 0 ? "" : "%") << "," 
		 << (maxredirect > 0 ? maxredirect : -maxredirect) 
		 << (maxredirect > 0 ? "" : "%") << ","
		 << maxrepeat << "," 
		 << srseed << ","
		 << reseed << ","
		 << pattern << endl;

	// K&R Fix
	
	if (KNROpt)
	{
		RefactoringTool tool(compilations, sources);
		assert_phase(FixKNRNotation(tool, &lopt));
	}
	
	if (maxrepeat > 0 && maxselect > 0)
	{
		// Create the tree with the method list
	
		RefactoringTool tool(compilations, sources);

		CALLTREE* pTree = NULL;
		assert_phase(BuildCallTreeMethods(tool, &lopt, &pTree));

		// Repeat the methods

		assert_phase(RepeatCallTree(tool, &lopt, pTree, srseed, maxselect, maxrepeat));

		if (maxredirect > 0)
		{
			// Fill the tree with the updated call list

			RefactoringTool tool2(compilations, sources);
			assert_phase(BuildCallTreeCalls(tool2, &lopt, pTree));

			// Now redirect the calls
			
			assert_phase(RedirectCallTree(tool2, &lopt, pTree, reseed, maxredirect));
		}
	}

	// List methods
	
	/*if (ListOpt)
	{
		for (const auto& m : pTree->methods)
			printMethod(m.second);
	}

	// List calls

	if (CallsOpt)
	{
		for (const auto& m : pTree->methods)
		{
			if (m.second->calls.size() > 0)
				printCalls(m.second);
		}
	}*/

	// Check if everything is working

	ClangTool tool3(compilations, sources);
	MatchFinder matchFinder;
	assert_tool(tool3.run(newFrontendActionFactory(&matchFinder).get()));

	return 0;
}

