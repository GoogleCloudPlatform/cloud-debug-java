#ifndef INC_TokenStreamRecognitionException_hpp__
#define INC_TokenStreamRecognitionException_hpp__

/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.jGuru.com
 * Software rights: http://www.antlr.org/RIGHTS.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.2/lib/cpp/antlr/TokenStreamRecognitionException.hpp#1 $
 */

#include <antlr/config.hpp>
#include <antlr/RecognitionException.hpp>
#include <antlr/TokenStreamException.hpp>

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr {
#endif

class TokenStreamRecognitionException : public TokenStreamException {
public:

	explicit TokenStreamRecognitionException(RecognitionException& re)
	: TokenStreamException(re.getMessage())
	, recog(re)
	{
	}
	~TokenStreamRecognitionException() throw()
	{
	}
	string toString() const
	{
		return recog.getFileLineColumnString()+getMessage();
	}

	virtual ANTLR_USE_NAMESPACE(std)string getFilename() const throw()
	{
		return recog.getFilename();
	}
	virtual int getLine() const throw()
	{
		return recog.getLine();
	}
	virtual int getColumn() const throw()
	{
		return recog.getColumn();
	}

  ANTLR_DECLARE_DYNAMIC( TokenStreamRecognitionException, TokenStreamException, ANTLRException );
private:
	RecognitionException recog;
};

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
#endif

#endif //INC_TokenStreamRecognitionException_hpp__
