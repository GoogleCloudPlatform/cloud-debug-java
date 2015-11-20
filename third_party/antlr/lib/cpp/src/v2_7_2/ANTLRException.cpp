
#include "antlr/ANTLRException.hpp"
#include "antlr/String.hpp"

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr {
#endif

// Again, the root is implemented directly.
const void* const s_typeId = &s_typeId; \
bool ANTLRException::IsKindOf( const void* typeId) const
{
  return typeId == s_typeId;
}
ANTLRException * ANTLRException::DynamicCast( ANTLRException& object )
{
  return &object;
}
const ANTLRException * ANTLRException::DynamicCast( const ANTLRException& object)
{
  return &object;
}




#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
#endif
