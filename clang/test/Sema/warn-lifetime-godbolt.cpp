// RUN: %clang -std=c++1z -fsyntax-only -Xclang -verify -Wlifetime -isystem %S/../../range-v3/include -stdlib=libstdc++ %s
// RUN: %clang -std=c++1z -fsyntax-only -Xclang -verify -Wlifetime -isystem %S/../../range-v3/include -stdlib=libc++ -I/usr/lib/llvm-7/include/c++/v1 %s
// Note: paths are for Ubuntu's libc++-7-dev package from http://apt.llvm.org/
// Examples are from https://herbsutter.com/2018/09/20/lifetime-profile-v1-0-posted/

#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <string_view>
#include <range/v3/view/filter.hpp>

using namespace std;
using namespace ranges;

// https://godbolt.org/z/szJjnH
void example_1_1_1() {
  int* p = nullptr;		// pset(p) = {null} – records that now p is null
  {
    int x = 0;
    p = &x;	 		    // A: set pset(p) = {x} – records that now p points to x
    cout << *p;			// B: ok – *p is ok because {x} is alive
  }					    // expected-note {{pointee 'x' left the scope here}}
                        // C: x destroyed => replace “x” with “invalid” in all psets
					    //                => set pset(p) = {invalid}
  cout << *p;			// D: error – because pset(p) contains {invalid} 
  // expected-warning@-1 {{dereferencing a dangling pointer}}
}

// https://godbolt.org/z/_midIP
extern bool cond;
void example_2_4_9_3() {
    int a[10], b[10];
    int i = 0;
    int* p = &a[0];	    			// pset(p) = {a}
    for( ; cond ; ) {
        *p = 42; // expected-warning {{dereferencing a possibly null pointer}}
        p = nullptr;				// A: pset(p) = {null} expected-note {{assigned here}}
        // ...
        if(cond) {
            // ...
            p = &b[i];  			// pset(p) = {b}
            // ...
        }
        // merge => pset(p) = {null,b} for second iteration
        // ...
    }
}

// https://godbolt.org/z/dymV_C
void example_2_6_2_1() {
    std::string_view s = "foo"s;		// A
    // expected-note@-1 {{temporary was destroyed at the end of the full expression}}
    s[0]; // expected-warning {{passing a dangling pointer as argument}}
}

// https://godbolt.org/z/eqCRLx
void example_1_1_3_c() {
  vector<int> ints{0, 1, 2, 3, 4, 5};

  auto even = [](int i) { return !(i % 2); };

  auto view = ints | view::filter(even);

  cout << *ranges::begin(view); // ok

  ints.push_back(6);            // A: invalidates view expected-note {{modified here}}

  cout << *ranges::begin(view); // expected-warning {{passing a dangling pointer as argument}}
}

// https://godbolt.org/z/iq5Qja
void example_1_1_3_b() {
  vector<bool> vb{false,true,false,true};
  auto proxy = vb[0];
  if (proxy) {/*...*/}	// ok

  vb.reserve(100);	    // A: invalidates proxy expected-note {{modified here}}

  if (proxy) {/*...*/}	// expected-warning {{passing a dangling pointer as argument}}
}

// https://godbolt.org/z/UE-Mb0
void example_2_4_6_2() {
    auto s = make_shared<int>(0);
    int* pi3 = s.get();			// pset(pi3) = {s'} [more on this later]
    s = make_shared<int>(1);	// A: KILL(s') => pset(pi3) = {invalid} expected-note {{modified here}}
    *pi3 = 42;	    			// expected-warning {{dereferencing a dangling pointer}}
    // Chris Hawblitzel’s example
    auto  sv  = make_shared<vector<int>>(100);
    shared_ptr<vector<int>>* sv2 = &sv; // pset(sv2) = {sv}
    vector<int>* vec = &*sv;	// pset(vec) = {sv'}
    int* ptr = &(*sv)[5];		// pset(ptr) = {sv''}

    *ptr = 1;					// ok
        						// track pset of:    sv2     vec     ptr
						        //                   -----   -----   -----
						        //             IN:   sv      sv'     sv''

    vec->	    				// same as “(*vec).” => pset(*vec) == {sv''} expected-note 2 {{modified here}}
         push_back(1);			// KILL(sv'') because non-const operation
		        				//            OUT:   sv      sv'     invalid

    *ptr = 3;					// expected-warning {{dereferencing a dangling pointer}}

    ptr = &(*sv)[5];            // back to previous state to demonstrate an alternative...

    *ptr = 4;					// ok
	        					//             IN:   sv      sv'     sv''
    (*sv2).		    			// pset(*sv2) == {sv'} expected-note {{modified here}}
       reset();		    		// KILL(sv') because non-const operation
						        //            OUT:   sv      invalid invalid
    vec->push_back(1);			// expected-warning {{passing a dangling pointer as argument}}
    *ptr = 3;					// expected-warning {{dereferencing a dangling pointer}}
}

