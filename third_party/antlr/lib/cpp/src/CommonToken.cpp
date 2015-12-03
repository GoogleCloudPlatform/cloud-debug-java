/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.jGuru.com
 * Software rights: http://www.antlr.org/RIGHTS.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.2/lib/cpp/src/CommonToken.cpp#1 $
 */

#include "antlr/CommonToken.hpp"
#include "antlr/String.hpp"

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr {
#endif

CommonToken::CommonToken() : Token(), line(1), col(1), text("")
{}

CommonToken::CommonToken(int t, const string& txt)
	: Token(t), line(1), col(1), text(txt)
{}

CommonToken::CommonToken(const string& s)
	: Token(), line(1), col(1), text(s)
{}

string CommonToken::toString() const
{
	return "[\""+getText()+"\",<"+type+">,line="+line+",column="+col+"]";
}

RefToken CommonToken::factory()
{
	return RefToken(new CommonToken);
}

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
#endif
