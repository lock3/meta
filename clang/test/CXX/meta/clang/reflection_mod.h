#ifndef REFLECTION_MOD_H
#define REFLECTION_MOD_H

enum class AccessModifier : unsigned {
  NotModified,
  Default,
  Public,
  Protected,
  Private
};

enum class StorageModifier : unsigned {
  NotModified,
  Static,
  Automatic,
  ThreadLocal
};

enum class ConstexprModifier : unsigned {
  NotModified,
  Constexpr,
  Consteval,
  Constinit
};

#endif
