/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.jGuru.com
 * Software rights: http://www.antlr.org/RIGHTS.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.2/lib/cpp/src/BaseAST.cpp#1 $
 */

#include "antlr/config.hpp"
#include "antlr/AST.hpp"
#include "antlr/BaseAST.hpp"

#include <typeinfo>
#include <iostream>

ANTLR_USING_NAMESPACE(std)
#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr {
#endif

//bool BaseAST::verboseStringConversion;
//ANTLR_USE_NAMESPACE(std)vector<string> BaseAST::tokenNames;

BaseAST::BaseAST() : AST()
{
}

BaseAST::~BaseAST()
{
}

BaseAST::BaseAST(const BaseAST& other)
: AST(other) // RK: don't copy links! , down(other.down), right(other.right)
{
}

const char* BaseAST::typeName( void ) const
{
	return "BaseAST";
}

RefAST BaseAST::clone( void ) const
{
	cerr << "BaseAST::clone()" << endl;
	return nullAST;
}

void BaseAST::addChild( RefAST c )
{
	if( !c )
		return;

	RefBaseAST tmp = down;

	if (tmp)
	{
		while (tmp->right)
			tmp = tmp->right;
		tmp->right = c;
	}
	else
		down = c;
}

void BaseAST::doWorkForFindAll(
		ANTLR_USE_NAMESPACE(std)vector<RefAST>& v,
		RefAST target,bool partialMatch)
{
	// Start walking sibling lists, looking for matches.
	for (RefAST sibling=this;
			sibling;
			sibling=sibling->getNextSibling())
	{
		if ( (partialMatch && sibling->equalsTreePartial(target)) ||
				(!partialMatch && sibling->equalsTree(target)) ) {
			v.push_back(sibling);
		}
		// regardless of match or not, check any children for matches
		if ( sibling->getFirstChild() ) {
			RefBaseAST(sibling->getFirstChild())->doWorkForFindAll(v, target, partialMatch);
		}
	}
}

/** Is t an exact structural and equals() match of this tree.  The
 *  'this' reference is considered the start of a sibling list.
 */
bool BaseAST::equalsList(RefAST t) const
{
	// the empty tree is not a match of any non-null tree.
	if (!t)
		return false;

	// Otherwise, start walking sibling lists.  First mismatch, return false.
	RefAST sibling=this;
	for (;sibling && t;
			sibling=sibling->getNextSibling(), t=t->getNextSibling()) {
		// as a quick optimization, check roots first.
		if (!sibling->equals(t))
			return false;
		// if roots match, do full list match test on children.
		if (sibling->getFirstChild()) {
			if (!sibling->getFirstChild()->equalsList(t->getFirstChild()))
				return false;
		}
		// sibling has no kids, make sure t doesn't either
		else if (t->getFirstChild())
			return false;
	}

	if (!sibling && !t)
		return true;

	// one sibling list has more than the other
	return false;
}

/** Is 'sub' a subtree of this list?
 *  The siblings of the root are NOT ignored.
 */
bool BaseAST::equalsListPartial(RefAST sub) const
{
	// the empty tree is always a subset of any tree.
	if (!sub)
		return true;

	// Otherwise, start walking sibling lists.  First mismatch, return false.
	RefAST sibling=this;
	for (;sibling && sub;
			sibling=sibling->getNextSibling(), sub=sub->getNextSibling()) {
		// as a quick optimization, check roots first.
		if (!sibling->equals(sub))
			return false;
		// if roots match, do partial list match test on children.
		if (sibling->getFirstChild())
			if (!sibling->getFirstChild()->equalsListPartial(sub->getFirstChild()))
				return false;
	}

	if (!sibling && sub)
		// nothing left to match in this tree, but subtree has more
		return false;

	// either both are null or sibling has more, but subtree doesn't
	return true;
}

/** Is tree rooted at 'this' equal to 't'?  The siblings
 *  of 'this' are ignored.
 */
