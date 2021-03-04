// RUN: %clang_cc1 -std=c++2a %s

struct trivial
{
  int a;
  int b;
};

struct nontrivial
{
  virtual ~nontrivial() = default;
  int a;
  int b;
};

/// Note that moving scalars is dumb.
int byval_scalar(int x)
{
  return static_cast<int&&>(x);
}

int move_scalar(move int x)
{
  return static_cast<int&&>(x);
}

trivial byref_trivial(trivial&& x)
{
  return static_cast<trivial&&>(x);
}

trivial move_trivial(move trivial x)
{
  return static_cast<trivial&&>(x);
}

nontrivial byref_nontrivial(nontrivial&& x)
{
  return static_cast<nontrivial&&>(x);
}

nontrivial move_nontrivial(move nontrivial x)
{
  return static_cast<nontrivial&&>(x);
}

