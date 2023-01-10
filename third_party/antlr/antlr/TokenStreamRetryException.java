package antlr;

/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.cs.usfca.edu
 * Software rights: http://www.antlr.org/license.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.7/antlr/TokenStreamRetryException.java#2 $
 */

/**
 * Aborted recognition of current token. Try to get one again.
 * Used by TokenStreamSelector.retry() to force nextToken()
 * of stream to re-enter and retry.
 */
public class TokenStreamRetryException extends TokenStreamException {
    public TokenStreamRetryException() {
    }
}
