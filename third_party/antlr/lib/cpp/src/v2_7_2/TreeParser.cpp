/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.jGuru.com
 * Software rights: http://www.antlr.org/RIGHTS.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.2/lib/cpp/src/TreeParser.cpp#1 $
 */

#include "antlr/TreeParser.hpp"
#include "antlr/ASTNULLType.hpp"
#include "antlr/MismatchedTokenException.hpp"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr {
#endif

ANTLR_C_USING(exit)

TreeParser::TreeParser()
: astFactory(0), inputState(new TreeParserInputState()), traceDepth(0)
{
  pExSlot_ = 0;
  SetExceptionSlot( &pExSlot_ );
}

TreeParser::TreeParser(const TreeParserSharedInputState& state)
: astFactory(0), inputState(state), traceDepth(0)
{
  pExSlot_ = 0;
  SetExceptionSlot( &pExSlot_ );
}

TreeParser::~TreeParser()
{
}

/** The AST Null object; the parsing cursor is set to this when
 *  it is found to be null.  This way, we can test the
 *  token type of a node without having to have tests for null
 *  everywhere.
 *
 *  Create a "static reference counter", i.e. don't do any
 *  reference counting as ASTNULL is supposed to exist
 *  "forever".
 */
RefAST TreeParser::ASTNULL(new ASTNULLType, true);

/** Get the AST return value squirreled away in the parser */
//RefAST getAST() const {
//      return returnAST;
//}

void TreeParser::match(RefAST t, int ttype)
{
        if (!t || t==ASTNULL || t->getType()!=ttype)
  {
#ifdef ANTLR_EXCEPTIONS
                throw MismatchedTokenException();
#else
    SetException( new MismatchedTokenException());
#endif
  }
}

/**Make sure current lookahead symbol matches the given set
 * Throw an exception upon mismatch, which is caught by either the
 * error handler or by the syntactic predicate.
 */
void TreeParser::match(RefAST t, const BitSet& b)
{
        if ( !t || t==ASTNULL || !b.member(t->getType()) )
  {
#ifdef ANTLR_EXCEPTIONS
                throw MismatchedTokenException();
#else
    SetException( new MismatchedTokenException());
#endif
        }
}

void TreeParser::matchNot(RefAST t, int ttype)
{
        //ANTLR_USE_NAMESPACE(std)cout << "match(" << ttype << "); cursor is " << t.toString() << ANTLR_USE_NAMESPACE(std)endl;
        if ( !t || t==ASTNULL || t->getType()==ttype ) {
#ifdef ANTLR_EXCEPTIONS
                throw MismatchedTokenException();
#else
    SetException( new MismatchedTokenException());
#endif
        }
}

void TreeParser::panic()
{
        ANTLR_USE_NAMESPACE(std)cerr << "TreeWalker: panic" << ANTLR_USE_NAMESPACE(std)endl;
        exit(1);
}

/** Parser error-reporting function can be overridden in subclass */
void TreeParser::reportError(const RecognitionException& ex)
{
        ANTLR_USE_NAMESPACE(std)cerr << ex.toString().c_str() << ANTLR_USE_NAMESPACE(std)endl;
}

/** Parser error-reporting function can be overridden in subclass */
void TreeParser::reportError(const ANTLR_USE_NAMESPACE(std)string& s)
{
        ANTLR_USE_NAMESPACE(std)cerr << "error: " << s.c_str() << ANTLR_USE_NAMESPACE(std)endl;
}

/** Parser warning-reporting function can be overridden in subclass */
void TreeParser::reportWarning(const ANTLR_USE_NAMESPACE(std)string& s)
{
        ANTLR_USE_NAMESPACE(std)cerr << "warning: " << s.c_str() << ANTLR_USE_NAMESPACE(std)endl;
}

/** Procedure to write out an indent for traceIn and traceOut */
void TreeParser::traceIndent()
{
        for( int i = 0; i < traceDepth; i++ )
                ANTLR_USE_NAMESPACE(std)cout << " ";
}

void TreeParser::traceIn(const char* rname, RefAST t)
{
        traceDepth++;
        traceIndent();

        ANTLR_USE_NAMESPACE(std)cout << "> " << rname
                        << "(" << (t ? t->toString().c_str() : "null") << ")"
                        << ((inputState->guessing>0)?" [guessing]":"")
                        << ANTLR_USE_NAMESPACE(std)endl;
}

void TreeParser::traceOut(const char* rname, RefAST t)
{
        traceIndent();

        ANTLR_USE_NAMESPACE(std)cout << "< " << rname
                        << "(" << (t ? t->toString().c_str() : "null") << ")"
                        << ((inputState->guessing>0)?" [guessing]":"")
                        << ANTLR_USE_NAMESPACE(std)endl;

        traceDepth--;
}

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
#endif
