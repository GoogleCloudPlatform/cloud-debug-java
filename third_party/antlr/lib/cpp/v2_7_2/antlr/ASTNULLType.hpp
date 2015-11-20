#ifndef INC_ASTNULLType_hpp__
#define INC_ASTNULLType_hpp__

/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.jGuru.com
 * Software rights: http://www.antlr.org/RIGHTS.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.2/lib/cpp/antlr/ASTNULLType.hpp#1 $
 */

#include <antlr/config.hpp>
#include <antlr/AST.hpp>
#include <iostream>

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr {
#endif

/** There is only one instance of this class **/
class ANTLR_API ASTNULLType : public AST {
public:
	const char* typeName( void ) const;
	RefAST clone( void ) const;

	void addChild(RefAST c);
	void setFirstChild(RefAST c);
	void setNextSibling(RefAST n);

	bool equals(RefAST t) const;
	bool equalsList(RefAST t) const;
	bool equalsListPartial(RefAST t) const;
	bool equalsTree(RefAST t) const;
	bool equalsTreePartial(RefAST t) const;

	ANTLR_USE_NAMESPACE(std)vector<RefAST> findAll(RefAST tree);
	ANTLR_USE_NAMESPACE(std)vector<RefAST> findAllPartial(RefAST subtree);

	RefAST getFirstChild() const;
	RefAST getNextSibling() const;

	string getText() const;
	int getType() const;

	void initialize(int t, const string& txt);
	void initialize(RefAST t);
	void initialize(RefToken t);
	void initialize(ANTLR_USE_NAMESPACE(std)istream& infile);

	void setText(const string& text);
	void setType(int ttype);
	string toString() const;
	string toStringList() const;
	string toStringTree() const;

	bool attributesToStream( ANTLR_USE_NAMESPACE(std)ostream &out ) const;
	void toStream( ANTLR_USE_NAMESPACE(std)ostream &out ) const;
};

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
#endif

#endif //INC_ASTNULLType_hpp__
