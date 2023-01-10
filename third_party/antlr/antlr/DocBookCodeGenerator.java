package antlr;

/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.cs.usfca.edu
 * Software rights: http://www.antlr.org/license.html
 *
 * $Id:$
 */

/** TODO: strip comments from javadoc entries
 */

import java.util.Enumeration;

import antlr.collections.impl.BitSet;
import antlr.collections.impl.Vector;

import java.io.PrintWriter; //SAS: changed for proper text file io
import java.io.IOException;
import java.io.FileWriter;

/**Generate P.sgml, a cross-linked representation of P with or without actions */
public class DocBookCodeGenerator extends CodeGenerator {
    /** non-zero if inside syntactic predicate generation */
    protected int syntacticPredLevel = 0;

    /** true during lexer generation, false during parser generation */
    protected boolean doingLexRules = false;

    protected boolean firstElementInAlt;

    protected AlternativeElement prevAltElem = null;	// what was generated last?

    /** Create a Diagnostic code-generator using the given Grammar
     * The caller must still call setTool, setBehavior, and setAnalyzer
     * before generating code.
     */
    public DocBookCodeGenerator() {
        super();
        charFormatter = new JavaCharFormatter();
    }

    /** Encode a string for printing in a HTML document..
     * e.g. encode '<' '>' and similar stuff
     * @param s the string to encode
     */
    static String HTMLEncode(String s) {
        StringBuffer buf = new StringBuffer();

        for (int i = 0, len = s.length(); i < len; i++) {
            char c = s.charAt(i);
            if (c == '&')
                buf.append("&amp;");
            else if (c == '\"')
                buf.append("&quot;");
            else if (c == '\'')
                buf.append("&#039;");
            else if (c == '<')
                buf.append("&lt;");
            else if (c == '>')
                buf.append("&gt;");
            else
                buf.append(c);
        }
        return buf.toString();
    }

    /** Encode a string for printing in a HTML document..
     * e.g. encode '<' '>' and similar stuff
     * @param s the string to encode
     */
    static String QuoteForId(String s) {
        StringBuffer buf = new StringBuffer();

        for (int i = 0, len = s.length(); i < len; i++) {
            char c = s.charAt(i);
            if (c == '_')
                buf.append(".");
            else
                buf.append(c);
        }
        return buf.toString();
    }

    public void gen() {
        // Do the code generation
        try {
            // Loop over all grammars
            Enumeration grammarIter = behavior.grammars.elements();
            while (grammarIter.hasMoreElements()) {
                Grammar g = (Grammar)grammarIter.nextElement();

                // Connect all the components to each other
                /*
				g.setGrammarAnalyzer(analyzer);
				analyzer.setGrammar(g);
				*/
                g.setCodeGenerator(this);

                // To get right overloading behavior across hetrogeneous grammars
                g.generate();

                if (antlrTool.hasError()) {
                    antlrTool.fatalError("Exiting due to errors.");
                }

            }

        }
        catch (IOException e) {
            antlrTool.reportException(e, null);
        }
    }

    /** Generate code for the given grammar element.
     * @param blk The {...} action to generate
     */
    public void gen(ActionElement action) {
        // no-op
    }

    /** Generate code for the given grammar element.
     * @param blk The "x|y|z|..." block to generate
     */
    public void gen(AlternativeBlock blk) {
        genGenericBlock(blk, "");
    }

    /** Generate code for the given grammar element.
     * @param blk The block-end element to generate.  Block-end
     * elements are synthesized by the grammar parser to represent
     * the end of a block.
     */
    public void gen(BlockEndElement end) {
        // no-op
    }

    /** Generate code for the given grammar element.
     * @param blk The character literal reference to generate
     */
    public void gen(CharLiteralElement atom) {
        if (atom.not) {
            _print("~");
        }
        _print(HTMLEncode(atom.atomText) + " ");
    }

    /** Generate code for the given grammar element.
     * @param blk The character-range reference to generate
     */
    public void gen(CharRangeElement r) {
        print(r.beginText + ".." + r.endText + " ");
    }

