#ifndef INC_CharStreamIOException_hpp__
#define INC_CharStreamIOException_hpp__

/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.jGuru.com
 * Software rights: http://www.antlr.org/RIGHTS.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.2/lib/cpp/antlr/CharStreamIOException.hpp#1 $
 */

#include <antlr/config.hpp>
#include <antlr/CharStreamException.hpp>

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr {
#endif

class ANTLR_API CharStreamIOException : public CharStreamException {
public:
// JLW change
//	ANTLR_USE_NAMESPACE(std)exception io;

  ANTLR_DECLARE_DYNAMIC( CharStreamIOException, CharStreamException, ANTLRException );
// JLW change
//	CharStreamIOException(ANTLR_USE_NAMESPACE(std)exception& e)
//		: CharStreamException(e.what()), io(e) {}
	explicit CharStreamIOException(const ANTLR_USE_NAMESPACE(std)string& s)
		: CharStreamException(s) {}
	~CharStreamIOException() throw() {}
};

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
#endif

#endif //INC_CharStreamIOException_hpp__
