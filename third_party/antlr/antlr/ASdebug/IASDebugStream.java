package antlr.ASdebug;

import antlr.Token;

/**
 * Provides information used by the 'Input Text' view 
 * of Antlr Studio.
 * @author Prashant Deva
 */
public interface IASDebugStream
{
    /**
     * Returns the entire text input to the lexer.
     * @return The entire text or <code>null</code>, if error occured or System.in was used.
     */
    String getEntireText();
    
    /**
     * Returns the offset information for the token
     * @param token the token whose information need to be retrieved
     * @return offset info, or <code>null</code>
     */
    TokenOffsetInfo getOffsetInfo(Token token);
}
