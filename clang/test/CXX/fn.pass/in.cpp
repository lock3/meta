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

// Extended parameter passing

int byval_ref(int x)
{
  return x;
}

int in_scalar(in int x)
{
  return x;
}

trivial byval_trival(trivial x)
{
  return x;
}

trivial in_trivial(in trivial x)
{
  return x;
}

nontrivial byref_nontrivial(nontrivial const& x)
{
  return x;
}

nontrivial in_nontrivial(in nontrivial x)
{
  return x;
}

