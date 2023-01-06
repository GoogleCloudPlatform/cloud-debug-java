package antlr;

/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.cs.usfca.edu
 * Software rights: http://www.antlr.org/license.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.7/antlr/ActionTransInfo.java#2 $
 */

/**
 * This class contains information about how an action
 * was translated (using the AST conversion rules).
 */
public class ActionTransInfo {
    public boolean assignToRoot = false;	// somebody did a "#rule = "
    public String refRuleRoot = null;		// somebody referenced #rule; string is translated var
    public String followSetName = null;		// somebody referenced $FOLLOW; string is the name of the lookahead set

    public String toString() {
        return "assignToRoot:" + assignToRoot + ", refRuleRoot:"
				+ refRuleRoot + ", FOLLOW Set:" + followSetName;
    }
}
