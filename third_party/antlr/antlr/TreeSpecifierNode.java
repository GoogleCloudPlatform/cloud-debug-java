package antlr;

/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.cs.usfca.edu
 * Software rights: http://www.antlr.org/license.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.7/antlr/TreeSpecifierNode.java#2 $
 */

class TreeSpecifierNode {
    private TreeSpecifierNode parent = null;
    private TreeSpecifierNode firstChild = null;
    private TreeSpecifierNode nextSibling = null;
    private Token tok;


    TreeSpecifierNode(Token tok_) {
        tok = tok_;
    }

    public TreeSpecifierNode getFirstChild() {
        return firstChild;
    }

    public TreeSpecifierNode getNextSibling() {
        return nextSibling;
    }

    // Accessors
    public TreeSpecifierNode getParent() {
        return parent;
    }

    public Token getToken() {
        return tok;
    }

    public void setFirstChild(TreeSpecifierNode child) {
        firstChild = child;
        child.parent = this;
    }

    // Structure-building
    public void setNextSibling(TreeSpecifierNode sibling) {
        nextSibling = sibling;
        sibling.parent = parent;
    }
}
