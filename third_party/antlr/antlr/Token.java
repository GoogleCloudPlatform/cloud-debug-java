package antlr;

/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.cs.usfca.edu
 * Software rights: http://www.antlr.org/license.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.7/antlr/Token.java#2 $
 */

/** A token is minimally a token type.  Subclasses can add the text matched
 *  for the token and line info.
 */
public class Token implements Cloneable {
    // constants
    public static final int MIN_USER_TYPE = 4;
    public static final int NULL_TREE_LOOKAHEAD = 3;
    public static final int INVALID_TYPE = 0;
    public static final int EOF_TYPE = 1;
    public static final int SKIP = -1;

    // each Token has at least a token type
    protected int type = INVALID_TYPE;

    // the illegal token object
    public static Token badToken = new Token(INVALID_TYPE, "<no text>");

    public Token() {
    }

    public Token(int t) {
        type = t;
    }

    public Token(int t, String txt) {
        type = t;
        setText(txt);
    }

    public int getColumn() {
        return 0;
    }

    public int getLine() {
        return 0;
    }

	public String getFilename() {
		return null;
	}

	public void setFilename(String name) {
	}

    public String getText() {
        return "<no text>";
    }

	public void setText(String t) {
    }

    public void setColumn(int c) {
    }

    public void setLine(int l) {
    }

	public int getType() {
        return type;
    }

    public void setType(int t) {
        type = t;
    }

    public String toString() {
        return "[\"" + getText() + "\",<" + getType() + ">]";
    }
}
