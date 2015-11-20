#ifndef INC_IOException_hpp__
#define INC_IOException_hpp__

/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.jGuru.com
 * Software rights: http://www.antlr.org/RIGHTS.html
 *
 * $Id: //depot/code/org.antlr/main/main/lib/cpp/antlr/ASTFactory.hpp#9 $
 */

#include <antlr/config.hpp>
#include <antlr/ANTLRException.hpp>

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr {
#endif

/** Generic IOException used inside support code. (thrown by XML I/O routs)
 * basically this is something I'm using since a lot of compilers don't
 * support ios_base::failure.
 */
class ANTLR_API IOException : public ANTLRException
{
public:
//	ANTLR_USE_NAMESPACE(std)exception io;
  ANTLR_DECLARE_DYNAMIC( IOException, ANTLRException, ANTLRException );

//	IOException( ANTLR_USE_NAMESPACE(std)exception& e )
//		: ANTLRException(e.what())
//	{
//	}
	explicit IOException( const ANTLR_USE_NAMESPACE(std)string& mesg )
		: ANTLRException(mesg)
	{
	}
	virtual ~IOException() throw()
	{
	}
};

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
#endif

#endif //INC_IOException_hpp__
