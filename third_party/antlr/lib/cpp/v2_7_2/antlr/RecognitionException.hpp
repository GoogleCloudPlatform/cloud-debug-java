#ifndef INC_RecognitionException_hpp__
# define INC_RecognitionException_hpp__

/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.jGuru.com
 * Software rights: http://www.antlr.org/RIGHTS.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.2/lib/cpp/antlr/RecognitionException.hpp#1 $
 */

# include <antlr/config.hpp>
# include <antlr/ANTLRException.hpp>

# ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr
{
# endif
	class ANTLR_API RecognitionException : public ANTLRException
	{
	public:
    ANTLR_DECLARE_DYNAMIC( RecognitionException, ANTLRException, ANTLRException );

		RecognitionException();
		explicit RecognitionException(const string& s);
		RecognitionException(const string& s,
									const string& fileName_,
									int line_,int column_);

		virtual ~RecognitionException() throw()
		{
		}

		/// Return file where mishap occurred.
		virtual string getFilename() const
		{
			return fileName;
		}
		/**
		 * @return the line number that this exception happened on.
		 */
		virtual int getLine() const
		{
			return line;
		}
		/**
		 * @return the column number that this exception happened on.
		 */
		virtual int getColumn() const
		{
			return column;
		}
#if 0
		/**
		 * @deprecated As of ANTLR 2.7.0
		 */
		virtual string getErrorMessage() const
		{
			return getMessage();
		}
#endif

		/// Return complete error message with line/column number info (if present)
		virtual string toString() const;

		/// See what file/line/column info is present and return it as a string
		virtual string getFileLineColumnString() const;
	protected:
		string fileName; // not used by treeparsers
		int line;    // not used by treeparsers
		int column;  // not used by treeparsers
	};

# ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
# endif

#endif //INC_RecognitionException_hpp__