    /** Generate the lexer HTML file */
    public void gen(LexerGrammar g) throws IOException {
        setGrammar(g);
        antlrTool.reportProgress("Generating " + grammar.getClassName() + ".sgml");
        currentOutput = antlrTool.openOutputFile(grammar.getClassName() + ".sgml");

        tabs = 0;
        doingLexRules = true;

        // Generate header common to all TXT output files
        genHeader();

        // Output the user-defined lexer premamble
        // RK: guess not..
        // println(grammar.preambleAction.getText());

        // Generate lexer class definition
        println("");

        // print javadoc comment if any
        if (grammar.comment != null) {
            _println(HTMLEncode(grammar.comment));
        }

        println("<para>Definition of lexer " + grammar.getClassName() + ", which is a subclass of " + grammar.getSuperClass() + ".</para>");

        // Generate user-defined parser class members
        // printAction(grammar.classMemberAction.getText());

        /*
		// Generate string literals
		println("");
		println("*** String literals used in the parser");
		println("The following string literals were used in the parser.");
		println("An actual code generator would arrange to place these literals");
		println("into a table in the generated lexer, so that actions in the");
		println("generated lexer could match token text against the literals.");
		println("String literals used in the lexer are not listed here, as they");
		println("are incorporated into the mainstream lexer processing.");
		tabs++;
		// Enumerate all of the symbols and look for string literal symbols
		Enumeration ids = grammar.getSymbols();
		while ( ids.hasMoreElements() ) {
			GrammarSymbol sym = (GrammarSymbol)ids.nextElement();
			// Only processing string literals -- reject other symbol entries
			if ( sym instanceof StringLiteralSymbol ) {
				StringLiteralSymbol s = (StringLiteralSymbol)sym;
				println(s.getId() + " = " + s.getTokenType());
			}
		}
		tabs--;
		println("*** End of string literals used by the parser");
		*/

        // Generate nextToken() rule.
        // nextToken() is a synthetic lexer rule that is the implicit OR of all
        // user-defined lexer rules.
        genNextToken();

        // Generate code for each rule in the lexer

        Enumeration ids = grammar.rules.elements();
        while (ids.hasMoreElements()) {
            RuleSymbol rs = (RuleSymbol)ids.nextElement();
            if (!rs.id.equals("mnextToken")) {
                genRule(rs);
            }
        }

        // Close the lexer output file
        currentOutput.close();
        currentOutput = null;
        doingLexRules = false;
    }

    /** Generate code for the given grammar element.
     * @param blk The (...)+ block to generate
     */
    public void gen(OneOrMoreBlock blk) {
        genGenericBlock(blk, "+");
    }

    /** Generate the parser HTML file */
    public void gen(ParserGrammar g) throws IOException {
        setGrammar(g);
        // Open the output stream for the parser and set the currentOutput
        antlrTool.reportProgress("Generating " + grammar.getClassName() + ".sgml");
        currentOutput = antlrTool.openOutputFile(grammar.getClassName() + ".sgml");

        tabs = 0;

        // Generate the header common to all output files.
        genHeader();

        // Generate parser class definition
        println("");

        // print javadoc comment if any
        if (grammar.comment != null) {
            _println(HTMLEncode(grammar.comment));
        }

        println("<para>Definition of parser " + grammar.getClassName() + ", which is a subclass of " + grammar.getSuperClass() + ".</para>");

        // Enumerate the parser rules
        Enumeration rules = grammar.rules.elements();
        while (rules.hasMoreElements()) {
            println("");
            // Get the rules from the list and downcast it to proper type
            GrammarSymbol sym = (GrammarSymbol)rules.nextElement();
            // Only process parser rules
            if (sym instanceof RuleSymbol) {
                genRule((RuleSymbol)sym);
            }
        }
        tabs--;
        println("");

        genTail();

        // Close the parser output stream
        currentOutput.close();
        currentOutput = null;
    }

