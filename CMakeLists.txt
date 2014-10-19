set(LLVM_LINK_COMPONENTS support)
set(LLVM_USED_LIBS clangTooling clangBasic clangAST)

add_clang_executable(crowbar
	tsp2.cpp
	Crowbar.cpp
	CallTree.cpp
	Repeater.cpp
	Redirector.cpp
	KNRConverter.cpp
	)

target_link_libraries(crowbar
   	clangTooling
	clangBasic
	clangASTMatchers
	)
