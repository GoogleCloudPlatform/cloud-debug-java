package antlr;

/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.cs.usfca.edu
 * Software rights: http://www.antlr.org/license.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.7/antlr/StringLiteralSymbol.java#2 $
 */

class StringLiteralSymbol extends TokenSymbol {
    protected String label;	// was this string literal labeled?


    public StringLiteralSymbol(String r) {
        super(r);
    }

    public String getLabel() {
        return label;
    }

    public void setLabel(String label) {
        this.label = label;
    }
}
