package antlr;

/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.cs.usfca.edu
 * Software rights: http://www.antlr.org/license.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.7/antlr/AlternativeElement.java#2 $
 */

abstract class AlternativeElement extends GrammarElement {
    AlternativeElement next;
    protected int autoGenType = AUTO_GEN_NONE;

    protected String enclosingRuleName;

    public AlternativeElement(Grammar g) {
        super(g);
    }

    public AlternativeElement(Grammar g, Token start) {
        super(g, start);
    }

    public AlternativeElement(Grammar g, Token start, int autoGenType_) {
        super(g, start);
        autoGenType = autoGenType_;
    }

    public int getAutoGenType() {
        return autoGenType;
    }

    public void setAutoGenType(int a) {
        autoGenType = a;
    }

    public String getLabel() {
        return null;
    }

    public void setLabel(String label) {
    }
}
