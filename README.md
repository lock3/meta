# The -Wlifetime warnings implemented in clang
This project contains clang's experimental implementation
of the [lifetime safety profile of the C++ core guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#SS-lifetime).

You can try this implementation on [Compiler Explorer](https://godbolt.org/z/z-x3Jj)
by choosing the clang version `experimental -Wlifetime`.

## Warning flags
* `-Wdangling-gsl`: Statement-local analysis (enabled by default)
* `-Wreturn-stack-address`: Statement-local analysis (enabled by default)
* `-Wlifetime`: Main flow-sensitive analysis
* `-Wno-lifetime-null`: Disable flow-sensitive warnings about nullness of Pointers 
* `-Wlifetime-global`: Flow-sensitive analysis that requires global variables of Pointer type to always point to variables with static lifetime
* `-Wlifetime-disabled`: Get warnings when the flow-sensitive analysis is disabled on a function due to forbidden constructs (`reinterpret_cast` or pointer arithmetic)

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