    /** Generate code for the given grammar element.
     * @param blk The rule-reference to generate
     */
    public void gen(RuleRefElement rr) {
        RuleSymbol rs = (RuleSymbol)grammar.getSymbol(rr.targetRule);

        // Generate the actual rule description
        _print("<link linkend=\"" + QuoteForId(rr.targetRule) + "\">");
        _print(rr.targetRule);
        _print("</link>");
        // RK: Leave out args..
        //	if (rr.args != null) {
        //		_print("["+rr.args+"]");
        //	}
        _print(" ");
    }

    /** Generate code for the given grammar element.
     * @param blk The string-literal reference to generate
     */
    public void gen(StringLiteralElement atom) {
        if (atom.not) {
            _print("~");
        }
        _print(HTMLEncode(atom.atomText));
        _print(" ");
    }

    /** Generate code for the given grammar element.
     * @param blk The token-range reference to generate
     */
    public void gen(TokenRangeElement r) {
        print(r.beginText + ".." + r.endText + " ");
    }

    /** Generate code for the given grammar element.
     * @param blk The token-reference to generate
     */
    public void gen(TokenRefElement atom) {
        if (atom.not) {
            _print("~");
        }
        _print(atom.atomText);
        _print(" ");
    }

    public void gen(TreeElement t) {
        print(t + " ");
    }

    /** Generate the tree-walker TXT file */
    public void gen(TreeWalkerGrammar g) throws IOException {
        setGrammar(g);
        // Open the output stream for the parser and set the currentOutput
        antlrTool.reportProgress("Generating " + grammar.getClassName() + ".sgml");
        currentOutput = antlrTool.openOutputFile(grammar.getClassName() + ".sgml");
        //SAS: changed for proper text file io

        tabs = 0;

        // Generate the header common to all output files.
        genHeader();

        // Output the user-defined parser premamble
        println("");
//		println("*** Tree-walker Preamble Action.");
//		println("This action will appear before the declaration of your tree-walker class:");
//		tabs++;
//		println(grammar.preambleAction.getText());
//		tabs--;
//		println("*** End of tree-walker Preamble Action");

        // Generate tree-walker class definition
        println("");

        // print javadoc comment if any
        if (grammar.comment != null) {
            _println(HTMLEncode(grammar.comment));
        }

        println("<para>Definition of tree parser " + grammar.getClassName() + ", which is a subclass of " + grammar.getSuperClass() + ".</para>");

        // Generate user-defined tree-walker class members
//		println("");
//		println("*** User-defined tree-walker class members:");
//		println("These are the member declarations that you defined for your class:");
//		tabs++;
//		printAction(grammar.classMemberAction.getText());
//		tabs--;
//		println("*** End of user-defined tree-walker class members");

        // Generate code for each rule in the grammar
        println("");
//		println("*** tree-walker rules:");
        tabs++;

        // Enumerate the tree-walker rules
        Enumeration rules = grammar.rules.elements();
        while (rules.hasMoreElements()) {
            println("");
            // Get the rules from the list and downcast it to proper type
            GrammarSymbol sym = (GrammarSymbol)rules.nextElement();
            // Only process tree-walker rules
            if (sym instanceof RuleSymbol) {
                genRule((RuleSymbol)sym);
            }
        }
        tabs--;
        println("");
//		println("*** End of tree-walker rules");

//		println("");
//		println("*** End of tree-walker");

        // Close the tree-walker output stream
        currentOutput.close();
        currentOutput = null;
    }

    /** Generate a wildcard element */
    public void gen(WildcardElement wc) {
        /*
		if ( wc.getLabel()!=null ) {
			_print(wc.getLabel()+"=");
		}
		*/
        _print(". ");
    }

    /** Generate code for the given grammar element.
     * @param blk The (...)* block to generate
     */
    public void gen(ZeroOrMoreBlock blk) {
        genGenericBlock(blk, "*");
    }

