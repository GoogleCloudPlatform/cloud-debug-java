#ifndef EXCEPTION_SLOT_H
#define EXCEPTION_SLOT_H

#include <antlr/ANTLRException.hpp>
#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr {
#endif

// Mixin class for sharing a semi-global exception object

class ANTLR_API ExceptionSlot
{
public:
  ExceptionSlot();
  virtual ~ExceptionSlot();

  // Somewhere, someone must set up storage for the exception
  // pointer and the point people to it.
  // Subclasses may need to forward this call to other objects
  // that it holds. For example, the parser should forward to
  // ParserSharedInputState
  virtual void SetExceptionSlot(ANTLRException **ppEx);
  virtual ANTLRException **GetExceptionSlot();

  //
  void SetException(ANTLRException *pEx);
  void ClearException();
  // Return a pointer to the active exception, else 0
  ANTLRException* ActiveException();

private:
  ANTLRException  **ppEx_;

};


#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
#endif


#endif // EXCEPTION_SLOT_H
