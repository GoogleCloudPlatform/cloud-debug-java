package antlr;

/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.cs.usfca.edu
 * Software rights: http://www.antlr.org/license.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.7/antlr/ASTVisitor.java#2 $
 */

import antlr.collections.AST;

public interface ASTVisitor {
    public void visit(AST node);
}
