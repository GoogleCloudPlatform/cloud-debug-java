/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.jGuru.com
 * Software rights: http://www.antlr.org/RIGHTS.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.2/lib/cpp/src/RecognitionException.cpp#1 $
 */

#include "antlr/RecognitionException.hpp"
#include "antlr/String.hpp"

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr {
#endif

ANTLR_IMPLEMENT_DYNAMIC( RecognitionException, ANTLRException, ANTLRException );

RecognitionException::RecognitionException()
: ANTLRException("parsing error")
, line(-1)
, column(-1)
{
}

RecognitionException::RecognitionException(const string& s)
: ANTLRException(s)
, line(-1)
, column(-1)
{
}

RecognitionException::RecognitionException(const string& s,
                                           const string& fileName_,
                                           int line_,int column_)
: ANTLRException(s)
, fileName(fileName_)
, line(line_)
, column(column_)
{
}

string RecognitionException::getFileLineColumnString() const
{
	string fileLineColumnString;

	if ( fileName.length() > 0 )
		fileLineColumnString = fileName + ":";

	if ( line != -1 )
	{
		if ( fileName.length() == 0 )
			fileLineColumnString = fileLineColumnString + "line ";

		fileLineColumnString = fileLineColumnString + line;

		if ( column != -1 )
			fileLineColumnString = fileLineColumnString + ":" + column;

		fileLineColumnString = fileLineColumnString + ":";
	}

	fileLineColumnString = fileLineColumnString + " ";

	return fileLineColumnString;
}

string RecognitionException::toString() const
{
	return getFileLineColumnString()+getMessage();
}

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
#endif