    protected void genAlt(Alternative alt) {
        if (alt.getTreeSpecifier() != null) {
            _print(alt.getTreeSpecifier().getText());
        }
        prevAltElem = null;
        for (AlternativeElement elem = alt.head;
             !(elem instanceof BlockEndElement);
             elem = elem.next) {
            elem.generate();
            firstElementInAlt = false;
            prevAltElem = elem;
        }
    }
    /** Generate the header for a block, which may be a RuleBlock or a
     * plain AlternativeBLock.  This generates any variable declarations,
     * init-actions, and syntactic-predicate-testing variables.
     * @blk The block for which the preamble is to be generated.
     */
//	protected void genBlockPreamble(AlternativeBlock blk) {
    // RK: don't dump out init actions
    // dump out init action
//		if ( blk.initAction!=null ) {
//			printAction("{" + blk.initAction + "}");
//		}
//	}
    /** Generate common code for a block of alternatives; return a postscript
     * that needs to be generated at the end of the block.  Other routines
     * may append else-clauses and such for error checking before the postfix
     * is generated.
     */
    public void genCommonBlock(AlternativeBlock blk) {
        if (blk.alternatives.size() > 1)
            println("<itemizedlist mark=\"none\">");
        for (int i = 0; i < blk.alternatives.size(); i++) {
            Alternative alt = blk.getAlternativeAt(i);
            AlternativeElement elem = alt.head;

            if (blk.alternatives.size() > 1)
                print("<listitem><para>");

            // dump alt operator |
            if (i > 0 && blk.alternatives.size() > 1) {
                _print("| ");
            }

            // Dump the alternative, starting with predicates
            //
            boolean save = firstElementInAlt;
            firstElementInAlt = true;
            tabs++;	// in case we do a newline in alt, increase the tab indent

            genAlt(alt);
            tabs--;
            firstElementInAlt = save;
            if (blk.alternatives.size() > 1)
                _println("</para></listitem>");
        }
        if (blk.alternatives.size() > 1)
            println("</itemizedlist>");
    }

    /** Generate a textual representation of the follow set
     * for a block.
     * @param blk  The rule block of interest
     */
    public void genFollowSetForRuleBlock(RuleBlock blk) {
        Lookahead follow = grammar.theLLkAnalyzer.FOLLOW(1, blk.endNode);
        printSet(grammar.maxk, 1, follow);
    }

    protected void genGenericBlock(AlternativeBlock blk, String blkOp) {
        if (blk.alternatives.size() > 1) {
            // make sure we start on a new line
            _println("");
            if (!firstElementInAlt) {
                // only do newline if the last element wasn't a multi-line block
                //if ( prevAltElem==null ||
                //	 !(prevAltElem instanceof AlternativeBlock) ||
                //	 ((AlternativeBlock)prevAltElem).alternatives.size()==1 )
                //{
                _println("(");
                //}
                //else
                //{
                //	_print("(");
                //}
                // _println("");
                // print("(\t");
            }
            else {
                _print("(");
            }
        }
        else {
            _print("( ");
        }
        // RK: don't dump init actions
        //	genBlockPreamble(blk);
        genCommonBlock(blk);
        if (blk.alternatives.size() > 1) {
            _println("");
            print(")" + blkOp + " ");
            // if not last element of alt, need newline & to indent
            if (!(blk.next instanceof BlockEndElement)) {
                _println("");
                print("");
            }
        }
        else {
            _print(")" + blkOp + " ");
        }
    }

    /** Generate a header that is common to all TXT files */
    protected void genHeader() {
        println("<?xml version=\"1.0\" standalone=\"no\"?>");
        println("<!DOCTYPE book PUBLIC \"-//OASIS//DTD DocBook V3.1//EN\">");
        println("<book lang=\"en\">");
        println("<bookinfo>");
        println("<title>Grammar " + grammar.getClassName() + "</title>");
        println("  <author>");
        println("    <firstname></firstname>");
        println("    <othername></othername>");
        println("    <surname></surname>");
        println("    <affiliation>");
        println("     <address>");
        println("     <email></email>");
        println("     </address>");
        println("    </affiliation>");
        println("  </author>");
        println("  <othercredit>");
        println("    <contrib>");
        println("    Generated by <ulink url=\"http://www.ANTLR.org/\">ANTLR</ulink>" + antlrTool.version);
        println("    from " + antlrTool.grammarFile);
        println("    </contrib>");
        println("  </othercredit>");
        println("  <pubdate></pubdate>");
        println("  <abstract>");
        println("  <para>");
        println("  </para>");
        println("  </abstract>");
        println("</bookinfo>");
        println("<chapter>");
        println("<title></title>");
    }

