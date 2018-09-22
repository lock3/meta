// RUN: %clang -std=c++1z -fsyntax-only -Xclang -verify -Wlifetime -isystem %S/../../range-v3/include -Wno-return-stack-address -stdlib=libstdc++ %s
// RUN: %clang -std=c++1z -fsyntax-only -Xclang -verify -Wlifetime -isystem %S/../../range-v3/include -Wno-return-stack-address -stdlib=libc++ -I/usr/lib/llvm-7/include/c++/v1 %s
// Note: paths are for Ubuntu's libc++-7-dev package from http://apt.llvm.org/

// This is intedend to contain all examples that are linked from the paper.

#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <string_view>
#include <range/v3/view/filter.hpp>

using namespace std;
using namespace ranges;
extern bool cond;

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

// https://godbolt.org/z/ytRTKt
void example_1_1_2() {
  string_view s;		// pset(s) = {null}
  {
    char a[100];
    s = a;	 		    // A: pset(s) = {a}
    cout << s[0];		// B: ok
  }					    // C: x destroyed => set pset(s) = {invalid} expected-note {{pointee 'a' left the scope here}}
  cout << s[0];			// D: error: ‘s[0]’ is illegal, s was invalidated when ‘a’ expected-warning {{passing a dangling pointer as argument}}
					    //     went out of scope on line C (path: A,C,D)
}

// https://godbolt.org/z/HS_v-D
void example_1_1_3() {
    using X = int;
    auto up = make_unique<X>(42);	// up is an Owner
    X* p = up.get();	// A: pset(p) = {up'}, or “something owned by up”
    cout << *p;			// B: ok: up is still keeping *p alive, p is valid
    up.reset();			// C: up changed => replace “up'” with “invalid” in all psets expected-note {{modified here}}
    cout << *p;			// D: error: ‘p’ pointed to data owned by ‘up’ and was expected-warning {{dereferencing a dangling pointer}}
    					//    invalidated by ‘up.reset’ on line C (path: A,C,D)
}

// https://godbolt.org/z/qTle8G
void example_1_1_3_a() {
  string s = "abcdefghijklmnopqrstuvwxyz";

  string_view sv = s;

  if (sv[0] == 'a') {
  } // ok

  s = "xyzzy"; // A: invalidates sv expected-note {{modified here}}

  if (sv[0] == 'a') { // expected-warning {{passing a dangling pointer as argument}}
  } // ERROR, sv was invalidated
}

