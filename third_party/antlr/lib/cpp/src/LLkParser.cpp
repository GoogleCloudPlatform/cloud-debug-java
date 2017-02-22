/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.jGuru.com
 * Software rights: http://www.antlr.org/RIGHTS.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.2/lib/cpp/src/LLkParser.cpp#1 $
 */

#include "antlr/LLkParser.hpp"
#include <iostream>

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr {
#endif

ANTLR_USING_NAMESPACE(std)

/**An LL(k) parser.
 *
 * @see antlr.Token
 * @see antlr.TokenBuffer
 * @see antlr.LL1Parser
 */

//	LLkParser(int k_);

LLkParser::LLkParser(const ParserSharedInputState& state, int k_)
: Parser(state), k(k_)
{
}

LLkParser::LLkParser(TokenBuffer& tokenBuf, int k_)
: Parser(tokenBuf), k(k_)
{
}

LLkParser::LLkParser(TokenStream& lexer, int k_)
: Parser(new TokenBuffer(lexer)), k(k_)
{
}

void LLkParser::trace(const char* ee, const char* rname)
{
	traceIndent();

	std::cout << ee << rname << ((inputState->guessing>0)?"; [guessing]":"; ");

	for (int i = 1; i <= k; i++)
	{
		if (i != 1) {
			std::cout << ", ";
		}
		std::cout << "LA(" << i << ")==";

		string temp;

//		try {
			temp = LT(i)->getText().c_str();
//		}
//		catch( ANTLRException& ae )
//		{
//			temp = "[error: ";
//			temp += ae.toString();
//			temp += ']';
//		}
		std::cout << temp;
	}

	std::cout << std::endl;
}

void LLkParser::traceIn(const char* rname)
{
	traceDepth++;
	trace("> ",rname);
}

void LLkParser::traceOut(const char* rname)
{
	trace("< ",rname);
	traceDepth--;
}

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
#endif