    /**Generate the lookahead set for an alternate. */
    protected void genLookaheadSetForAlt(Alternative alt) {
        if (doingLexRules && alt.cache[1].containsEpsilon()) {
            println("MATCHES ALL");
            return;
        }
        int depth = alt.lookaheadDepth;
        if (depth == GrammarAnalyzer.NONDETERMINISTIC) {
            // if the decision is nondeterministic, do the best we can: LL(k)
            // any predicates that are around will be generated later.
            depth = grammar.maxk;
        }
        for (int i = 1; i <= depth; i++) {
            Lookahead lookahead = alt.cache[i];
            printSet(depth, i, lookahead);
        }
    }

    /** Generate a textual representation of the lookahead set
     * for a block.
     * @param blk  The block of interest
     */
    public void genLookaheadSetForBlock(AlternativeBlock blk) {
        // Find the maximal lookahead depth over all alternatives
        int depth = 0;
        for (int i = 0; i < blk.alternatives.size(); i++) {
            Alternative alt = blk.getAlternativeAt(i);
            if (alt.lookaheadDepth == GrammarAnalyzer.NONDETERMINISTIC) {
                depth = grammar.maxk;
                break;
            }
            else if (depth < alt.lookaheadDepth) {
                depth = alt.lookaheadDepth;
            }
        }

        for (int i = 1; i <= depth; i++) {
            Lookahead lookahead = grammar.theLLkAnalyzer.look(i, blk);
            printSet(depth, i, lookahead);
        }
    }

    /** Generate the nextToken rule.
     * nextToken is a synthetic lexer rule that is the implicit OR of all
     * user-defined lexer rules.
     */
    public void genNextToken() {
        println("");
        println("/** Lexer nextToken rule:");
        println(" *  The lexer nextToken rule is synthesized from all of the user-defined");
        println(" *  lexer rules.  It logically consists of one big alternative block with");
        println(" *  each user-defined rule being an alternative.");
        println(" */");

        // Create the synthesized rule block for nextToken consisting
        // of an alternate block containing all the user-defined lexer rules.
        RuleBlock blk = MakeGrammar.createNextTokenRule(grammar, grammar.rules, "nextToken");

        // Define the nextToken rule symbol
        RuleSymbol nextTokenRs = new RuleSymbol("mnextToken");
        nextTokenRs.setDefined();
        nextTokenRs.setBlock(blk);
        nextTokenRs.access = "private";
        grammar.define(nextTokenRs);

        /*
		// Analyze the synthesized block
		if (!grammar.theLLkAnalyzer.deterministic(blk))
		{
			println("The grammar analyzer has determined that the synthesized");
			println("nextToken rule is non-deterministic (i.e., it has ambiguities)");
			println("This means that there is some overlap of the character");
			println("lookahead for two or more of your lexer rules.");
		}
		*/

        genCommonBlock(blk);
    }

    /** Generate code for a named rule block
     * @param s The RuleSymbol describing the rule to generate
     */
    public void genRule(RuleSymbol s) {
        if (s == null || !s.isDefined()) return;	// undefined rule
        println("");

        if (s.access.length() != 0) {
            if (!s.access.equals("public")) {
                _print("<para>" + s.access + " </para>");
            }
        }

        println("<section id=\"" + QuoteForId(s.getId()) + "\">");
        println("<title>" + s.getId() + "</title>");
        if (s.comment != null) {
            _println("<para>" + HTMLEncode(s.comment) + "</para>");
        }
        println("<para>");

        // Get rule return type and arguments
        RuleBlock rblk = s.getBlock();

        // RK: for HTML output not of much value...
        // Gen method return value(s)
//		if (rblk.returnAction != null) {
//			_print("["+rblk.returnAction+"]");
//		}
        // Gen arguments
//		if (rblk.argAction != null)
//		{
//				_print(" returns [" + rblk.argAction+"]");
//		}
        _println("");
        print(s.getId() + ":\t");
        tabs++;

        // Dump any init-action
        // genBlockPreamble(rblk);

        // Dump the alternates of the rule
        genCommonBlock(rblk);

        _println("");
//		println(";");
        tabs--;
        _println("</para>");
        _println("</section><!-- section \"" + s.getId() + "\" -->");
    }

