#ifndef INC_ANTLRException_hpp__
#define INC_ANTLRException_hpp__

/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.jGuru.com
 * Software rights: http://www.antlr.org/RIGHTS.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.2/lib/cpp/antlr/ANTLRException.hpp#1 $
 */

#include <string>
#include <antlr/config.hpp>
#include <antlr/DynamicCast.hpp>

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr {
#endif

#define ANTLR_CHECK( condition ) \
  if ( !(condition) )                                                                     \
  {                                                                                       \
    fprintf(stderr, "Check failed: %s\n  at %s, %d\n", #condition, __FILE__, __LINE__ );  \
    exit(1);                                                                              \
  }

#define ANTLR_FATAL( message ) \
  {                                                                                    \
    fprintf(stderr, "Fatal error: %s\n  at %s, %d\n", message , __FILE__, __LINE__ );  \
    exit(1);                                                                           \
  }

#define ANTLR_CHECK_EXCEPTION \
if ( ActiveException() ) {    \
  return;                     \
}

#define ANTLR_CHECK_EXCEPTION_WRTN( rtnValue )  \
if ( ActiveException() ) {                      \
  return (rtnValue);                            \
}



class ANTLR_API ANTLRException
{
public:
	/// Create ANTLR base exception without error message
	ANTLRException() : text("")
	{
	}
	/// Create ANTLR base exception with error message
	explicit ANTLRException(const string& s)
	: text(s)
	{
	}
	virtual ~ANTLRException() throw()
	{
	}

	/** Return complete error message with line/column number info (if present)
	 * @note for your own exceptions override this one. Call getMessage from
	 * here to get the 'clean' error message stored in the text attribute.
	 */
	virtual string toString() const
	{
		return text;
	}

	/** Return error message without additional info (if present)
	 * @note when making your own exceptions classes override toString
	 * and call in toString getMessage which relays the text attribute
	 * from here.
	 */
	virtual string getMessage() const
	{
		return text;
	}

  // This is the root of the hierarchy, so macro doesn't quite work right
  static ANTLRException * DynamicCast( ANTLRException& object);
  static const ANTLRException * DynamicCast( const ANTLRException& object);
  virtual bool IsKindOf(const void* typeId) const;


private:
	string text;
};
#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
#endif

#endif //INC_ANTLRException_hpp__
