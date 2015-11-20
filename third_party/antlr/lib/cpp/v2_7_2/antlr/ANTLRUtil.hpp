#ifndef INC_ANTLRUtil_hpp__
#define INC_ANTLRUtil_hpp__

/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.jGuru.com
 * Software rights: http://www.antlr.org/RIGHTS.html
 *
 * $Id: //depot/code/org.antlr/main/main/lib/cpp/antlr/ASTFactory.hpp#9 $
 */

#include <antlr/config.hpp>

class ANTLRException;

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr {
#endif

/** Eat whitespace from the input stream
 * @param is the stream to read from
 */
ANTLR_USE_NAMESPACE(std)istream& eatwhite( ANTLR_USE_NAMESPACE(std)istream& is );

/** Read a string enclosed by '"' from a stream. Also handles escaping of \".
 * Skips leading whitespace.
 * @param in the istream to read from.
 * @returns the string read from file exclusive the '"'
 * @throws ios_base::failure if string is badly formatted
 */
ANTLR_USE_NAMESPACE(std)string read_string( ANTLR_USE_NAMESPACE(std)istream& in,
                                            ANTLRException** pExSlot );

/* Read a ([A-Z][0-9][a-z]_)* kindoff thing. Skips leading whitespace.
 * @param in the istream to read from.
 */

ANTLR_USE_NAMESPACE(std)string read_identifier( ANTLR_USE_NAMESPACE(std)istream& in );

/** Read a attribute="value" thing. Leading whitespace is skipped.
 * Between attribute and '=' no whitespace is allowed. After the '=' it is
 * permitted.
 * @param in the istream to read from.
 * @param attribute string the attribute name is put in
 * @param value string the value of the attribute is put in
 * @throws ios_base::failure if something is fishy. E.g. malformed quoting
 * or missing '='
 */
void read_AttributeNValue( ANTLR_USE_NAMESPACE(std)istream& in,
								   ANTLR_USE_NAMESPACE(std)string& attribute,
								   ANTLR_USE_NAMESPACE(std)string& value,
                   ANTLRException** pExSlot );

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
#endif

#endif
