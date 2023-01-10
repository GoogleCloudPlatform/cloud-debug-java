package antlr.ASdebug;

/**
 * Provides offset info for a token.<br>
 * All offsets are 0-based.
 * @author Prashant Deva
 */
public class TokenOffsetInfo
{
    public final int beginOffset, length;
    
    public TokenOffsetInfo(int offset, int length)
    {
        this.beginOffset = offset;
        this.length = length;
    }
    
    public int getEndOffset()
    {
       return beginOffset+length-1;   
    }
}
