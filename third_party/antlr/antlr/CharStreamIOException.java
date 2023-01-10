package antlr;

/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.cs.usfca.edu
 * Software rights: http://www.antlr.org/license.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.7/antlr/CharStreamIOException.java#2 $
 */

import java.io.IOException;

/**
 * Wrap an IOException in a CharStreamException
 */
public class CharStreamIOException extends CharStreamException {
    public IOException io;

    public CharStreamIOException(IOException io) {
        super(io.getMessage());
        this.io = io;
    }
}
