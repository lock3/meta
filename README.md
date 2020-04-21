[![Actions Status](https://github.com/mgehre/llvm-project/workflows/main/badge.svg)](https://github.commgehre/llvm-project/actions)
# The -Wlifetime warnings implemented in clang
This project contains clang's experimental implementation
of the [lifetime safety profile of the C++ core guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#SS-lifetime).

You can try this implementation on [Compiler Explorer](https://godbolt.org/z/z-x3Jj)
by choosing the clang version `experimental -Wlifetime`.

## Warning flags
* `-Wdangling-gsl`: Statement-local analysis (enabled by default)
* `-Wreturn-stack-address`: Statement-local analysis (enabled by default)
* `-Wlifetime`: Main flow-sensitive analysis
* `-Wlifetime-filter`: Reduce the number of false-postives on unannotated code.
* `-Wno-lifetime-null`: Disable flow-sensitive warnings about nullness of Pointers 
* `-Wlifetime-global`: Flow-sensitive analysis that requires global variables of Pointer type to always point to variables with static lifetime
* `-Wlifetime-disabled`: Get warnings when the flow-sensitive analysis is disabled on a function due to forbidden constructs (`reinterpret_cast` or pointer arithmetic)

Suppress Warnings
* The `[[gsl::suppress("lifetime")]]` attribute can be used to suppress lifetime warnings for functions. See  [clang/test/Sema/attr-lifetime-suppress.cpp](clang/test/Sema/attr-lifetime-suppress.cpp) for an example.

## Build
You can either try it on [Compiler Explorer](https://godbolt.org/z/z-x3Jj), or build clang yourself:

    mkdir build
    cd build 
    cmake ../llvm -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD="X86" -DLLVM_ENABLE_PROJECTS=clang
    make
    # Run the tests (optional)
    make check-clang 
    # Use clang
    ./bin/clang++ <some-source> <warning-flags>
    
 ## Further information
 
[The specification](https://github.com/isocpp/CppCoreGuidelines/blob/master/docs/Lifetime.pdf)
 
[CppCon 2019 Talk, focusing on statement local analysis](https://www.youtube.com/watch?v=d67kfSnhbpA)
 
[EuroLLVM 2019 Talk, focusing on flow-sensitive analysis](https://www.youtube.com/watch?v=VynWyOIb6Bk)
 
[CppCon 2018 Talk, focusing on flow-sensitive analysis](https://www.youtube.com/watch?v=sjnp3P9x5jA)
 
[CppCon 2018 Keynote, by Herb Sutter](https://www.youtube.com/watch?v=80BZxujhY38&t=914s)
 
[CppCon 2015 Keynote, by Herb Sutter, first introduction](https://youtu.be/hEx5DNLWGgA?t=1471)

## Development
Tests:
* [clang/test/Sema/warn-lifetime-analysis-nocfg.cpp](clang/test/Sema/warn-lifetime-analysis-nocfg.cpp) Test statement-local analysis
* [clang/test/Sema/warn-lifetime-analysis.cpp](clang/test/Sema/warn-lifetime-analysis.cpp): Test all diagnostics emitted by `-Wlifetime`
* [clang/test/Sema/attr-psets.cpp](clang/test/Sema/attr-psets.cpp): The main tests of the flow-sensitive analysis. They verify how psets are propagated through all types of C++ expressions.
* [clang/test/Sema/warn-lifetime-analysis-nonull.cpp](clang/test/Sema/warn-lifetime-analysis-nonull.cpp): Test `-Wno-lifetime-null`
* [clang/test/Sema/attr-psets-annotation.cpp](clang/test/Sema/attr-psets-annotation.cpp): Test for the function attributes to give pre- and post-conditions to the psets of parameters and the return value
* [clang/test/Sema/warn-lifetime-analysis-nocfg-disabled.cpp](clang/test/Sema/warn-lifetime-analysis-nocfg-disabled.cpp) Test disabling the statement-local analysis (because it's enabled by default)
* [lifetime-integration-tests/](lifetime-integration-tests/): Integration tests that verify the analysis against various standard libraries and versions

Implementation:
* Entry point into flow-sensitive analysis, checking the warning flag and calling `runAnalysis`: https://github.com/mgehre/llvm-project/blob/lifetime/clang/lib/Sema/AnalysisBasedWarnings.cpp#L2277
* Implementation of `runAnalysis` https://github.com/mgehre/llvm-project/blob/lifetime/clang/lib/Analysis/Lifetime.cpp#L294, delegating to `LifetimeContext`. 
* Builds a CFG (Control flow graph) of the AST (Abstract syntax tree) nodes and then iterates over the basic blocks until the pset converge. Calls `VisitBlock` https://github.com/mgehre/llvm-project/blob/lifetime/clang/lib/Analysis/Lifetime.cpp#L253 to compute the psets of a BasicBlock. 
* `VisitBlock` https://github.com/mgehre/llvm-project/blob/lifetime/clang/lib/Analysis/LifetimePsetBuilder.cpp#L1255 uses a `PSetsBuilder` to visit each (sub-)expression in basic block. 
* This dispatches to the various `Visit*` member functions, e.g. https://github.com/mgehre/llvm-project/blob/lifetime/clang/lib/Analysis/LifetimePsetBuilder.cpp#L1719, to compute the pset of the current expression (if it has one - i.e. it's an lvalue or an prvalue of Pointer type) based on the psets of its subexpressions (which have been computed earlier).
