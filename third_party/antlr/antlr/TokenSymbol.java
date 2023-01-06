package antlr;

/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.cs.usfca.edu
 * Software rights: http://www.antlr.org/license.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.7/antlr/TokenSymbol.java#2 $
 */

class TokenSymbol extends GrammarSymbol {
    protected int ttype;
    /** describes what token matches in "human terms" */
    protected String paraphrase = null;

    /** Set to a value in the tokens {...} section */
    protected String ASTNodeType;

    public TokenSymbol(String r) {
        super(r);
        ttype = Token.INVALID_TYPE;
    }

    public String getASTNodeType() {
        return ASTNodeType;
    }

    public void setASTNodeType(String type) {
        ASTNodeType = type;
    }

    public String getParaphrase() {
        return paraphrase;
    }

    public int getTokenType() {
        return ttype;
    }

    public void setParaphrase(String p) {
        paraphrase = p;
    }

    public void setTokenType(int t) {
        ttype = t;
    }
}
