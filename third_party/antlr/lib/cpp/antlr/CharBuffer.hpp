#ifndef INC_CharBuffer_hpp__
#define INC_CharBuffer_hpp__

/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.jGuru.com
 * Software rights: http://www.antlr.org/RIGHTS.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.2/lib/cpp/antlr/CharBuffer.hpp#1 $
 */

#include <antlr/config.hpp>
#include <antlr/InputBuffer.hpp>

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr {
#endif

/**A Stream of characters fed to the lexer from a InputStream that can
 * be rewound via mark()/rewind() methods.
 * <p>
 * A dynamic array is used to buffer up all the input characters.  Normally,
 * "k" characters are stored in the buffer.  More characters may be stored
 * during guess mode (testing syntactic predicate), or when LT(i>k) is
 * referenced.
 * Consumption of characters is deferred.  In other words, reading the next
 * character is not done by consume(), but deferred until needed by LA or LT.
 * <p>
 *
 * @see antlr.CharQueue
 */

class ANTLR_API CharBuffer : public InputBuffer {
private:
	// character source
	ANTLR_USE_NAMESPACE(std)istream& input;

public:
	/// Create a character buffer
	explicit CharBuffer(ANTLR_USE_NAMESPACE(std)istream& input_);
	/// Get the next character from the stream
	int getChar();
private:
	// NOTE: Unimplemented
	CharBuffer(const CharBuffer& other);
	CharBuffer& operator=(const CharBuffer& other);
};

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
#endif

#endif //INC_CharBuffer_hpp__
