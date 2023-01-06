package antlr;

/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.cs.usfca.edu
 * Software rights: http://www.antlr.org/license.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.7/antlr/CharFormatter.java#2 $
 */

/** Interface used by BitSet to format elements of the set when
 * converting to string
 */
public interface CharFormatter {


    public String escapeChar(int c, boolean forCharLiteral);

    public String escapeString(String s);

    public String literalChar(int c);

    public String literalString(String s);
}
