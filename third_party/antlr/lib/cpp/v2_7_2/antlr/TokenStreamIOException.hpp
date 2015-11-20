#ifndef INC_TokenStreamIOException_hpp__
#define INC_TokenStreamIOException_hpp__

/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.jGuru.com
 * Software rights: http://www.antlr.org/RIGHTS.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.2/lib/cpp/antlr/TokenStreamIOException.hpp#1 $
 */

#include <antlr/config.hpp>
#include <antlr/TokenStreamException.hpp>

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr {
#endif

class TokenStreamIOException : public TokenStreamException {
public:
  ANTLR_DECLARE_DYNAMIC( TokenStreamIOException, TokenStreamException, ANTLRException );
	
  TokenStreamIOException()
	: TokenStreamException()	
	{
	}
//	TokenStreamIOException(const ANTLR_USE_NAMESPACE(std)exception& e)
//	: TokenStreamException(e.what())
//	, io(e)
//	{
//	}
	explicit TokenStreamIOException(const ANTLR_USE_NAMESPACE(std)string& s)
		: TokenStreamException(s) {}

	~TokenStreamIOException() throw()
	{
	}
private:
//	ANTLR_USE_NAMESPACE(std)exception io;
};

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
#endif

#endif //INC_TokenStreamIOException_hpp__