// https://godbolt.org/z/iq5Qja
void example_1_1_3_b() {
  vector<bool> vb{false,true,false,true};
  auto proxy = vb[0];
  if (proxy) {/*...*/}	// ok

  vb.reserve(100);	    // A: invalidates proxy expected-note {{modified here}}

  if (proxy) {/*...*/}	// ERROR, proxy was invalidated expected-warning {{passing a dangling pointer as argument}}
                     	// by ‘vb.reserve’ (line A)
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

// https://godbolt.org/z/1iOvAO
string_view foo(const string& s1, const string& s2);
void example_1_1_4() {
    string_view sv;
    // ...
    sv = foo("tmp1", "tmp2");	// C: in: pset(arg1) = {__tmp1'}, pset(arg2) = {__tmp2'}
					            // ... so assume: pset(sv) = {__tmp1', __tmp2'}  [the union]
            					// ... and then at the end of this statement, __tmp1 and
            					//     __tmp2 are destroyed, so pset(sv) = {invalid}
    // expected-note@-4 {{temporary was destroyed at the end of the full expression}}
    cout << sv[0];   			// D: error: ‘s[0]’ is illegal, s was invalidated when expected-warning {{passing a dangling pointer as argument}}
	            				//   ‘__tmp1’ went out of scope on line C (path: C,D)
}

// https://godbolt.org/z/g9C8G6
void example_2_4_2_1(string param) {
    int* p;				// A: pset(p) = {invalid} expected-note {{it was never initialized here}}
    *p = 1;				// ERROR (lifetime.1), p is invalid (line A) expected-warning {{dereferencing a dangling pointer}}
    int** pp;			// B: pset(pp) = {invalid} expected-note {{it was never initialized here}}
    cout << *pp;		// ERROR (lifetime.1), pp is invalid (line B) expected-warning {{dereferencing a dangling pointer}}
    if (cond) {
        // ...
        int x;
        // ...
    }					// KILL(x) — invalidates any Pointer to x
    while (cond) {
        // ...
        string s;
        // ...
        s = param;		// KILL(s') — invalidates any Pointer to s’s data
						//   (the non-const operation could move s’s buffer)
        // ...
    }					// KILL(s) — invalidates any Pointer to s
    // ...
}						// KILL(param) — invalidates any Pointer to param

// https://godbolt.org/z/G3BCOR
void example_2_4_5_1() {
    int* p = nullptr;	// A: pset(p) = pset(nullptr) which is {null} expected-note {{assigned here}}
    *p = 1;		    	// ERROR (lifetime.1), p is null (line A) expected-warning {{dereferencing a null pointer}}
    int* p2;			// pset(p2) = {invalid}
    {
        struct { int i; } s = {0};
        p = &s.i;		// pset(p) = pset(temp) = {s.i}, p now points to s.i
        p2 = p; 		// pset(p2) = {s.i}, p2 now also points to s.i
        *p = 1; 		// ok
        *p2 = 1;		// ok
    } 					// B: KILL(i)  expected-note 2 {{pointee 's' left the scope here}}
						//   => pset(p) = {invalid} and pset(p2) = {invalid}
    *p = 1; 			// ERROR (lifetime.1), p was invalidated when s expected-warning {{dereferencing a dangling pointer}}
						//   went out of scope (line B)
    *p2 = 1; 			// ERROR (lifetime.1), p2 was invalidated when s expected-warning {{dereferencing a dangling pointer}}
						//   went out of scope (line B)
    p = nullptr;		// C: pset(p) = pset(nullptr) which is {null} expected-note {{assigned here}}
    int x[100];
    p2 = &x[10];		// pset(p2) = {x} — p2 now points to x
    *p2 = 1; 			// ok
    p2 = p;				// D: pset(p2) = pset(p) which is {null} expected-note {{assigned here}}
    *p2 = 1;			// ERROR (lifetime.1), p2 is null (lines CD) expected-warning {{dereferencing a null pointer}}
    p2 = &x[10];		// pset(p2) = {x} — p2 now points to x again
    *p2 = 1; 			// ok
    int** pp = &p2;		// pset(pp) = {p}
    *pp = p; 			// ok
}

// https://godbolt.org/z/x26Fsg
void consume(vector<int>&&);
void example_2_4_5_3() {
    vector<int> v(1000);
    auto iter = v.begin();		// pset(iter) = {v'}
    consume(move(v));   		// A: pset(iter) = {invalid}, pset(v) = {invalid} expected-note {{modified here}}
    *iter;		    	    	// error, iter was invalidated on line A expected-warning {{passing a dangling pointer as argument}}
    v[100];			        	// error, v is moved-from and [] has a precondition
}

// https://godbolt.org/z/KzfY4B
void example_2_4_6_1() {
  int* p1 = nullptr;
  int* p2 = nullptr;
  int* p3 = nullptr;
  int* p4 = nullptr;
  {
      int   i = 0;
      int&  ri = i;			// pset(ri) = {i}
      p1 = &ri;				// pset(p1) = {i}
      *p1 = 1;				// ok
      int*  pi = &i;			// pset(pi) = {i}
      p2 = pi;				// pset(p2) = {i}
      *p2 = 1;				// ok
      int** ppi = &pi;		// pset(ppi) = {pi}
      **ppi = 1;				// ok, modifies i
      int* pi2 = *ppi;		// pset(pi2) = pset(pset(ppi)) which is {i}
      p3 = pi2;				// pset(p3) = {i}
      *p3 = 1;				// ok
      {
          int   j = 0;
          pi = &j;			// pset(pi) = {j}  (note: so *ppi now points to j)
          pi2 = *ppi;			// pset(pi2) = pset(pset(ppi)) but which is now {j}
          **ppi = 1;			// ok, modifies j
          *pi2 = 1;			// ok
          p4 = pi2;			// pset(p4) = {j}
          *p4 = 1;			// ok
      } 			    		// A: KILL(j) => pset(pi) = {invalid}, expected-note 3 {{pointee 'j' left the scope here}}
                  //   pset(pi2) = {invalid}, and pset(p4) = {invalid}
      **ppi = 1;				// ERROR (lifetime.1), *ppi was invalidated when j expected-warning {{dereferencing a dangling pointer}}
                  //   went out of scope (line A)
      *pi2 = 1;				// ERROR (lifetime.1), pi2 was invalidated when j expected-warning {{dereferencing a dangling pointer}}
                  //   went out of scope (line A)
      *p4 = 1;				// ERROR (lifetime.1), p4 was invalidated when j expected-warning {{dereferencing a dangling pointer}}
                  //   went out of scope (line A)
      *p3 = 1;				// ok
      *p2 = 1;				// ok
      *p1 = 1;				// ok
  } 						    // B: KILL(i) => pset(p3) = {invalid}, expected-note 3 {{pointee 'i' left the scope here}}
                  //   pset(p2) = {invalid}, and pset(p1) = {invalid}
  *p3 = 1;					// ERROR (lifetime.1), p3 was invalidated when i expected-warning {{dereferencing a dangling pointer}}
                  //   went out of scope (line B)
  *p2 = 1;					// ERROR (lifetime.1), p2 was invalidated when i expected-warning {{dereferencing a dangling pointer}}
                  //   went out of scope (line B)
  *p1 = 1;					// ERROR (lifetime.1), p1 was invalidated when i expected-warning {{dereferencing a dangling pointer}}
                  //   went out of scope (line B)
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

// https://godbolt.org/z/cAEqzJ
void example_2_4_6_3() {
    vector<vector<int>> vv;
    vector<vector<int>>* vv2 = &vv;	// pset(vv2) = vv
    vector<int>* vec = &vv[0];		// pset(vec) = vv'
    int* ptr = &(*vec)[5];			// pset(ptr) = vv''

    *ptr = 0;   					// ok

				            		// track pset of:    vv2     vec     ptr
						            //                   -----   -----   -----
						            //             IN:   vv      vv'     vv''
    vec->       					// same as “(*vec).” => pset(*vec) == {vv''} expected-note {{modified here}}
         push_back(1);  			// KILL(vv'') because non-const operation
            						//            OUT:   vv      vv'     invalid

    *ptr = 1;					    // ERROR, invalidated by push_back expected-warning {{dereferencing a dangling pointer}}

    ptr = &(vv[0])[5];              // back to previous state to demonstrate an alternative...

    *ptr = 0;					    // ok

	            					//             IN:   vv      vv'     vv''
    vv2->	        				// same as “(*vv2).” => pset(*vv2) == {vv'} expected-note {{modified here}}
         clear();	    			// KILL(vv') because non-const operation
						            //            OUT:   vv      invalid invalid

    *ptr = 2;					    // ERROR, invalidated by clear expected-warning {{dereferencing a dangling pointer}}
}

// https://godbolt.org/z/RjQogH
void example_2_4_7_1() {
    int* p = nullptr; 		// pset(p) = {null}
    if(cond) {
        int i = 0;
        p = &i; 			// pset(p) = {i}
        *p = 42; 			// ok
    } 					    // A: KILL(i) => pset(p) = {invalid}
    else {
        int j = 1;
        p = &j; 			// pset(p) = j
        *p = 42; 			// ok
    } 	    				// B: KILL(j) => pset(p) = {invalid} expected-note {{pointee 'j' left the scope here}}
    // merge => pset(p) = {invalid}

    *p = 42;					// ERROR, p was invalidated when i went out of scope expected-warning {{dereferencing a dangling pointer}}
			    		// at line A or j went out of scope at line B.
				    	// Solution: increase i’s and j’s lifetimes, or
					    // reduce p’s lifetime
}

// https://godbolt.org/z/WqK1wT
void example_2_4_7_2() {
    int* p = nullptr; 		// pset(p) = {null}
    int i = 0;
    if(cond) {
        p = &i; 			// pset(p) = {i}
        *p = 42; 	   		// ok
    } 			    		// no invalidation
    else {
        int j = 1;
        p = &j; 			// pset(p) = {j}
        *p = 42; 			// ok
    } 					    // A: KILL(j) => pset(p) = {invalid} expected-note 2 {{pointee 'j' left the scope here}}
    // merge => pset(p) = {invalid}

    *p = 1; 				// ERROR, p was invalidated when j went out of scope expected-warning {{dereferencing a possibly dangling pointer}}
	    		    		// at line A. Solution: increase j’s lifetime, or
		        			// reduce p’s lifetime
    if(cond) *p = 2;		// ERROR, (same diagnostic) even if cond is unchanged expected-warning {{dereferencing a possibly dangling pointer}}
}

// https://godbolt.org/z/uXExWo
void example_2_4_7_3() {
    int* p = nullptr;		// pset(p) = {null}
    int i = 0;
    {
        int j = 1;
        if(cond) {
            p = &i; 		// pset(p) = {i}
            *p = 42; 		// ok
        } 				    // no invalidation
        else {
            p = &j; 		// pset(p) = {j}
            *p = 42; 		// ok
        } 			    	// no invalidation
        // merge => pset(p) = {i,j}
        *p = 42; 			// ok
    }
}

// https://godbolt.org/z/4dTX9n
void example_2_4_8_1() {
    int* p = nullptr;			// A: pset(p) = {null} expected-note {{assigned here}}
    int i = 0;
    if(cond) { 
        p = &i;				    // pset(p) = {i}
    } 
    // merge: pset(p) = {null,i}
    *p = 42;					// ERROR, p could be null from line A expected-warning {{dereferencing a possibly null pointer}}
    if(p) {					    // remove null in this branch => pset(p) = {i}
        *p = 42;				// ok, pset(p) == {i}
    }
    // here, outside the null testing branch, pset(p) is still {null,i}
}

// https://godbolt.org/z/Q-4zwP
void example_2_4_8_2() {
    int i = 0, j = 0;
    int* p = cond ? &i : nullptr;	// A: pset(p) = {i, null} expected-note {{assigned here}}

    *p = 42;				    	// ERROR, p could be null from line A expected-warning {{dereferencing a possibly null pointer}}

    if(!p) {					    // in this branch, pset(p) = {null}
        p = &j; 				    // pset(p) = {j}
    }
    // NOTE: in implicit “else”, pset(p) = {i}
    // merge pset(p) = {j} U {i}

    *p = 1;	 				        // ok, pset(p) = {i,j}, does not contain null
}

// https://godbolt.org/z/_midIP
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

// https://godbolt.org/z/p_QjCR
static int gi = 0;
void example_2_4_10_2() {
    int i = 0;
    throw &i;				// ERROR expected-warning {{throwing a pointer with points-to set (i) where points-to set ((static)) is expected}}
    throw &gi;				// OK
}

// https://godbolt.org/z/1xAGOH
void example_2_5_6_2() {
    auto sp = make_shared<int>(0);
    int* p = sp.get();			// pset(p) = pset(sp.get()) which is {sp'}
    *p = 42;					// ok
    sp = make_shared<int>(1);	// KILL(sp') => pset(p) = {invalid} expected-note {{modified here}}
    *p = 42;					// ERROR expected-warning {{dereferencing a dangling pointer}}
}

// https://godbolt.org/z/1C4t8m
void example_2_5_6_4() {
    auto x=10, y=2;
    auto& good = min(x,y);		// ok
    cout << good;			    // ok
    auto& bad = min(x,y+1);		// A expected-note {{temporary was destroyed at the end of the full expression}}
    cout << bad;				// ERROR, bad initialized as invalid on line A expected-warning {{dereferencing a dangling pointer}}
}

// https://godbolt.org/z/sKIltx
struct widget { };
widget& example_2_5_6_5() {	// pset(ret) = {static}
    static widget w;
    return w;				// ok, {static} is substitutable for {static}
}

// https://godbolt.org/z/vC5g6G
int* example_2_5_6_6a() {	// pset(ret) = {static}
    int i = 0;
    return &i;		    	// pset(&i) = {i}, then KILL(i) -> pset(ret) = {invalid}
				        	// ERROR, {invalid} is not substitutable for {static}
    // expected-warning@-2 {{returning a dangling Pointer}}
    // expected-note@-3 {{pointee 'i' left the scope here}}
}

// https://godbolt.org/z/53qeAD
int* example_2_5_6_7_a(int* pi) {
    int i = 0;
    return cond ? pi : &i;	// pset(expr)={pi,i}, KILL(i) => pset(expr)={invalid}
 	        				// ERROR, {invalid} is not subtitutable for {pi}
    // expected-warning@-2 {{returning a dangling Pointer}}
    // expected-note@-3 {{pointee 'i' left the scope here}}
}

// https://godbolt.org/z/-exrN7
int* f();					// pset(ret) = {static}
void example_2_5_6_8() {
    int* p = f(); 			// pset(p) = {static}
    *p = 42; 				// ok
}

// https://godbolt.org/z/4G-8H-
struct A {};
void use(A) {}
unique_ptr<A> myFun();
void example_2_6_1() {
    const A& rA = *myFun();		// A: ERROR, rA is unusable, initialized invalid 
	        					// reference (invalidated by destruction of the
			        			// temporary unique_ptr returned from myFun)
    // expected-note@-3 {{temporary was destroyed at the end of the full expression}}
    use(rA);		    		// ERROR, rA initialized as invalid on line A expected-warning {{dereferencing a dangling pointer}}
}

// https://godbolt.org/z/dymV_C
void example_2_6_2_1() {
    std::string_view s = "foo"s;		// A
    // expected-note@-1 {{temporary was destroyed at the end of the full expression}}
    s[0]; // expected-warning {{passing a dangling pointer as argument}}
}

// https://godbolt.org/z/0aeHPp
std::string operator+ (std::string_view s1, std::string_view s2) {
    return std::string{s1} + std::string{s2};
}
void example_2_6_2_2() {
    std::string_view sv = "hi";
    sv[0];                  // ok
    sv = sv + sv; 			// A expected-note {{temporary was destroyed at the end of the full expression}}
    sv[0];					// ERROR (lifetime.3): sv was invalidated when expected-warning {{passing a dangling pointer as argument}}
						    // temporary "foo"s was destroyed (line A)
}

// https://godbolt.org/z/IsPG8P
template<typename T>	// for T == string_view, pset(ret) = {x1,x2},
T add(T x1, T x2) {		//   pset(x1) = {x1}, pset(x2) = {x2}
    return x1 + x2 ;	// A: ERROR (lifetime.4): {tmp'} was invalidated
}						//   when temporary ‘x1 + x2’ was destroyed (line A)
// expected-warning@-2 {{returning a dangling Pointer}}
// expected-note@-3 {{temporary was destroyed at the end of the full expression}}

void example_2_6_2_3() {
    add(1,2);           // ok
    std::string_view sv = "hi";
    sv = add(sv, sv);   // instantiates add<string_view> expected-note {{in instantiation}}
}

// https://godbolt.org/z/wncC9a
struct X { int a, b; };

int&  example_2_6_2_4(X& x) { return x.a; }		// ok, pset(ret) == pset(x)

// https://godbolt.org/z/AqXDYp
void example_2_6_2_5() {
    char& c = std::string{"hello my pretty long string"}[0]; // expected-note {{temporary was destroyed at the end of the full expression}}
    cout << c;              // ERROR expected-warning {{dereferencing a dangling pointer}}
}

// https://godbolt.org/z/BvP4w6
using K = string;
using V = string;
const V& findOrDefault(const std::map<K,V>& m, const K& key, const V& defvalue);
						// because K and V are std::string (an Owner),
						// pset(ret) = {m',key',defvalue'}

void example_2_6_2_7() {
    std::map<std::string, std::string> myMap;
    std::string key = "xyzzy";
    const std::string& s = findOrDefault(myMap, key, "none");
						// A: pset(s) = {mymap', key', tmp')
						// tmp destroyed --> pset(s) = {invalid}
    // expected-note@-3 {{temporary was destroyed at the end of the full expression}}
    s[0];				// ERROR (lifetime.3): ‘s’ was invalidated when expected-warning {{dereferencing a dangling pointer}}
						// temporary ‘"none"’ was destroyed (line A)
}