bool BaseAST::equalsTree(RefAST t) const
{
	// check roots first
	if (!equals(t))
		return false;
	// if roots match, do full list match test on children.
	if (getFirstChild()) {
		if (!getFirstChild()->equalsList(t->getFirstChild()))
			return false;
	}
	// sibling has no kids, make sure t doesn't either
	else if (t->getFirstChild())
		return false;

	return true;
}

/** Is 'sub' a subtree of the tree rooted at 'this'?  The siblings
 *  of 'this' are ignored.
 */
bool BaseAST::equalsTreePartial(RefAST sub) const
{
	// the empty tree is always a subset of any tree.
	if (!sub)
		return true;

	// check roots first
	if (!equals(sub))
		return false;
	// if roots match, do full list partial match test on children.
	if (getFirstChild())
		if (!getFirstChild()->equalsListPartial(sub->getFirstChild()))
			return false;

	return true;
}

/** Walk the tree looking for all exact subtree matches.  Return
 *  an ASTEnumerator that lets the caller walk the list
 *  of subtree roots found herein.
 */
ANTLR_USE_NAMESPACE(std)vector<RefAST> BaseAST::findAll(RefAST target)
{
	ANTLR_USE_NAMESPACE(std)vector<RefAST> roots;

	// the empty tree cannot result in an enumeration
	if (target) {
		doWorkForFindAll(roots,target,false); // find all matches recursively
	}

	return roots;
}

/** Walk the tree looking for all subtrees.  Return
 *  an ASTEnumerator that lets the caller walk the list
 *  of subtree roots found herein.
 */
ANTLR_USE_NAMESPACE(std)vector<RefAST> BaseAST::findAllPartial(RefAST target)
{
	ANTLR_USE_NAMESPACE(std)vector<RefAST> roots;

	// the empty tree cannot result in an enumeration
	if (target)
		doWorkForFindAll(roots,target,true); // find all matches recursively

	return roots;
}

void BaseAST::setText(const string& txt)
{
}

void BaseAST::setType(int type)
{
}

string BaseAST::toString() const
{
	return getText();
}

string BaseAST::toStringList() const
{
	string ts="";

	if (getFirstChild())
	{
		ts+=" ( ";
		ts+=toString();
		ts+=getFirstChild()->toStringList();
		ts+=" )";
	}
	else
	{
		ts+=" ";
		ts+=toString();
	}

	if (getNextSibling())
		ts+=getNextSibling()->toStringList();

	return ts;
}

string BaseAST::toStringTree() const
{
	string ts = "";

	if (getFirstChild())
	{
		ts+=" ( ";
		ts+=toString();
		ts+=getFirstChild()->toStringList();
		ts+=" )";
	}
	else
	{
		ts+=" ";
		ts+=toString();
	}
	return ts;
}

#ifdef ANTLR_SUPPORT_XML
/* This whole XML output stuff needs a little bit more thought
 * I'd like to store extra XML data in the node. e.g. for custom ast's
 * with for instance symboltable references. This
 * should be more pluggable..
 * @returns boolean value indicating wether a closetag should be produced.
 */
bool BaseAST::attributesToStream( ANTLR_USE_NAMESPACE(std)ostream& out ) const
{
	out << "text=\"" << this->getText()
		<< "\" type=\"" << this->getType() << "\"";

	return false;
}

void BaseAST::toStream( ANTLR_USE_NAMESPACE(std)ostream& out ) const
{
	for( RefAST node = this; node != 0; node = node->getNextSibling() )
	{
		out << "<" << this->typeName() << " ";

		// Write out attributes and if there is extra data...
		bool need_close_tag = node->attributesToStream( out );

		if( need_close_tag )
		{
			// got children so write them...
			if( node->getFirstChild() != 0 )
				node->getFirstChild()->toStream( out );

			// and a closing tag..
			out << "</" << node->typeName() << ">" << endl;
		}
	}
}
#endif

// this is nasty, but it makes the code generation easier
ANTLR_API RefAST nullAST;
ANTLR_API AST* const nullASTptr=0;

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
#endif
