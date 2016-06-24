#ifndef INC_CharStreamException_hpp__
#define INC_CharStreamException_hpp__

/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.jGuru.com
 * Software rights: http://www.antlr.org/RIGHTS.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.2/lib/cpp/antlr/CharStreamException.hpp#1 $
 */

#include <antlr/config.hpp>
#include <antlr/ANTLRException.hpp>

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr {
#endif

class ANTLR_API CharStreamException : public ANTLRException {
public:
  ANTLR_DECLARE_DYNAMIC( CharStreamException, ANTLRException, ANTLRException );

	explicit CharStreamException(const ANTLR_USE_NAMESPACE(std)string& s)
		: ANTLRException(s) {}
	~CharStreamException() throw() {}
};

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
#endif

#endif //INC_CharStreamException_hpp__
