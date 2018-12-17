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

#endif
