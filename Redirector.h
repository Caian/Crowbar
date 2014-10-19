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
#include <iostream>
#include <unordered_map>
#include <stdexcept>
#include <sstream>
#include <stdlib.h>

#include "Crowbar.h"
#include "CallTree.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;
using namespace std;

int RedirectCallTree(RefactoringTool& tool, const LangOptions* lopt, 
		const CALLTREE* tree, int seed, int maxredirect);
