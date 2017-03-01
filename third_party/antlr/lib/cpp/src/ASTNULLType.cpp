/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.jGuru.com
 * Software rights: http://www.antlr.org/RIGHTS.html
 *
 * $Id: //depot/code/org.antlr/main/main/lib/cpp/antlr/ASTFactory.hpp#9 $
 */

#include "antlr/config.hpp"
#include "antlr/AST.hpp"
#include "antlr/ASTNULLType.hpp"

#include <iostream>

ANTLR_USING_NAMESPACE(std)

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr {
#endif

RefAST ASTNULLType::clone( void ) const
{
	return RefAST(this);
}

void ASTNULLType::addChild(RefAST c)
{
}

bool ASTNULLType::equals(RefAST t) const
{
	return false;
}

bool ASTNULLType::equalsList(RefAST t) const
{
	return false;
}

bool ASTNULLType::equalsListPartial(RefAST t) const
{
	return false;
}

bool ASTNULLType::equalsTree(RefAST t) const
{
	return false;
}

bool ASTNULLType::equalsTreePartial(RefAST t) const
{
	return false;
}

std::vector<RefAST> ASTNULLType::findAll(RefAST tree)
{
	return std::vector<RefAST>();
}

std::vector<RefAST> ASTNULLType::findAllPartial(RefAST subtree)
{
	return std::vector<RefAST>();
}

RefAST ASTNULLType::getFirstChild() const
{
	return RefAST(this);
}

RefAST ASTNULLType::getNextSibling() const
{
	return RefAST(this);
}

string ASTNULLType::getText() const
{
	return "<ASTNULL>";
}

int ASTNULLType::getType() const
{
	return Token::NULL_TREE_LOOKAHEAD;
}

void ASTNULLType::initialize(int t, const string& txt)
{
}

void ASTNULLType::initialize(RefAST t)
{
}

void ASTNULLType::initialize(RefToken t)
{
}

#ifdef ANTLR_SUPPORT_XML
void ASTNULLType::initialize( istream& )
{
}
#endif

void ASTNULLType::setFirstChild(RefAST c)
{
}

void ASTNULLType::setNextSibling(RefAST n)
{
}

void ASTNULLType::setText(const string& text)
{
}

void ASTNULLType::setType(int ttype)
{
}

string ASTNULLType::toString() const
{
	return getText();
}

string ASTNULLType::toStringList() const
{
	return getText();
}

string ASTNULLType::toStringTree() const
{
	return getText();
}

#ifdef ANTLR_SUPPORT_XML
bool ASTNULLType::attributesToStream( ostream &out ) const
{
	return false;
}

void ASTNULLType::toStream( ostream &out ) const
{
	out << "<ASTNULL/>" << endl;
}
#endif

const char* ASTNULLType::typeName( void ) const
{
	return "ASTNULLType";
}

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
#endif
