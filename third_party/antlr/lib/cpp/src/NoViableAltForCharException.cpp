/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.jGuru.com
 * Software rights: http://www.antlr.org/RIGHTS.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.2/lib/cpp/src/NoViableAltForCharException.cpp#1 $
 */

#include "antlr/NoViableAltForCharException.hpp"
#include "antlr/String.hpp"

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr {
#endif

ANTLR_IMPLEMENT_DYNAMIC( NoViableAltForCharException, RecognitionException, ANTLRException );


NoViableAltForCharException::NoViableAltForCharException(int c, CharScanner* scanner)
  : RecognitionException("NoViableAlt",
                         scanner->getFilename(),
								 scanner->getLine(),scanner->getColumn()),
    foundChar(c)
{
}

NoViableAltForCharException::NoViableAltForCharException(
					int c,
                                        const string& fileName_,
					int line_, int column_)
  : RecognitionException("NoViableAlt",fileName_,line_,column_),
    foundChar(c)
{
}

string NoViableAltForCharException::getMessage() const
{
	return string("unexpected char: ")+charName(foundChar);
}

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
#endif
