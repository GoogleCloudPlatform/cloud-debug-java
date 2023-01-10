package antlr.preprocessor;

/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.cs.usfca.edu
 * Software rights: http://www.antlr.org/license.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.7/antlr/preprocessor/Option.java#2 $
 */

import antlr.collections.impl.Vector;

class Option {
    protected String name;
    protected String rhs;
    protected Grammar enclosingGrammar;

    public Option(String n, String rhs, Grammar gr) {
        name = n;
        this.rhs = rhs;
        setEnclosingGrammar(gr);
    }

    public Grammar getEnclosingGrammar() {
        return enclosingGrammar;
    }

    public String getName() {
        return name;
    }

    public String getRHS() {
        return rhs;
    }

    public void setEnclosingGrammar(Grammar g) {
        enclosingGrammar = g;
    }

    public void setName(String n) {
        name = n;
    }

    public void setRHS(String rhs) {
        this.rhs = rhs;
    }

    public String toString() {
        return "\t" + name + "=" + rhs;
    }
}
