/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.jGuru.com
 * Software rights: http://www.antlr.org/RIGHTS.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.2/lib/cpp/src/MismatchedCharException.cpp#1 $
 */

#include "antlr/CharScanner.hpp"
#include "antlr/MismatchedCharException.hpp"
#include "antlr/String.hpp"
#include "antlr/DynamicCast.hpp"

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr {
#endif

ANTLR_IMPLEMENT_DYNAMIC( MismatchedCharException, RecognitionException, ANTLRException );

MismatchedCharException::MismatchedCharException()
  : RecognitionException("Mismatched char")
{}

// Expected range / not range
MismatchedCharException::MismatchedCharException(
	int c,
	int lower,
	int upper_,
	bool matchNot,
	CharScanner* scanner_
) : RecognitionException("Mismatched char",
                         scanner_->getFilename(),
								 scanner_->getLine(), scanner_->getColumn())
  , mismatchType(matchNot ? NOT_RANGE : RANGE)
  , foundChar(c)
  , expecting(lower)
  , upper(upper_)
  , scanner(scanner_)
{
}

// Expected token / not token
MismatchedCharException::MismatchedCharException(
	int c,
	int expecting_,
	bool matchNot,
	CharScanner* scanner_
) : RecognitionException("Mismatched char",
                         scanner_->getFilename(),
								 scanner_->getLine(), scanner_->getColumn())
  , mismatchType(matchNot ? NOT_CHAR : CHAR)
  , foundChar(c)
  , expecting(expecting_)
  , scanner(scanner_)
{
}

// Expected BitSet / not BitSet
MismatchedCharException::MismatchedCharException(
	int c,
	BitSet set_,
	bool matchNot,
	CharScanner* scanner_
) : RecognitionException("Mismatched char",
                         scanner_->getFilename(),
								 scanner_->getLine(), scanner_->getColumn())
  , mismatchType(matchNot ? NOT_SET : SET)
  , foundChar(c)
  , set(set_)
  , scanner(scanner_)
{
}

string MismatchedCharException::getMessage() const
{
	string s;

	switch (mismatchType) {
	case CHAR :
		s += "expecting '" + charName(expecting) + "', found '" + charName(foundChar) + "'";
		break;
	case NOT_CHAR :
		s += "expecting anything but '" + charName(expecting) + "'; got it anyway";
		break;
	case RANGE :
		s += "expecting token in range: '" + charName(expecting) + "'..'" + charName(upper) + "', found '" + charName(foundChar) + "'";
		break;
	case NOT_RANGE :
		s += "expecting token NOT in range: " + charName(expecting) + "'..'" + charName(upper) + "', found '" + charName(foundChar) + "'";
		break;
	case SET :
	case NOT_SET :
		{
			s += string("expecting ") + (mismatchType == NOT_SET ? "NOT " : "") + "one of (";
			ANTLR_USE_NAMESPACE(std)vector<unsigned int> elems = set.toArray();
			for ( unsigned int i = 0; i < elems.size(); i++ )
			{
				s += " '";
				s += charName(elems[i]);
				s += "'";
			}
			s += "), found '" + charName(foundChar) + "'";
		}
		break;
	default :
		s += RecognitionException::getMessage();
		break;
	}

	return s;
}

#ifndef NO_STATIC_CONSTS
const int MismatchedCharException::CHAR;
const int MismatchedCharException::NOT_CHAR;
const int MismatchedCharException::RANGE;
const int MismatchedCharException::NOT_RANGE;
const int MismatchedCharException::SET;
const int MismatchedCharException::NOT_SET;
#endif

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
#endif
