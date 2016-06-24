/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.jGuru.com
 * Software rights: http://www.antlr.org/RIGHTS.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.2/lib/cpp/src/ASTRefCount.cpp#1 $
 */
#include "antlr/ASTRefCount.hpp"
#include "antlr/AST.hpp"

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr {
#endif

ASTRef::ASTRef(AST* p, bool static_ref)
: ptr(p), count(static_ref ? -1 : 1)
{
        if (p && !p->ref)
                p->ref = this;
}

ASTRef::~ASTRef()
{
        delete ptr;
}

ASTRef* ASTRef::getRef(const AST* p)
{
        if (p) {
                AST* pp = const_cast<AST*>(p);
                if (pp->ref)
                        return pp->ref->increment();
                else
                        return new ASTRef(pp, false);
        } else
                return 0;
}

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
#endif

