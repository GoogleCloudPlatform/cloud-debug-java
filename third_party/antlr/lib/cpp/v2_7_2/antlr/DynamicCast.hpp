// Author: Joseph White
//
// Poor mans dynamic cast. Based on the description in
// "Large Scale C++ Design" by John Lakos

#ifndef DYNAMIC_CAST_H
#define DYNAMIC_CAST_H

// Put the following macro in the class header file
//    Although this does not currently work for multiple base classes
//    the macro is easily extended

#define ANTLR_DECLARE_DYNAMIC( className, baseClass, rootClass ) \
  static className * DynamicCast( rootClass& object); \
  static const className * DynamicCast( const rootClass& object); \
  static className * DynamicCast( rootClass* object); \
  static const className * DynamicCast( const rootClass* object); \
  virtual bool IsKindOf(const void* typeId) const;

// Put this in the cpp file
#define ANTLR_IMPLEMENT_DYNAMIC( className, baseClass, rootClass ) \
  const void* const s_typeId = &s_typeId; \
  bool className::IsKindOf( const void* typeId) const   \
  {                                                     \
    return typeId == s_typeId                           \
           || baseClass::IsKindOf(typeId);              \
  }                                                     \
  className * className::DynamicCast( rootClass& object )              \
  {                                                                    \
    return object.IsKindOf(s_typeId) ? (className *) &object : 0;      \
  }                                                                    \
  const className * className::DynamicCast( const rootClass& object)   \
  {                                                                    \
    return object.IsKindOf(s_typeId) ? (className *) &object : 0;      \
  }                                                                    \
  className * className::DynamicCast( rootClass* object )              \
  {                                                                    \
    return (object && object->IsKindOf(s_typeId)) ? (className *) object : 0;      \
  }                                                                    \
  const className * className::DynamicCast( const rootClass* object)   \
  {                                                                    \
    return (object && object->IsKindOf(s_typeId)) ? (className *) object : 0;      \
  }

#endif