    /** Generate the syntactic predicate.  This basically generates
     * the alternative block, buts tracks if we are inside a synPred
     * @param blk  The syntactic predicate block
     */
    protected void genSynPred(SynPredBlock blk) {
        // no op
    }

    public void genTail() {
        println("</chapter>");
        println("</book>");
    }

    /** Generate the token types TXT file */
    protected void genTokenTypes(TokenManager tm) throws IOException {
        // Open the token output TXT file and set the currentOutput stream
        antlrTool.reportProgress("Generating " + tm.getName() + TokenTypesFileSuffix + TokenTypesFileExt);
        currentOutput = antlrTool.openOutputFile(tm.getName() + TokenTypesFileSuffix + TokenTypesFileExt);
        //SAS: changed for proper text file io
        tabs = 0;

        // Generate the header common to all diagnostic files
        genHeader();

        // Generate a string for each token.  This creates a static
        // array of Strings indexed by token type.
        println("");
        println("*** Tokens used by the parser");
        println("This is a list of the token numeric values and the corresponding");
        println("token identifiers.  Some tokens are literals, and because of that");
        println("they have no identifiers.  Literals are double-quoted.");
        tabs++;

        // Enumerate all the valid token types
        Vector v = tm.getVocabulary();
        for (int i = Token.MIN_USER_TYPE; i < v.size(); i++) {
            String s = (String)v.elementAt(i);
            if (s != null) {
                println(s + " = " + i);
            }
        }

        // Close the interface
        tabs--;
        println("*** End of tokens used by the parser");

        // Close the tokens output file
        currentOutput.close();
        currentOutput = null;
    }

    /// unused.
    protected String processActionForSpecialSymbols(String actionStr,
                                                    int line,
                                                    RuleBlock currentRule,
                                                    ActionTransInfo tInfo) {
        return actionStr;
    }

    /** Get a string for an expression to generate creation of an AST subtree.
     * @param v A Vector of String, where each element is an expression in the target language yielding an AST node.
     */
    public String getASTCreateString(Vector v) {
        return null;
    }

    /** Get a string for an expression to generate creating of an AST node
     * @param str The arguments to the AST constructor
     */
    public String getASTCreateString(GrammarAtom atom, String str) {
        return null;
    }

    /** Map an identifier to it's corresponding tree-node variable.
     * This is context-sensitive, depending on the rule and alternative
     * being generated
     * @param id The identifier name to map
     * @param forInput true if the input tree node variable is to be returned, otherwise the output variable is returned.
     */
    public String mapTreeId(String id, ActionTransInfo tInfo) {
        return id;
    }

    /** Format a lookahead or follow set.
     * @param depth The depth of the entire lookahead/follow
     * @param k The lookahead level to print
     * @param lookahead  The lookahead/follow set to print
     */
    public void printSet(int depth, int k, Lookahead lookahead) {
        int numCols = 5;

        int[] elems = lookahead.fset.toArray();

        if (depth != 1) {
            print("k==" + k + ": {");
        }
        else {
            print("{ ");
        }
        if (elems.length > numCols) {
            _println("");
            tabs++;
            print("");
        }

        int column = 0;
        for (int i = 0; i < elems.length; i++) {
            column++;
            if (column > numCols) {
                _println("");
                print("");
                column = 0;
            }
            if (doingLexRules) {
                _print(charFormatter.literalChar(elems[i]));
            }
            else {
                _print((String)grammar.tokenManager.getVocabulary().elementAt(elems[i]));
            }
            if (i != elems.length - 1) {
                _print(", ");
            }
        }

        if (elems.length > numCols) {
            _println("");
            tabs--;
            print("");
        }
        _println(" }");
    }
}
