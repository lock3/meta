This is the home of the Clang-based implementation of Herb Sutter’s Lifetime
safety profile for the C++ Core Guidelines, available online
at cppx.godbolt.org.

TL;DR:
  git clone https://git.llvm.org/git/llvm 
  cd llvm
  git checkout -b lifetime 75521639
  cd tools
  git clone https://github.com/mgehre/clang.git
  cd ..
  mkdir build
  cd build
  cmake .. -G Ninja
  ninja clang
  ./bin/clang -Wlifetime ....

Compile with LLVM commit 75521639735cfda147aadecacb91968d8f92e3dd.

Also checkout master (known to work: c50d5b32) of
https://github.com/ericniebler/range-v3.git into a range-v3 subfolder of your
clang checkout to run the tests.

//===----------------------------------------------------------------------===//
// C Language Family Front-end
//===----------------------------------------------------------------------===//

Welcome to Clang.  This is a compiler front-end for the C family of languages
(C, C++, Objective-C, and Objective-C++) which is built as part of the LLVM
compiler infrastructure project.

Unlike many other compiler frontends, Clang is useful for a number of things
beyond just compiling code: we intend for Clang to be host to a number of
different source-level tools.  One example of this is the Clang Static Analyzer.

If you're interested in more (including how to build Clang) it is best to read
the relevant web sites.  Here are some pointers:

Information on Clang:             http://clang.llvm.org/
Building and using Clang:         http://clang.llvm.org/get_started.html
Clang Static Analyzer:            http://clang-analyzer.llvm.org/
Information on the LLVM project:  http://llvm.org/

If you have questions or comments about Clang, a great place to discuss them is
on the Clang development mailing list:
  http://lists.llvm.org/mailman/listinfo/cfe-dev

If you find a bug in Clang, please file it in the LLVM bug tracker:
  http://llvm.org/bugs/

