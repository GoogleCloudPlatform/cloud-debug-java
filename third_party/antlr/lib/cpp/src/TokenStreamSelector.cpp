/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.jGuru.com
 * Software rights: http://www.antlr.org/RIGHTS.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.2/lib/cpp/src/TokenStreamSelector.cpp#1 $
 */
#include <stdio.h>
#include <stdlib.h>
#include "antlr/TokenStreamSelector.hpp"
#include "antlr/TokenStreamRetryException.hpp"

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr {
#endif

/** A token stream MUX (multiplexor) knows about n token streams
 *  and can multiplex them onto the same channel for use by token
 *  stream consumer like a parser.  This is a way to have multiple
 *  lexers break up the same input stream for a single parser.
 *	Or, you can have multiple instances of the same lexer handle
 *  multiple input streams; this works great for includes.
 */

TokenStreamSelector::TokenStreamSelector()
: input(0)
{
}

TokenStreamSelector::~TokenStreamSelector()
{
}

void TokenStreamSelector::addInputStream(TokenStream* stream, const ANTLR_USE_NAMESPACE(std)string& key)
{
	inputStreamNames[key] = stream;
}

TokenStream* TokenStreamSelector::getCurrentStream() const
{
	return input;
}

TokenStream* TokenStreamSelector::getStream(const ANTLR_USE_NAMESPACE(std)string& sname) const
{
	inputStreamNames_coll::const_iterator i = inputStreamNames.find(sname);
	if (i == inputStreamNames.end()) {
#ifdef ANTLR_EXCEPTIONS
		throw ANTLR_USE_NAMESPACE(std)string("TokenStream ")+sname+" not found";
#else
    // For now. Notice that the were not throwing an ANTLRException derived exception
    ANTLR_USE_NAMESPACE(std)string error( "TokenStream "+sname+" not found");
    ANTLR_FATAL( error.c_str() ) ;
#endif
	}
	return (*i).second;
}

RefToken TokenStreamSelector::nextToken()
{
	// keep looking for a token until you don't
	// get a retry exception
	for (;;) {
    RefToken retToken = input->nextToken();
    if (!ActiveException())
      return retToken;
    else if ( ANTLR_USE_NAMESPACE(antlr)TokenStreamRetryException::DynamicCast(ActiveException())  )
      ClearException();  // try again
    else
      return retToken;  // "Throw" to caller
#if 0
		try {
			return input->nextToken();
		}
		catch (TokenStreamRetryException& r) {
			// just retry "forever"
		}
#endif
	}
}

TokenStream* TokenStreamSelector::pop()
{
	TokenStream* stream = streamStack.top();
	streamStack.pop();
	select(stream);
	return stream;
}

void TokenStreamSelector::push(TokenStream* stream)
{
	streamStack.push(input);
	select(stream);
}

void TokenStreamSelector::push(const ANTLR_USE_NAMESPACE(std)string& sname)
{
	streamStack.push(input);
	select(sname);
}

void TokenStreamSelector::retry()
{
#ifdef ANTLR_EXCEPTIONS
	throw TokenStreamRetryException();
#else
  SetException( new TokenStreamRetryException() ) ;
  return;
#endif
}

/** Set the stream without pushing old stream */
void TokenStreamSelector::select(TokenStream* stream)
{
	input = stream;
  input->SetExceptionSlot(GetExceptionSlot());
}

void TokenStreamSelector::select(const ANTLR_USE_NAMESPACE(std)string& sname)
{
	inputStreamNames_coll::const_iterator i = inputStreamNames.find(sname);
	if (i == inputStreamNames.end()) {
#ifdef  ANTLR_EXCEPTIONS
		throw ANTLR_USE_NAMESPACE(std)string("TokenStream ")+sname+" not found";
#else
    // For now. Notice that the were not throwing an ANTLRException derived exception
    ANTLR_USE_NAMESPACE(std)string error( "TokenStream "+sname+" not found");
    ANTLR_FATAL( error.c_str() ) ;
#endif
	}
	input = (*i).second;
  input->SetExceptionSlot(GetExceptionSlot());
}

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
#endif
