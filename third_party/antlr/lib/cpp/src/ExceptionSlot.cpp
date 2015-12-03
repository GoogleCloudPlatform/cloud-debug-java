#include "antlr/ExceptionSlot.hpp"

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr {
#endif

ANTLR_USING_NAMESPACE(std)

ExceptionSlot::ExceptionSlot()
  : ppEx_(0) {
}

ExceptionSlot::~ExceptionSlot(){
  ClearException();
}

void ExceptionSlot::SetExceptionSlot(ANTLRException **ppEx){
  ClearException();
  ppEx_ = ppEx;
}

ANTLRException** ExceptionSlot::GetExceptionSlot(){
  return ppEx_;
}


void ExceptionSlot::SetException(ANTLRException *pEx){
  if (!ppEx_) return;
  if (*ppEx_) {
    // if this isn't done, a memory leak may occur
    // (and previously, has).
    delete *ppEx_;
  }
  *ppEx_ = pEx;
}

void ExceptionSlot::ClearException(){
  if (!ppEx_) return;
  if (*ppEx_) delete *ppEx_;
  *ppEx_ = 0;
}

ANTLRException* ExceptionSlot::ActiveException(){
  return   *ppEx_;
}

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
#endif
