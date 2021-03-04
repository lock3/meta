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

void byref_scalar(int& x)
{
  x = 42;
}

void out_scalar(out int x)
{
  x = 42;
}

void byref_trivial(trivial& x)
{
  x = trivial();
}

void out_trivial(out trivial x)
{
  x = trivial();
}

void byref_nontrivial(nontrivial& x)
{
  x = nontrivial();
}

void out_nontrivial(out nontrivial x)
{
  x = nontrivial();
}

