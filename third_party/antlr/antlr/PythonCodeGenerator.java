// This file is part of PyANTLR. See LICENSE.txt for license
// details..........Copyright (C) Wolfgang Haefelinger, 2004.
//
// $Id:$

package antlr;

import java.util.Enumeration;
import java.util.Hashtable;

import antlr.collections.impl.BitSet;
import antlr.collections.impl.Vector;

import java.io.PrintWriter; //SAS: changed for proper text file io
import java.io.IOException;
import java.io.FileWriter;

/**Generate MyParser.java, MyLexer.java and MyParserTokenTypes.java */
public class PythonCodeGenerator extends CodeGenerator {
  
  // non-zero if inside syntactic predicate generation
  protected int syntacticPredLevel = 0;

  // Are we generating ASTs (for parsers and tree parsers) right now?
  protected boolean genAST = false;

  // Are we saving the text consumed (for lexers) right now?
  protected boolean saveText = false;

  // Grammar parameters set up to handle different grammar classes.
  // These are used to get instanceof tests out of code generation
  String labeledElementType;
  String labeledElementASTType;
  String labeledElementInit;
  String commonExtraArgs;
  String commonExtraParams;
  String commonLocalVars;
  String lt1Value;
  String exceptionThrown;
  String throwNoViable;

  public static final String initHeaderAction = "__init__";
  public static final String mainHeaderAction = "__main__";

  String lexerClassName;
  String parserClassName;
  String treeWalkerClassName;

  /** Tracks the rule being generated.  Used for mapTreeId */
  RuleBlock currentRule;

  /** Tracks the rule or labeled subrule being generated.  Used for
      AST generation. */
  String currentASTResult;

  /** Mapping between the ids used in the current alt, and the
   * names of variables used to represent their AST values.
   */
  Hashtable treeVariableMap = new Hashtable();

  /** Used to keep track of which AST variables have been defined in a rule
   * (except for the #rule_name and #rule_name_in var's
   */
  Hashtable declaredASTVariables = new Hashtable();

  /* Count of unnamed generated variables */
  int astVarNumber = 1;

  /** Special value used to mark duplicate in treeVariableMap */
  protected static final String NONUNIQUE = new String();

  public static final int caseSizeThreshold = 127; // ascii is max

  private Vector semPreds;

  /** Create a Java code-generator using the given Grammar.
   * The caller must still call setTool, setBehavior, and setAnalyzer
   * before generating code.
   */

  
  protected void printTabs() {
    for (int i = 0; i < tabs; i++) {
      // don't print tabs ever - replace a tab by '    '
      currentOutput.print("    ");
    }
  }

  public PythonCodeGenerator() {
    super();
    charFormatter = new antlr.PythonCharFormatter();
    DEBUG_CODE_GENERATOR = true;
  }

  /** Adds a semantic predicate string to the sem pred vector
      These strings will be used to build an array of sem pred names
      when building a debugging parser.  This method should only be
      called when the debug option is specified
  */
  protected int addSemPred(String predicate) {
    semPreds.appendElement(predicate);
    return semPreds.size() - 1;
  }

  public void exitIfError() {
    if (antlrTool.hasError()) {
      antlrTool.fatalError("Exiting due to errors.");
    }
  }

  protected void checkCurrentOutputStream() {
    try
    {
      if(currentOutput == null)
        throw new NullPointerException();
    }
    catch(Exception e)
    {
      Utils.error("current output is not set");
    }
  }
      
  /** Get the identifier portion of an argument-action.
   * For Python the ID of an action is assumed to be everything before
   * the assignment, as Python does not support a type.
   * @param s The action text
   * @param line Line used for error reporting.
   * @param column Line used for error reporting.
   * @return A string containing the text of the identifier
   */
  protected String extractIdOfAction(String s, int line, int column) {
    s = removeAssignmentFromDeclaration(s);
    //wh: removeAssignmentFromDeclaration returns an indentifier that
    //wh: may start with whitespace.
    s = s.trim();
    // println("###ZZZZZ \""+s+"\"");
    return s;
  }


  /** Get the type portion of an argument-action.
   * Python does not have a type  declaration before an identifier, so we
   * just return the empty string.
   * @param s The action text
   * @param line Line used for error reporting.
   * @return A string containing the text of the type
   */
  protected String extractTypeOfAction(String s, int line, int column) {
    return "";
  }

  protected void flushTokens() {
    try
    {
      boolean generated = false;

      checkCurrentOutputStream();

      println("");
      println("### import antlr.Token ");
      println("from antlr import Token");
      println("### >>>The Known Token Types <<<");


      /* save current stream */
      PrintWriter cout = currentOutput;

      // Loop over all token managers (some of which are lexers)
      Enumeration tmIter = 
        behavior.tokenManagers.elements();

      while (tmIter.hasMoreElements()) 
      {
        TokenManager tm = 
          (TokenManager)tmIter.nextElement();

        if (!tm.isReadOnly()) 
        {
          // Write the token manager tokens as Java
          // this must appear before genTokenInterchange so that
          // labels are set on string literals
          if(! generated) {
            genTokenTypes(tm);
            generated = true;
          }

          /* restore stream */
          currentOutput = cout;

          // Write the token manager tokens as plain text
          genTokenInterchange(tm);
          currentOutput = cout;

        }
        exitIfError();
      }
    }
    catch(Exception e) {
      exitIfError();
    }
    checkCurrentOutputStream();
    println("");
  }


  /**Generate the parser, lexer, treeparser, and token types in Java */
  public void gen() {
    // Do the code generation
    try {
      // Loop over all grammars
      Enumeration grammarIter = behavior.grammars.elements();
      while (grammarIter.hasMoreElements()) {
        Grammar g = (Grammar)grammarIter.nextElement();
        // Connect all the components to each other
        g.setGrammarAnalyzer(analyzer);
        g.setCodeGenerator(this);
        analyzer.setGrammar(g);
        // To get right overloading behavior across hetrogeneous grammars
        setupGrammarParameters(g);
        g.generate();
        // print out the grammar with lookahead sets (and FOLLOWs)
        // System.out.print(g.toString());
        exitIfError();
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
    if (action.isSemPred) {
      genSemPred(action.actionText, action.line);
    }
    else 
    {
      if (grammar.hasSyntacticPredicate) {
        println("if not self.inputState.guessing:");
        tabs++;
      }

      // get the name of the followSet for the current rule so that we
      // can replace $FOLLOW in the .g file.
      ActionTransInfo tInfo = new ActionTransInfo();
      String actionStr = processActionForSpecialSymbols(action.actionText,
                                                        action.getLine(),
                                                        currentRule,
                                                        tInfo);

      if (tInfo.refRuleRoot != null) {
        // Somebody referenced "#rule", make sure translated var is valid
        // assignment to #rule is left as a ref also, meaning that assignments
        // with no other refs like "#rule = foo();" still forces this code to be
        // generated (unnecessarily).
        println(tInfo.refRuleRoot + " = currentAST.root");
      }

      // dump the translated action
      printAction(actionStr);

      if (tInfo.assignToRoot) {
        // Somebody did a "#rule=", reset internal currentAST.root
        println("currentAST.root = " + tInfo.refRuleRoot + "");

        println("if (" + tInfo.refRuleRoot + " != None) and (" + tInfo.refRuleRoot + ".getFirstChild() != None):");
        tabs++;
        println("currentAST.child = " + tInfo.refRuleRoot + ".getFirstChild()");
        tabs--;
        println("else:");
        tabs++;
        println("currentAST.child = " + tInfo.refRuleRoot);
        tabs--;
        println("currentAST.advanceChildToEnd()");
      }

      if (grammar.hasSyntacticPredicate) {
        tabs--;
      }
    }
  }

  /** Generate code for the given grammar element.
   * @param blk The "x|y|z|..." block to generate
   */
  public void gen(AlternativeBlock blk) {
    if (DEBUG_CODE_GENERATOR) System.out.println("gen(" + blk + ")");
    genBlockPreamble(blk);
    genBlockInitAction(blk);
    
    // Tell AST generation to build subrule result
    String saveCurrentASTResult = currentASTResult;
    if (blk.getLabel() != null) {
      currentASTResult = blk.getLabel();
    }

    boolean ok = grammar.theLLkAnalyzer.deterministic(blk);

    {
      int _tabs_ = tabs;
      PythonBlockFinishingInfo howToFinish = genCommonBlock(blk, true);
      genBlockFinish(howToFinish, throwNoViable);
      tabs = _tabs_;
    }

    // Restore previous AST generation
    currentASTResult = saveCurrentASTResult;
  }

  /** Generate code for the given grammar element.
   * @param blk The block-end element to generate.  Block-end
   * elements are synthesized by the grammar parser to represent
   * the end of a block.
   */
  public void gen(BlockEndElement end) {
    if (DEBUG_CODE_GENERATOR) System.out.println("genRuleEnd(" + end + ")");
  }

  /** Generate code for the given grammar element.
   * @param blk The character literal reference to generate
   */
  public void gen(CharLiteralElement atom) {
    if (DEBUG_CODE_GENERATOR) System.out.println("genChar(" + atom + ")");

    if (atom.getLabel() != null) {
      println(atom.getLabel() + " = " + lt1Value );
    }

    boolean oldsaveText = saveText;
    saveText = saveText && atom.getAutoGenType() == GrammarElement.AUTO_GEN_NONE;
    genMatch(atom);
    saveText = oldsaveText;
  }


  String toString(boolean v) {
    String s;
    if(v) 
      s = "True";
    else
      s = "False";
    return s;
  }
  

  /** Generate code for the given grammar element.
   * @param blk The character-range reference to generate
   */
  public void gen(CharRangeElement r) {
    if (r.getLabel() != null && syntacticPredLevel == 0) {
      println(r.getLabel() + " = " + lt1Value);
    }
    boolean flag = ( grammar instanceof LexerGrammar &&
                     ( !saveText ||
                       r.getAutoGenType() ==
                       GrammarElement.AUTO_GEN_BANG ) );
    if (flag) {
      println("_saveIndex = self.text.length()");
    }

    println("self.matchRange(u" + r.beginText + ", u" + r.endText + ")");

    if (flag) {
      println("self.text.setLength(_saveIndex)");
    }
  }

  /** Generate the lexer Java file */
  public void gen(LexerGrammar g) throws IOException 
  {
    // If debugging, create a new sempred vector for this grammar
    if (g.debuggingOutput)
      semPreds = new Vector();

    setGrammar(g);
    if (!(grammar instanceof LexerGrammar)) {
      antlrTool.panic("Internal error generating lexer");
    }

    // SAS: moved output creation to method so a subclass can change
    //      how the output is generated (for VAJ interface)
    setupOutput(grammar.getClassName());

    genAST = false; // no way to gen trees.
    saveText = true;  // save consumed characters.

    tabs = 0;

    // Generate header common to all Python output files
    genHeader();

    // Generate header specific to lexer Python file
    println("### import antlr and other modules ..");
    println("import sys");
    println("import antlr");
    println("");
    println("version = sys.version.split()[0]");
    println("if version < '2.2.1':");
    tabs++;
    println("False = 0");
    tabs--;
    println("if version < '2.3':");
    tabs++;
    println("True = not False");
    tabs--;

    println("### header action >>> ");
    printActionCode(behavior.getHeaderAction(""),0);
    println("### header action <<< ");
   
    // Generate user-defined lexer file preamble
    println("### preamble action >>> ");
    printActionCode(grammar.preambleAction.getText(),0);
    println("### preamble action <<< ");

    // Generate lexer class definition
    String sup = null;
    if (grammar.superClass != null) {
      sup = grammar.superClass;
    }
    else {
      sup = "antlr." + grammar.getSuperClass();
    }

    // get prefix (replaces "public" and lets user specify)
    String prefix = "";
    Token tprefix = (Token)grammar.options.get("classHeaderPrefix");
    if (tprefix != null) {
      String p = StringUtils.stripFrontBack(tprefix.getText(), "\"", "\"");
      if (p != null) {
        prefix = p;
      }
    }

    // print my literals 
    println("### >>>The Literals<<<");
    println("literals = {}");
    Enumeration keys = grammar.tokenManager.getTokenSymbolKeys();
    while (keys.hasMoreElements()) {
      String key = (String)keys.nextElement();
      if (key.charAt(0) != '"') {
        continue;
      }
      TokenSymbol sym = grammar.tokenManager.getTokenSymbol(key);
      if (sym instanceof StringLiteralSymbol) {
        StringLiteralSymbol s = (StringLiteralSymbol)sym;
        println("literals[u" + s.getId() + "] = " + s.getTokenType());
      }
    }
    println("");
    flushTokens();

    // print javadoc comment if any
    genJavadocComment(grammar);

    // class name remains the same, it's the module that changes in python.
    println("class " + lexerClassName + "(" + sup + ") :");
    tabs++;
    
    printGrammarAction(grammar);

    // Generate the constructor from InputStream, which in turn
    // calls the ByteBuffer constructor
    //

    println("def __init__(self, *argv, **kwargs) :"); 
    tabs++;
    println(sup + ".__init__(self, *argv, **kwargs)");

    // Generate the setting of various generated options.
    // These need to be before the literals since ANTLRHashString depends on
    // the casesensitive stuff.
    println("self.caseSensitiveLiterals = " + toString(g.caseSensitiveLiterals));
    println("self.setCaseSensitive(" + toString(g.caseSensitive) + ")" );
    println("self.literals = literals");
    Enumeration ids;

    // generate the rule name array for debugging
    if (grammar.debuggingOutput) {
      println("ruleNames[] = [");
      ids = grammar.rules.elements();
      int ruleNum = 0;
      tabs++;
      while (ids.hasMoreElements()) {
        GrammarSymbol sym = (GrammarSymbol)ids.nextElement();
        if (sym instanceof RuleSymbol)
          println("\"" + ((RuleSymbol)sym).getId() + "\",");
      }
      tabs--;
      println("]");
    }
    
    genHeaderInit(grammar);

    tabs--;
    // wh: iterator moved to base class as proposed by mk.
    // println("");
    // Generate the __iter__ method for Python CharScanner (sub)classes.
    // genIterator();

    // Generate nextToken() rule.
    // nextToken() is a synthetic lexer rule that is the implicit OR of all
    // user-defined lexer rules.
    genNextToken();
    println("");

    // Generate code for each rule in the lexer
    ids = grammar.rules.elements();
    int ruleNum = 0;
    while (ids.hasMoreElements()) {
      RuleSymbol sym = (RuleSymbol)ids.nextElement();
      // Don't generate the synthetic rules
      if (!sym.getId().equals("mnextToken")) {
        genRule(sym, false, ruleNum++);
      }
      exitIfError();
    }

    // Generate the semantic predicate map for debugging
    if (grammar.debuggingOutput)
      genSemPredMap();

    // Generate the bitsets used throughout the lexer
    genBitsets(bitsetsUsed, ((LexerGrammar)grammar).charVocabulary.size());
    println("");

    genHeaderMain(grammar);

    // Close the lexer output stream
    currentOutput.close();
    currentOutput = null;
  }

  protected void genHeaderMain(Grammar grammar) 
  {
    String h = grammar.getClassName() + "." + mainHeaderAction;
    String s = behavior.getHeaderAction(h);

    if (isEmpty(s)) {
      s = behavior.getHeaderAction(mainHeaderAction);
    }
    if(isEmpty(s)) {
      if(grammar instanceof LexerGrammar) {
        int _tabs = tabs;
        tabs = 0;
        println("### __main__ header action >>> ");
        genLexerTest();
        tabs = 0;
        println("### __main__ header action <<< ");
        tabs = _tabs;
      }
    } else {
      int _tabs = tabs;
      tabs = 0;
      println("");
      println("### __main__ header action >>> ");
      printMainFunc(s);
      tabs = 0;
      println("### __main__ header action <<< ");
      tabs = _tabs;
    }
  }

  protected void genHeaderInit(Grammar grammar)
  {
    String h = grammar.getClassName() + "." + initHeaderAction;
    String s = behavior.getHeaderAction(h);
    
    if (isEmpty(s)) {
      s = behavior.getHeaderAction(initHeaderAction);
    }
    if(isEmpty(s)) {
      /* nothing gets generated by default */
    } else {
      int _tabs = tabs;
      println("### __init__ header action >>> ");
      printActionCode(s,0);
      tabs = _tabs;
      println("### __init__ header action <<< ");
    }
  }
    
  protected void printMainFunc(String s) {
    int _tabs = tabs;
    tabs = 0;
    println("if __name__ == '__main__':");
    tabs++;
    printActionCode(s,0);
    tabs--;
    tabs = _tabs;
  }

  /** Generate code for the given grammar element.
   * @param blk The (...)+ block to generate
   */
  public void gen(OneOrMoreBlock blk) {
    String label;
    String cnt;
    /* save current tabs */
    int _tabs_ = tabs;

    genBlockPreamble(blk);

    if (blk.getLabel() != null) 
    {
      cnt = "_cnt_" + blk.getLabel();
    }
    else {
      cnt = "_cnt" + blk.ID;
    }
    println("" + cnt + "= 0");
    println("while True:");
    tabs++;
    _tabs_ = tabs;
    // generate the init action for ()+ ()* inside the loop
    // this allows us to do usefull EOF checking...
    genBlockInitAction(blk);

    // Tell AST generation to build subrule result
    String saveCurrentASTResult = currentASTResult;
    if (blk.getLabel() != null) {
      currentASTResult = blk.getLabel();
    }

    boolean ok = grammar.theLLkAnalyzer.deterministic(blk);

    // generate exit test if greedy set to false
    // and an alt is ambiguous with exit branch
    // or when lookahead derived purely from end-of-file
    // Lookahead analysis stops when end-of-file is hit,
    // returning set {epsilon}.  Since {epsilon} is not
    // ambig with any real tokens, no error is reported
    // by deterministic() routines and we have to check
    // for the case where the lookahead depth didn't get
    // set to NONDETERMINISTIC (this only happens when the
    // FOLLOW contains real atoms + epsilon).
    boolean generateNonGreedyExitPath = false;
    int nonGreedyExitDepth = grammar.maxk;

    if (!blk.greedy &&
        blk.exitLookaheadDepth <= grammar.maxk &&
        blk.exitCache[blk.exitLookaheadDepth].containsEpsilon()) 
    {
      generateNonGreedyExitPath = true;
      nonGreedyExitDepth = blk.exitLookaheadDepth;
    }
    else 
    {
      if (!blk.greedy &&
          blk.exitLookaheadDepth == LLkGrammarAnalyzer.NONDETERMINISTIC) {
        generateNonGreedyExitPath = true;
      }
    }

    // generate exit test if greedy set to false
    // and an alt is ambiguous with exit branch
    if (generateNonGreedyExitPath) 
    {
      println("### nongreedy (...)+ loop; exit depth is " + blk.exitLookaheadDepth);

      String predictExit =
        getLookaheadTestExpression(
          blk.exitCache,
          nonGreedyExitDepth);
      
      println("### nongreedy exit test");
      println("if " + cnt + " >= 1 and " + predictExit + ":");
      tabs++;
      println("break");
      tabs--;
    }

    {
      int _tabs = tabs;
      PythonBlockFinishingInfo howToFinish = genCommonBlock(blk, false);
      genBlockFinish(howToFinish, "break");
      tabs = _tabs;
    }
    /* no matter what previous block did, here we have to continue
    ** one the 'while block' level. Reseting tabs .. */
    tabs = _tabs_;
    println(cnt + " += 1");
    tabs = _tabs_;
    tabs--;
    println("if " + cnt + " < 1:");
    tabs++;
    println(throwNoViable);
    tabs--;
    // Restore previous AST generation
    currentASTResult = saveCurrentASTResult;
  }

  /** Generate the parser Java file */
  public void gen(ParserGrammar g) 
    throws IOException {

    // if debugging, set up a new vector to keep track of sempred
    //   strings for this grammar
    if (g.debuggingOutput)
      semPreds = new Vector();

    setGrammar(g);
    if (!(grammar instanceof ParserGrammar)) {
      antlrTool.panic("Internal error generating parser");
    }

    // Open the output stream for the parser and set the currentOutput
    // SAS: moved file setup so subclass could do it (for VAJ interface)
    setupOutput(grammar.getClassName());

    genAST = grammar.buildAST;

    tabs = 0;

    // Generate the header common to all output files.
    genHeader();

    // Generate header specific to lexer Java file
    println("### import antlr and other modules ..");
    println("import sys");
    println("import antlr");
    println("");
    println("version = sys.version.split()[0]");
    println("if version < '2.2.1':");
    tabs++;
    println("False = 0");
    tabs--;
    println("if version < '2.3':");
    tabs++;
    println("True = not False");
    tabs--;

    println("### header action >>> ");
    printActionCode(behavior.getHeaderAction(""),0);
    println("### header action <<< ");

    println("### preamble action>>>");
    // Output the user-defined parser preamble
    printActionCode(grammar.preambleAction.getText(),0);
    println("### preamble action <<<");
    
    flushTokens();

    // Generate parser class definition
    String sup = null;
    if (grammar.superClass != null)
      sup = grammar.superClass;
    else
      sup = "antlr." + grammar.getSuperClass();

    // print javadoc comment if any
    genJavadocComment(grammar);

    // get prefix (replaces "public" and lets user specify)
    String prefix = "";
    Token tprefix = (Token)grammar.options.get("classHeaderPrefix");
    if (tprefix != null) {
      String p = StringUtils.stripFrontBack(tprefix.getText(), "\"", "\"");
      if (p != null) {
        prefix = p;
      }
    }

    print("class " + parserClassName + "(" + sup);
    println("):");
    tabs++;

    // set up an array of all the rule names so the debugger can
    // keep track of them only by number -- less to store in tree...
    if (grammar.debuggingOutput) {
      println("_ruleNames = [");

      Enumeration ids = grammar.rules.elements();
      int ruleNum = 0;
      tabs++;
      while (ids.hasMoreElements()) {
        GrammarSymbol sym = (GrammarSymbol)ids.nextElement();
        if (sym instanceof RuleSymbol)
          println("\"" + ((RuleSymbol)sym).getId() + "\",");
      }
      tabs--;
      println("]");
    }

    // Generate user-defined parser class members
    printGrammarAction(grammar);

    // Generate parser class constructor from TokenBuffer
    println("");
    println("def __init__(self, *args, **kwargs):");
    tabs++;
    println(sup + ".__init__(self, *args, **kwargs)");
    println("self.tokenNames = _tokenNames");
    // if debugging, set up arrays and call the user-overridable
    //   debugging setup method
    if (grammar.debuggingOutput) {
      println("self.ruleNames  = _ruleNames");
      println("self.semPredNames = _semPredNames");
      println("self.setupDebugging(self.tokenBuf)");
    }
    if (grammar.buildAST) {
      println("self.buildTokenTypeASTClassMap()");
      println("self.astFactory = antlr.ASTFactory(self.getTokenTypeToASTClassMap())");
      if(labeledElementASTType != null) 
      {
        println("self.astFactory.setASTNodeClass("+
                labeledElementASTType+")");
      }
    }
    genHeaderInit(grammar);
    println("");

    // Generate code for each rule in the grammar
    Enumeration ids = grammar.rules.elements();
    int ruleNum = 0;
    while (ids.hasMoreElements()) {
      GrammarSymbol sym = (GrammarSymbol)ids.nextElement();
      if (sym instanceof RuleSymbol) {
        RuleSymbol rs = (RuleSymbol)sym;
        genRule(rs, rs.references.size() == 0, ruleNum++);
      }
      exitIfError();
    }


    if ( grammar.buildAST ) {
      genTokenASTNodeMap();
    }
    
    // Generate the token names
    genTokenStrings();

    // Generate the bitsets used throughout the grammar
    genBitsets(bitsetsUsed, grammar.tokenManager.maxTokenType());

    // Generate the semantic predicate map for debugging
    if (grammar.debuggingOutput)
      genSemPredMap();

    // Close class definition
    println("");

    tabs = 0;
    genHeaderMain(grammar);
    // Close the parser output stream
    currentOutput.close();
    currentOutput = null;
  }

  /** Generate code for the given grammar element.
   * @param blk The rule-reference to generate
   */
  public void gen(RuleRefElement rr) {
    if (DEBUG_CODE_GENERATOR) System.out.println("genRR(" + rr + ")");
    RuleSymbol rs = (RuleSymbol)grammar.getSymbol(rr.targetRule);
    if (rs == null || !rs.isDefined()) {
      // Is this redundant???
      antlrTool.error("Rule '" + rr.targetRule + "' is not defined", grammar.getFilename(), rr.getLine(), rr.getColumn());
      return;
    }
    if (!(rs instanceof RuleSymbol)) {
      // Is this redundant???
      antlrTool.error("'" + rr.targetRule + "' does not name a grammar rule", grammar.getFilename(), rr.getLine(), rr.getColumn());
      return;
    }

    genErrorTryForElement(rr);

    // AST value for labeled rule refs in tree walker.
    // This is not AST construction;  it is just the input tree node value.
    if (grammar instanceof TreeWalkerGrammar &&
        rr.getLabel() != null &&
        syntacticPredLevel == 0) {
      println(rr.getLabel() + " = antlr.ifelse(_t == antlr.ASTNULL, None, " + lt1Value + ")");
    }

    // if in lexer and ! on rule ref or alt or rule, save buffer index to kill later
    if (grammar instanceof LexerGrammar && (!saveText || rr.getAutoGenType() == GrammarElement.AUTO_GEN_BANG)) {
      println("_saveIndex = self.text.length()");
    }

    // Process return value assignment if any
    printTabs();
    if (rr.idAssign != null) {
      // Warn if the rule has no return type
      if (rs.block.returnAction == null) {
        antlrTool.warning("Rule '" + rr.targetRule + "' has no return type", grammar.getFilename(), rr.getLine(), rr.getColumn());
      }
      _print(rr.idAssign + "=");
    }
    else {
      // Warn about return value if any, but not inside syntactic predicate
      if (!(grammar instanceof LexerGrammar) && syntacticPredLevel == 0 && rs.block.returnAction != null) {
        antlrTool.warning("Rule '" + rr.targetRule + "' returns a value", grammar.getFilename(), rr.getLine(), rr.getColumn());
      }
    }

    // Call the rule
    GenRuleInvocation(rr);

    // if in lexer and ! on element or alt or rule, save buffer index to kill later
    if (grammar instanceof LexerGrammar && (!saveText || rr.getAutoGenType() == GrammarElement.AUTO_GEN_BANG)) {
      println("self.text.setLength(_saveIndex)");
    }

    // if not in a syntactic predicate
    if (syntacticPredLevel == 0) {
      boolean doNoGuessTest = (
        grammar.hasSyntacticPredicate && (
          grammar.buildAST && rr.getLabel() != null ||
          (genAST && rr.getAutoGenType() == GrammarElement.AUTO_GEN_NONE)
          )
        );
      if (doNoGuessTest) {
        // println("if (inputState.guessing==0) {");
        // tabs++;
      }

      if (grammar.buildAST && rr.getLabel() != null) {
        // always gen variable for rule return on labeled rules
        println(rr.getLabel() + "_AST = self.returnAST");
      }
      if (genAST) {
        switch (rr.getAutoGenType()) {
          case GrammarElement.AUTO_GEN_NONE:
            println("self.addASTChild(currentAST, self.returnAST)");
            break;
          case GrammarElement.AUTO_GEN_CARET:
            antlrTool.error("Internal: encountered ^ after rule reference");
            break;
          default:
            break;
        }
      }

      // if a lexer and labeled, Token label defined at rule level, just set it here
      if (grammar instanceof LexerGrammar && rr.getLabel() != null) {
        println(rr.getLabel() + " = self._returnToken");
      }

      if (doNoGuessTest) {
      }
    }
    genErrorCatchForElement(rr);
  }

  /** Generate code for the given grammar element.
   * @param blk The string-literal reference to generate
   */
  public void gen(StringLiteralElement atom) {
    if (DEBUG_CODE_GENERATOR) System.out.println("genString(" + atom + ")");

    // Variable declarations for labeled elements
    if (atom.getLabel() != null && syntacticPredLevel == 0) {
      println(atom.getLabel() + " = " + lt1Value + "");
    }

    // AST
    genElementAST(atom);

    // is there a bang on the literal?
    boolean oldsaveText = saveText;
    saveText = saveText && atom.getAutoGenType() == GrammarElement.AUTO_GEN_NONE;

    // matching
    genMatch(atom);

    saveText = oldsaveText;

    // tack on tree cursor motion if doing a tree walker
    if (grammar instanceof TreeWalkerGrammar) {
      println("_t = _t.getNextSibling()");
    }
  }

  /** Generate code for the given grammar element.
   * @param blk The token-range reference to generate
   */
  public void gen(TokenRangeElement r) {
    genErrorTryForElement(r);
    if (r.getLabel() != null && syntacticPredLevel == 0) {
      println(r.getLabel() + " = " + lt1Value);
    }

    // AST
    genElementAST(r);

    // match
    println("self.matchRange(u" + r.beginText + ", u" + r.endText + ")");
    genErrorCatchForElement(r);
  }

  /** Generate code for the given grammar element.
   * @param blk The token-reference to generate
   */
  public void gen(TokenRefElement atom) {
    if (DEBUG_CODE_GENERATOR) System.out.println("genTokenRef(" + atom + ")");
    if (grammar instanceof LexerGrammar) {
      antlrTool.panic("Token reference found in lexer");
    }
    genErrorTryForElement(atom);
    // Assign Token value to token label variable
    if (atom.getLabel() != null && syntacticPredLevel == 0) {
      println(atom.getLabel() + " = " + lt1Value + "");
    }

    // AST
    genElementAST(atom);
    // matching
    genMatch(atom);
    genErrorCatchForElement(atom);

    // tack on tree cursor motion if doing a tree walker
    if (grammar instanceof TreeWalkerGrammar) {
      println("_t = _t.getNextSibling()");
    }
  }

  public void gen(TreeElement t) {
    // save AST cursor
    println("_t" + t.ID + " = _t");

    // If there is a label on the root, then assign that to the variable
    if (t.root.getLabel() != null) {
      println(t.root.getLabel() + " = antlr.ifelse(_t == antlr.ASTNULL, None, _t)");
    }

    // check for invalid modifiers ! and ^ on tree element roots
    if ( t.root.getAutoGenType() == GrammarElement.AUTO_GEN_BANG ) {
      antlrTool.error("Suffixing a root node with '!' is not implemented",
                      grammar.getFilename(), t.getLine(), t.getColumn());
      t.root.setAutoGenType(GrammarElement.AUTO_GEN_NONE);
    }
    if ( t.root.getAutoGenType() == GrammarElement.AUTO_GEN_CARET ) {
      antlrTool.warning("Suffixing a root node with '^' is redundant; already a root",
                        grammar.getFilename(), t.getLine(), t.getColumn());
      t.root.setAutoGenType(GrammarElement.AUTO_GEN_NONE);
    }

    // Generate AST variables
    genElementAST(t.root);
    if (grammar.buildAST) {
      // Save the AST construction state
      println("_currentAST" + t.ID + " = currentAST.copy()");
      // Make the next item added a child of the TreeElement root
      println("currentAST.root = currentAST.child");
      println("currentAST.child = None");
    }

    // match root
    if ( t.root instanceof WildcardElement ) {
      println("if not _t: raise antlr.MismatchedTokenException()");
    }
    else {
      genMatch(t.root);
    }
    // move to list of children
    println("_t = _t.getFirstChild()");

    // walk list of children, generating code for each
    for (int i = 0; i < t.getAlternatives().size(); i++) {
      Alternative a = t.getAlternativeAt(i);
      AlternativeElement e = a.head;
      while (e != null) {
        e.generate();
        e = e.next;
      }
    }

    if (grammar.buildAST) {
      // restore the AST construction state to that just after the
      // tree root was added
      println("currentAST = _currentAST" + t.ID + "");
    }
    // restore AST cursor
    println("_t = _t" + t.ID + "");
    // move cursor to sibling of tree just parsed
    println("_t = _t.getNextSibling()");
  }



  /** Generate the tree-parser Java file */
  public void gen(TreeWalkerGrammar g) throws IOException {
    // SAS: debugging stuff removed for now...
    setGrammar(g);
    if (!(grammar instanceof TreeWalkerGrammar)) {
      antlrTool.panic("Internal error generating tree-walker");
    }
    // Open the output stream for the parser and set the currentOutput
    // SAS: move file open to method so subclass can override it
    //      (mainly for VAJ interface)
    setupOutput(grammar.getClassName());

    genAST = grammar.buildAST;
    tabs = 0;

    // Generate the header common to all output files.
    genHeader();

    // Generate header specific to lexer Java file
    println("### import antlr and other modules ..");
    println("import sys");
    println("import antlr");
    println("");
    println("version = sys.version.split()[0]");
    println("if version < '2.2.1':");
    tabs++;
    println("False = 0");
    tabs--;
    println("if version < '2.3':");
    tabs++;
    println("True = not False");
    tabs--;

    println("### header action >>> ");
    printActionCode(behavior.getHeaderAction(""),0);
    println("### header action <<< ");

    flushTokens();

    println("### user code>>>");
    // Output the user-defined parser preamble
    printActionCode(grammar.preambleAction.getText(),0);
    println("### user code<<<");

    // Generate parser class definition
    String sup = null;
    if (grammar.superClass != null) {
      sup = grammar.superClass;
    }
    else {
      sup = "antlr." + grammar.getSuperClass();
    }
    println("");

    // get prefix (replaces "public" and lets user specify)
    String prefix = "";
    Token tprefix = (Token)grammar.options.get("classHeaderPrefix");
    if (tprefix != null) {
      String p = StringUtils.stripFrontBack(tprefix.getText(), "\"", "\"");
      if (p != null) {
        prefix = p;
      }
    }
    
    // print javadoc comment if any
    genJavadocComment(grammar);

    println("class " + treeWalkerClassName + "(" + sup + "):");
    tabs++;
 
    // Generate default parser class constructor
    println("");
    println("# ctor ..");
    println("def __init__(self, *args, **kwargs):");
    tabs++;
    println(sup + ".__init__(self, *args, **kwargs)");
    println("self.tokenNames = _tokenNames");
    genHeaderInit(grammar);
    tabs--;
    println("");

    // print grammar specific action
    printGrammarAction(grammar);

    // Generate code for each rule in the grammar
    Enumeration ids = grammar.rules.elements();
    int ruleNum = 0;
    String ruleNameInits = "";
    while (ids.hasMoreElements()) {
      GrammarSymbol sym = (GrammarSymbol)ids.nextElement();
      if (sym instanceof RuleSymbol) {
        RuleSymbol rs = (RuleSymbol)sym;
        genRule(rs, rs.references.size() == 0, ruleNum++);
      }
      exitIfError();
    }

    // Generate the token names
    genTokenStrings();

    // Generate the bitsets used throughout the grammar
    genBitsets(bitsetsUsed, grammar.tokenManager.maxTokenType());
    
    tabs = 0;
    genHeaderMain(grammar);
    // Close the parser output stream
    currentOutput.close();
    currentOutput = null;
  }

  /** Generate code for the given grammar element.
   * @param wc The wildcard element to generate
   */
  public void gen(WildcardElement wc) {
    // Variable assignment for labeled elements
    if (wc.getLabel() != null && syntacticPredLevel == 0) {
      println(wc.getLabel() + " = " + lt1Value + "");
    }

    // AST
    genElementAST(wc);
    // Match anything but EOF
    if (grammar instanceof TreeWalkerGrammar) {
      println("if not _t:");
      tabs++;
      println("raise antlr.MismatchedTokenException()");
      tabs--;
    }
    else if (grammar instanceof LexerGrammar) {
      if (grammar instanceof LexerGrammar &&
          (!saveText || wc.getAutoGenType() == GrammarElement.AUTO_GEN_BANG)) {
        println("_saveIndex = self.text.length()");
      }
      println("self.matchNot(antlr.EOF_CHAR)");
      if (grammar instanceof LexerGrammar &&
          (!saveText || wc.getAutoGenType() == GrammarElement.AUTO_GEN_BANG)) {
        println("self.text.setLength(_saveIndex)"); // kill text atom put in buffer
      }
    }
    else {
      println("self.matchNot(" + getValueString(Token.EOF_TYPE,false) + ")");
    }

    // tack on tree cursor motion if doing a tree walker
    if (grammar instanceof TreeWalkerGrammar) {
      println("_t = _t.getNextSibling()");
    }
  }


  /** Generate code for the given grammar element.
   * @param blk The (...)* block to generate
   */
  public void gen(ZeroOrMoreBlock blk) {
    int _tabs_ = tabs;
    genBlockPreamble(blk);
    String label;
    println("while True:");
    tabs++;
    _tabs_ = tabs;
    // generate the init action for ()* inside the loop
    // this allows us to do usefull EOF checking...
    genBlockInitAction(blk);

    // Tell AST generation to build subrule result
    String saveCurrentASTResult = currentASTResult;
    if (blk.getLabel() != null) {
      currentASTResult = blk.getLabel();
    }

    boolean ok = grammar.theLLkAnalyzer.deterministic(blk);

    // generate exit test if greedy set to false
    // and an alt is ambiguous with exit branch
    // or when lookahead derived purely from end-of-file
    // Lookahead analysis stops when end-of-file is hit,
    // returning set {epsilon}.  Since {epsilon} is not
    // ambig with any real tokens, no error is reported
    // by deterministic() routines and we have to check
    // for the case where the lookahead depth didn't get
    // set to NONDETERMINISTIC (this only happens when the
    // FOLLOW contains real atoms + epsilon).
    boolean generateNonGreedyExitPath = false;
    int nonGreedyExitDepth = grammar.maxk;

    if (!blk.greedy &&
        blk.exitLookaheadDepth <= grammar.maxk &&
        blk.exitCache[blk.exitLookaheadDepth].containsEpsilon()) {
      generateNonGreedyExitPath = true;
      nonGreedyExitDepth = blk.exitLookaheadDepth;
    }
    else if (!blk.greedy &&
             blk.exitLookaheadDepth == LLkGrammarAnalyzer.NONDETERMINISTIC) {
      generateNonGreedyExitPath = true;
    }
    if (generateNonGreedyExitPath) {
      if (DEBUG_CODE_GENERATOR) {
        System.out.println("nongreedy (...)* loop; exit depth is " +
                           blk.exitLookaheadDepth);
      }
      String predictExit =
        getLookaheadTestExpression(blk.exitCache,
                                   nonGreedyExitDepth);
      println("###  nongreedy exit test");
      println("if (" + predictExit + "):");
      tabs++;
      println("break");
      tabs--;
    }

    {
      int _tabs = tabs;
      PythonBlockFinishingInfo howToFinish = genCommonBlock(blk, false);
      genBlockFinish(howToFinish, "break");
      tabs = _tabs;
    }
    tabs = _tabs_; /* no matter where we are */
    tabs--;

    // Restore previous AST generation
    currentASTResult = saveCurrentASTResult;
  }

  /** Generate an alternative.
   * @param alt  The alternative to generate
   * @param blk The block to which the alternative belongs
   */
  protected void genAlt(Alternative alt, AlternativeBlock blk) {
    // Save the AST generation state, and set it to that of the alt
    boolean savegenAST = genAST;
    genAST = genAST && alt.getAutoGen();

    boolean oldsaveTest = saveText;
    saveText = saveText && alt.getAutoGen();

    // Reset the variable name map for the alternative
    Hashtable saveMap = treeVariableMap;
    treeVariableMap = new Hashtable();

    // Generate try block around the alt for  error handling
    if (alt.exceptionSpec != null) {
      println("try:");
      tabs++;
    }

    println("pass"); // make sure that always something gets generated ..
    AlternativeElement elem = alt.head;
    while (!(elem instanceof BlockEndElement)) {
      elem.generate(); // alt can begin with anything. Ask target to gen.
      elem = elem.next;
    }

    if (genAST) {
      if (blk instanceof RuleBlock) {
        // Set the AST return value for the rule
        RuleBlock rblk = (RuleBlock)blk;
        if (grammar.hasSyntacticPredicate) {
        }
        println(rblk.getRuleName() + "_AST = currentAST.root");
        if (grammar.hasSyntacticPredicate) {
        }
      }
      else if (blk.getLabel() != null) {
        antlrTool.warning(
          "Labeled subrules not yet supported", 
          grammar.getFilename(), blk.getLine(), blk.getColumn());
      }
    }

    if (alt.exceptionSpec != null) {
      tabs--;
      genErrorHandler(alt.exceptionSpec);
    }

    genAST = savegenAST;
    saveText = oldsaveTest;

    treeVariableMap = saveMap;
  }

  /** Generate all the bitsets to be used in the parser or lexer
   * Generate the raw bitset data like "long _tokenSet1_data[] = {...}"
   * and the BitSet object declarations like "BitSet _tokenSet1 = new BitSet(_tokenSet1_data)"
   * Note that most languages do not support object initialization inside a
   * class definition, so other code-generators may have to separate the
   * bitset declarations from the initializations (e.g., put the initializations
   * in the generated constructor instead).
   * @param bitsetList The list of bitsets to generate.
   * @param maxVocabulary Ensure that each generated bitset can contain at least this value.
   */
  protected void genBitsets(Vector bitsetList,
                            int maxVocabulary
    ) {
    println("");
    for (int i = 0; i < bitsetList.size(); i++) {
      BitSet p = (BitSet)bitsetList.elementAt(i);
      // Ensure that generated BitSet is large enough for vocabulary
      p.growToInclude(maxVocabulary);
      genBitSet(p, i);
    }
  }

  /** Do something simple like:
   *  private static final long[] mk_tokenSet_0() {
   *    long[] data = { -2305839160922996736L, 63L, 16777216L, 0L, 0L, 0L };
   *    return data;
   *  }
   *  public static final BitSet _tokenSet_0 = new BitSet(mk_tokenSet_0());
   *
   *  Or, for large bitsets, optimize init so ranges are collapsed into loops.
   *  This is most useful for lexers using unicode.
   */
  private void genBitSet(BitSet p, int id) {
    int _tabs_ = tabs;
    // wanna have bitsets on module scope, so they are available
    // when module gets loaded.
    tabs = 0;

    println("");
    println("### generate bit set");
    println(
      "def mk" + getBitsetName(id) + "(): " );
    tabs++;
    int n = p.lengthInLongWords();
    if ( n<BITSET_OPTIMIZE_INIT_THRESHOLD ) 
    {
      println("### var1");
      println("data = [ " + p.toStringOfWords() + "]");
    }
    else 
    {
      // will init manually, allocate space then set values
      println("data = [0L] * " + n + " ### init list");

      long[] elems = p.toPackedArray();

      for (int i = 0; i < elems.length;) 
      {
        if ( elems[i]==0 ) 
        {
          // done automatically by Java, don't waste time/code
          i++;
          continue;
        }

        if ( (i+1)==elems.length || elems[i]!=elems[i+1] ) 
        {
          // last number or no run of numbers, just dump assignment
          println("data["+ i + "] ="  + elems[i] + "L");
          i++;
          continue;
        }

        // scan to find end of run
        int j;
        for (j = i + 1;j < elems.length && elems[j]==elems[i];j++)
        {}

        long e = elems[i];
        // E0007: fixed
        println("for x in xrange(" + i+", " + j + "):");
        tabs++;
        println("data[x] = " + e + "L");
        tabs--;
        i = j;
      }
    }

    println("return data");
    tabs--;

    // BitSet object
    println(
      getBitsetName(id) + " = antlr.BitSet(mk" + getBitsetName(id) + "())" );

    // restore tabs
    tabs = _tabs_;
  }

  private void genBlockFinish(PythonBlockFinishingInfo howToFinish,
                              String noViableAction) {
    if (howToFinish.needAnErrorClause &&
        (howToFinish.generatedAnIf || howToFinish.generatedSwitch)) {
      if (howToFinish.generatedAnIf) 
      {
        println("else:" );
      }
      tabs++;
      println(noViableAction);
      tabs--;
    }

    if (howToFinish.postscript != null) {
      println(howToFinish.postscript);
    }
  }

  /* just to be called by nextToken */
  private void genBlockFinish1(PythonBlockFinishingInfo howToFinish,
                               String noViableAction) {
    if (howToFinish.needAnErrorClause &&
        (howToFinish.generatedAnIf || howToFinish.generatedSwitch)) 
    {
      if (howToFinish.generatedAnIf) 
      {
        // tabs++;
        println("else:" );
      }
      tabs++;
      println(noViableAction);
      tabs--;
      if (howToFinish.generatedAnIf) 
      {
        // tabs--;
        // println("### tabs--");
      }
    }

    if (howToFinish.postscript != null) {
      println(howToFinish.postscript);
    }
  }

  /** Generate the init action for a block, which may be a RuleBlock or a
   * plain AlternativeBLock.
   * @blk The block for which the preamble is to be generated.
   */
  protected void genBlockInitAction(AlternativeBlock blk) {
    // dump out init action
    if (blk.initAction != null) {
      printAction(processActionForSpecialSymbols(blk.initAction, blk.getLine(), currentRule, null));
    }
  }

  /** Generate the header for a block, which may be a RuleBlock or a
   * plain AlternativeBLock.  This generates any variable declarations
   * and syntactic-predicate-testing variables.
   * @blk The block for which the preamble is to be generated.
   */
  protected void genBlockPreamble(AlternativeBlock blk) {
    // define labels for rule blocks.
    if (blk instanceof RuleBlock) {
      RuleBlock rblk = (RuleBlock)blk;
      if (rblk.labeledElements != null) {
        for (int i = 0; i < rblk.labeledElements.size(); i++) {
          AlternativeElement a = (AlternativeElement)rblk.labeledElements.elementAt(i);
          // System.out.println("looking at labeled element: "+a);
          // Variables for labeled rule refs and
          // subrules are different than variables for
          // grammar atoms.  This test is a little tricky
          // because we want to get all rule refs and ebnf,
          // but not rule blocks or syntactic predicates
          if (
            a instanceof RuleRefElement ||
            a instanceof AlternativeBlock &&
            !(a instanceof RuleBlock) &&
            !(a instanceof SynPredBlock)
            ) {

            if (
              !(a instanceof RuleRefElement) &&
              ((AlternativeBlock)a).not &&
              analyzer.subruleCanBeInverted(((AlternativeBlock)a), grammar instanceof LexerGrammar)
              ) {
              // Special case for inverted subrules that
              // will be inlined.  Treat these like
              // token or char literal references
              println(a.getLabel() + " = " + labeledElementInit);
              if (grammar.buildAST) {
                genASTDeclaration(a);
              }
            }
            else {
              if (grammar.buildAST) {
                // Always gen AST variables for
                // labeled elements, even if the
                // element itself is marked with !
                genASTDeclaration(a);
              }
              if (grammar instanceof LexerGrammar) {
                println(a.getLabel() + " = None");
              }
              if (grammar instanceof TreeWalkerGrammar) {
                // always generate rule-ref variables
                // for tree walker
                println(a.getLabel() + " = " + labeledElementInit);
              }
            }
          }
          else {
            // It is a token or literal reference.  Generate the
            // correct variable type for this grammar
            println(a.getLabel() + " = " + labeledElementInit);

            // In addition, generate *_AST variables if
            // building ASTs
            if (grammar.buildAST) {
              if (a instanceof GrammarAtom &&
                  ((GrammarAtom)a).getASTNodeType() != null) {
                GrammarAtom ga = (GrammarAtom)a;
                genASTDeclaration(a, ga.getASTNodeType());
              }
              else {
                genASTDeclaration(a);
              }
            }
          }
        }
      }
    }
  }

  /** Generate a series of case statements that implement a BitSet test.
   * @param p The Bitset for which cases are to be generated
   */
  protected void genCases(BitSet p) {
    if (DEBUG_CODE_GENERATOR) System.out.println("genCases(" + p + ")");
    int[] elems;

    elems = p.toArray();
    // Wrap cases four-per-line for lexer, one-per-line for parser
    int wrap = (grammar instanceof LexerGrammar) ? 4 : 1;
    int j = 1;
    boolean startOfLine = true;
    print("elif la1 and la1 in ");
    
    if (grammar instanceof LexerGrammar) 
    {
      _print("u'");
      for (int i = 0; i < elems.length; i++) {
        _print(getValueString(elems[i],false));
      }
      _print("':\n");
      return;
    }

    // Parser or TreeParser ..
    _print("[");
    for (int i = 0; i < elems.length; i++) {
      _print(getValueString(elems[i],false));
      if(i+1<elems.length)
        _print(",");
    }
    _print("]:\n");
  }

  /**Generate common code for a block of alternatives; return a
   * postscript that needs to be generated at the end of the
   * block.  Other routines may append else-clauses and such for
   * error checking before the postfix is generated.  If the
   * grammar is a lexer, then generate alternatives in an order
   * where alternatives requiring deeper lookahead are generated
   * first, and EOF in the lookahead set reduces the depth of
   * the lookahead.  @param blk The block to generate @param
   * noTestForSingle If true, then it does not generate a test
   * for a single alternative.
   */
  public PythonBlockFinishingInfo genCommonBlock(AlternativeBlock blk,
                                                 boolean noTestForSingle) {
    int _tabs_ = tabs; // remember where we are ..
    int nIF = 0;
    boolean createdLL1Switch = false;
    int closingBracesOfIFSequence = 0;

    PythonBlockFinishingInfo finishingInfo = 
      new PythonBlockFinishingInfo();

    // Save the AST generation state, and set it to that of the block
    boolean savegenAST = genAST;
    genAST = genAST && blk.getAutoGen();

    boolean oldsaveTest = saveText;
    saveText = saveText && blk.getAutoGen();

    // Is this block inverted?  If so, generate special-case code
    if (
      blk.not &&
      analyzer.subruleCanBeInverted(blk, grammar instanceof LexerGrammar)
      ) 
    {
      if (DEBUG_CODE_GENERATOR) System.out.println("special case: ~(subrule)");
      Lookahead p = analyzer.look(1, blk);
      // Variable assignment for labeled elements
      if (blk.getLabel() != null && syntacticPredLevel == 0) 
      {
        println(blk.getLabel() + " = " + lt1Value);
      }

      // AST
      genElementAST(blk);

      String astArgs = "";
      if (grammar instanceof TreeWalkerGrammar) {
        astArgs = "_t, ";
      }

      // match the bitset for the alternative
      println("self.match(" + astArgs + getBitsetName(markBitsetForGen(p.fset)) + ")");

      // tack on tree cursor motion if doing a tree walker
      if (grammar instanceof TreeWalkerGrammar) {
        println("_t = _t.getNextSibling()");
      }
      return finishingInfo;
    }


    // Special handling for single alt
    if (blk.getAlternatives().size() == 1) 
    {
      Alternative alt = blk.getAlternativeAt(0);
      // Generate a warning if there is a synPred for single alt.
      if (alt.synPred != null) {
        antlrTool.warning(
          "Syntactic predicate superfluous for single alternative",
          grammar.getFilename(),
          blk.getAlternativeAt(0).synPred.getLine(),
          blk.getAlternativeAt(0).synPred.getColumn()
          );
      }
      if (noTestForSingle) 
      {
        if (alt.semPred != null) {
          // Generate validating predicate
          genSemPred(alt.semPred, blk.line);
        }
        genAlt(alt, blk);     
        return finishingInfo;
      }
    }

    // count number of simple LL(1) cases; only do switch for
    // many LL(1) cases (no preds, no end of token refs)
    // We don't care about exit paths for (...)*, (...)+
    // because we don't explicitly have a test for them
    // as an alt in the loop.
    //
    // Also, we now count how many unicode lookahead sets
    // there are--they must be moved to DEFAULT or ELSE
    // clause.
    int nLL1 = 0;
    for (int i = 0; i < blk.getAlternatives().size(); i++) {
      Alternative a = blk.getAlternativeAt(i);
      if (suitableForCaseExpression(a)) {
        nLL1++;
      }
    }

    // do LL(1) cases
    if (nLL1 >= makeSwitchThreshold) 
    {
      // Determine the name of the item to be compared
      String testExpr = lookaheadString(1);
      createdLL1Switch = true;
      // when parsing trees, convert null to valid tree node with NULL lookahead
      if (grammar instanceof TreeWalkerGrammar) {
        println("if not _t:");
        tabs++;
        println("_t = antlr.ASTNULL");
        tabs--;
      }

      println("la1 = " + testExpr);
      // print dummy if to get a regular genCases ..
      println("if False:");
      tabs++;
      println("pass");
      //println("assert 0  # lunatic case");
      tabs--;

      for (int i = 0; i < blk.alternatives.size(); i++) 
      {
        Alternative alt = blk.getAlternativeAt(i);
        // ignore any non-LL(1) alts, predicated alts,
        // or end-of-token alts for case expressions
        if (!suitableForCaseExpression(alt)) {
          continue;
        }
        Lookahead p = alt.cache[1];
        if (p.fset.degree() == 0 && !p.containsEpsilon()) 
        {
          antlrTool.warning(
            "Alternate omitted due to empty prediction set",
            grammar.getFilename(),
            alt.head.getLine(), alt.head.getColumn());
        }
        else 
        {
          /* make the case statment, ie. if la1 in .. : */
          genCases(p.fset);
          tabs++;
          genAlt(alt,blk);
          tabs--;
        }
      }
      /* does this else belong here? */
      println("else:");
      tabs++;
    }

    // do non-LL(1) and nondeterministic cases This is tricky in
    // the lexer, because of cases like: STAR : '*' ; ASSIGN_STAR
    // : "*="; Since nextToken is generated without a loop, then
    // the STAR will have end-of-token as it's lookahead set for
    // LA(2).  So, we must generate the alternatives containing
    // trailing end-of-token in their lookahead sets *after* the
    // alternatives without end-of-token.  This implements the
    // usual lexer convention that longer matches come before
    // shorter ones, e.g.  "*=" matches ASSIGN_STAR not STAR
    //
    // For non-lexer grammars, this does not sort the alternates
    // by depth Note that alts whose lookahead is purely
    // end-of-token at k=1 end up as default or else clauses.
    int startDepth = (grammar instanceof LexerGrammar) ? grammar.maxk : 0;
    for (int altDepth = startDepth; altDepth >= 0; altDepth--) 
    {
      for (int i = 0; i < blk.alternatives.size(); i++) 
      {
        Alternative alt = blk.getAlternativeAt(i);
        if (DEBUG_CODE_GENERATOR) System.out.println("genAlt: " + i);
        // if we made a switch above, ignore what we already took care
        // of.  Specifically, LL(1) alts with no preds
        // that do not have end-of-token in their prediction set
        // and that are not giant unicode sets.
        if (createdLL1Switch && suitableForCaseExpression(alt)) {
          if (DEBUG_CODE_GENERATOR) 
            System.out.println("ignoring alt because it was in the switch");
          continue;
        }
        String e;

        boolean unpredicted = false;

        if (grammar instanceof LexerGrammar) 
        {
          // Calculate the "effective depth" of the alt,
          // which is the max depth at which
          // cache[depth]!=end-of-token
          int effectiveDepth = alt.lookaheadDepth;
          if (effectiveDepth == GrammarAnalyzer.NONDETERMINISTIC) {
            // use maximum lookahead
            effectiveDepth = grammar.maxk;
          }
          while (effectiveDepth >= 1 &&
                 alt.cache[effectiveDepth].containsEpsilon()) {
            effectiveDepth--;
          }
          // Ignore alts whose effective depth is other than
          // the ones we are generating for this iteration.
          if (effectiveDepth != altDepth) {
            if (DEBUG_CODE_GENERATOR)
              System.out.println(
                "ignoring alt because effectiveDepth!=altDepth" 
                + effectiveDepth + "!=" + altDepth);
            continue;
          }

          unpredicted = lookaheadIsEmpty(alt, effectiveDepth);
          e = getLookaheadTestExpression(alt, effectiveDepth);
        }
        else 
        {

          unpredicted = lookaheadIsEmpty(alt, grammar.maxk);
          e = getLookaheadTestExpression(alt, grammar.maxk);
        }

        // Was it a big unicode range that forced unsuitability
        // for a case expression?
        if (alt.cache[1].fset.degree() > caseSizeThreshold &&
            suitableForCaseExpression(alt)) {
          if (nIF == 0) {
            println("<m1> if " + e + ":");
          }
          else {
            println("<m2> elif " + e + ":");
          }
        }
        else
        {
          if (unpredicted &&
              alt.semPred == null &&
              alt.synPred == null) 
          {
            // The alt has empty prediction set and no
            // predicate to help out.  if we have not
            // generated a previous if, just put {...} around
            // the end-of-token clause
            if (nIF == 0) {
              println("##<m3> <closing");
            }
            else 
            {
              println("else: ## <m4>");
              tabs++;
              // to prevent an empty boyd
              // println("pass");
            }
            finishingInfo.needAnErrorClause = false;
          }
          else 
          { 
            // check for sem and syn preds

            // Add any semantic predicate expression to the
            // lookahead test
            if (alt.semPred != null) 
            {
              // if debugging, wrap the evaluation of the
              // predicate in a method translate $ and #
              // references
              ActionTransInfo tInfo = new ActionTransInfo();
              String actionStr =
                processActionForSpecialSymbols(
                  alt.semPred,
                  blk.line,
                  currentRule,
                  tInfo);
              // ignore translation info...we don't need to
              // do anything with it.  call that will inform
              // SemanticPredicateListeners of the result
              if (((grammar instanceof ParserGrammar) ||
                   (grammar instanceof LexerGrammar)) &&
                  grammar.debuggingOutput) 
              {
                e = "(" + e + 
                  " and fireSemanticPredicateEvaluated(antlr.debug.SemanticPredicateEvent.PREDICTING, " +
                  addSemPred(charFormatter.escapeString(actionStr)) + ", " + actionStr + "))";
              }
              else 
              {
                e = "(" + e + " and (" + actionStr + "))";
              }
            }

            // Generate any syntactic predicates
            if (nIF > 0) 
            {
              if (alt.synPred != null) 
              {
                println("else:");
                tabs++;  /* who's closing this one? */
                genSynPred(alt.synPred, e);
                closingBracesOfIFSequence++;
              }
              else 
              {
                println("elif " + e + ":");
              }
            }
            else 
            {
              if (alt.synPred != null) 
              {
                genSynPred(alt.synPred, e);
              }
              else 
              {
                // when parsing trees, convert null to
                // valid tree node with NULL lookahead.
                if (grammar instanceof TreeWalkerGrammar) {
                  println("if not _t:");
                  tabs++;
                  println("_t = antlr.ASTNULL");
                  tabs--;
                }
                println("if " + e + ":");
              }
            }
          }
        }

        nIF++;
        tabs++;
        genAlt(alt, blk); // this should have generated something. If not
        // we could end up in an empty else:
        tabs--;
      }
    }
    String ps = "";
    //for (int i = 1; i <= closingBracesOfIFSequence; i++) {
    //    ps += "";
    //}

    // Restore the AST generation state
    genAST = savegenAST;

    // restore save text state
    saveText = oldsaveTest;

    // Return the finishing info.
    if (createdLL1Switch) {
      finishingInfo.postscript = ps;
      finishingInfo.generatedSwitch = true;
      finishingInfo.generatedAnIf = nIF > 0;
    }
    else {
      finishingInfo.postscript = ps;
      finishingInfo.generatedSwitch = false;
      finishingInfo.generatedAnIf = nIF > 0;
    }

    return finishingInfo;
  }

  private static boolean suitableForCaseExpression(Alternative a) {
    return
      a.lookaheadDepth == 1 &&
      a.semPred == null &&
      !a.cache[1].containsEpsilon() &&
      a.cache[1].fset.degree() <= caseSizeThreshold;
  }

  /** Generate code to link an element reference into the AST */
  private void genElementAST(AlternativeElement el) {
    // handle case where you're not building trees, but are in tree walker.
    // Just need to get labels set up.
    if (grammar instanceof TreeWalkerGrammar && !grammar.buildAST) {
      String elementRef;
      String astName;

      // Generate names and declarations of the AST variable(s)
      if (el.getLabel() == null) {
        elementRef = lt1Value;
        // Generate AST variables for unlabeled stuff
        astName = "tmp" + astVarNumber + "_AST";
        astVarNumber++;
        // Map the generated AST variable in the alternate
        mapTreeVariable(el, astName);
        // Generate an "input" AST variable also
        println(astName + "_in = " + elementRef);
      }
      return;
    }

    if (grammar.buildAST && syntacticPredLevel == 0) {
      boolean needASTDecl =
        (genAST &&
         (el.getLabel() != null ||
          el.getAutoGenType() != GrammarElement.AUTO_GEN_BANG
           )
          );

      // RK: if we have a grammar element always generate the decl
      // since some guy can access it from an action and we can't
      // peek ahead (well not without making a mess).
      // I'd prefer taking this out.
      if (el.getAutoGenType() != GrammarElement.AUTO_GEN_BANG &&
          (el instanceof TokenRefElement))
      {
        needASTDecl = true;
      }

      boolean doNoGuessTest =
        (grammar.hasSyntacticPredicate && needASTDecl);

      String elementRef;
      String astNameBase;

      // Generate names and declarations of the AST variable(s)
      if (el.getLabel() != null) {
        elementRef = el.getLabel();
        astNameBase = el.getLabel();
      }
      else {
        elementRef = lt1Value;
        // Generate AST variables for unlabeled stuff
        astNameBase = "tmp" + astVarNumber;
        ;
        astVarNumber++;
      }

      // Generate the declaration if required.
      if (needASTDecl) {
        // Generate the declaration
        if (el instanceof GrammarAtom) {
          GrammarAtom ga = (GrammarAtom)el;
          if (ga.getASTNodeType() != null) {
            genASTDeclaration(el, astNameBase, ga.getASTNodeType());
          }
          else {
            genASTDeclaration(el, astNameBase, labeledElementASTType);
          }
        }
        else {
          genASTDeclaration(el, astNameBase, labeledElementASTType);
        }
      }

      // for convenience..
      String astName = astNameBase + "_AST";

      // Map the generated AST variable in the alternate
      mapTreeVariable(el, astName);
      if (grammar instanceof TreeWalkerGrammar) {
        // Generate an "input" AST variable also
        println(astName + "_in = None");
      }

      // Enclose actions with !guessing
      if (doNoGuessTest) {
        // println("if (inputState.guessing==0) {");
        // tabs++;
      }

      // if something has a label assume it will be used
      // so we must initialize the RefAST
      if (el.getLabel() != null) {
        if (el instanceof GrammarAtom) {
          println(astName + " = " + getASTCreateString((GrammarAtom)el, elementRef) + "");
        }
        else {
          println(astName + " = " + getASTCreateString(elementRef) + "");
        }
      }

      // if it has no label but a declaration exists initialize it.
      if (el.getLabel() == null && needASTDecl) {
        elementRef = lt1Value;
        if (el instanceof GrammarAtom) {
          println(astName + " = " + getASTCreateString((GrammarAtom)el, elementRef) + "");
        }
        else {
          println(astName + " = " + getASTCreateString(elementRef) + "");
        }
        // Map the generated AST variable in the alternate
        if (grammar instanceof TreeWalkerGrammar) {
          // set "input" AST variable also
          println(astName + "_in = " + elementRef + "");
        }
      }

      if (genAST) {
        switch (el.getAutoGenType()) {
          case GrammarElement.AUTO_GEN_NONE:
            println("self.addASTChild(currentAST, " + astName + ")");
            break;
          case GrammarElement.AUTO_GEN_CARET:
            println("self.makeASTRoot(currentAST, " + astName + ")");
            break;
          default:
            break;
        }
      }
      if (doNoGuessTest) {
        // tabs--;
      }
    }
  }

  /** Close the try block and generate catch phrases
   * if the element has a labeled handler in the rule
   */
  private void genErrorCatchForElement(AlternativeElement el) {
    if (el.getLabel() == null) return;
    String r = el.enclosingRuleName;
    if (grammar instanceof LexerGrammar) {
      r = CodeGenerator.encodeLexerRuleName(el.enclosingRuleName);
    }
    RuleSymbol rs = (RuleSymbol)grammar.getSymbol(r);
    if (rs == null) {
      antlrTool.panic("Enclosing rule not found!");
    }
    ExceptionSpec ex = rs.block.findExceptionSpec(el.getLabel());
    if (ex != null) {
      tabs--;
      genErrorHandler(ex);
    }
  }

  /** Generate the catch phrases for a user-specified error handler */
  private void genErrorHandler(ExceptionSpec ex) {
    // Each ExceptionHandler in the ExceptionSpec is a separate catch
    for (int i = 0; i < ex.handlers.size(); i++) {
      ExceptionHandler handler = (ExceptionHandler)ex.handlers.elementAt(i);
      // Generate except statement
      String exceptionType = "", exceptionName = "";
      String s = handler.exceptionTypeAndName.getText();
      s = removeAssignmentFromDeclaration(s);
      s = s.trim();
      // Search back from the end for a non alphanumeric.  That marks the
      // beginning of the identifier
      for (int j = s.length() - 1; j >= 0; j--) {
        // TODO: make this work for language-independent identifiers?
        if (!Character.isLetterOrDigit(s.charAt(j)) &&
          s.charAt(j) != '_') {
          // Found end of type part
          exceptionType = s.substring(0, j);
          exceptionName = s.substring(j + 1);
	  break;
        }
      }

      println("except " + exceptionType + ", " + exceptionName + ":");
      tabs++;
      if (grammar.hasSyntacticPredicate) {
        println("if not self.inputState.guessing:");
        tabs++;
      }

      // When not guessing, execute user handler action
      ActionTransInfo tInfo = new ActionTransInfo();
      printAction(
        processActionForSpecialSymbols(handler.action.getText(),
                                       handler.action.getLine(),
                                       currentRule, tInfo)
        );

      if (grammar.hasSyntacticPredicate) {
        tabs--;
        println("else:");
        tabs++;
        // When guessing, reraise exception
        println("raise " + exceptionName);
        tabs--;
      }
      // Close except statement
      tabs--;
    }
  }

  /** Generate a try { opening if the element has a labeled handler in the rule */
  private void genErrorTryForElement(AlternativeElement el) {
    if (el.getLabel() == null) return;
    String r = el.enclosingRuleName;
    if (grammar instanceof LexerGrammar) {
      r = CodeGenerator.encodeLexerRuleName(el.enclosingRuleName);
    }
    RuleSymbol rs = (RuleSymbol)grammar.getSymbol(r);
    if (rs == null) {
      antlrTool.panic("Enclosing rule not found!");
    }
    ExceptionSpec ex = rs.block.findExceptionSpec(el.getLabel());
    if (ex != null) {
      println("try: # for error handling");
      tabs++;
    }
  }

  protected void genASTDeclaration(AlternativeElement el) {
    genASTDeclaration(el, labeledElementASTType);
  }

  protected void genASTDeclaration(AlternativeElement el, String node_type) {
    genASTDeclaration(el, el.getLabel(), node_type);
  }

  protected void genASTDeclaration(AlternativeElement el, String var_name, String node_type) {
    // already declared?
    if (declaredASTVariables.contains(el))
      return;

    // emit code
    println(var_name + "_AST = None");

    // mark as declared
    declaredASTVariables.put(el,el);
  }

  /** Generate a header that is common to all Python files */
  protected void genHeader() {
    println("### $ANTLR " + Tool.version + ": " +
            "\"" + antlrTool.fileMinusPath(antlrTool.grammarFile) + "\"" +
            " -> " +
            "\"" + grammar.getClassName() + ".py\"$");
  }

  /** Generate an iterator method for the Python CharScanner (sub)classes. */
//  protected void genIterator() {
//    println("def __iter__(self):");
//    tabs++;
//    println("return antlr.CharScannerIterator(self)");
//    tabs--;
//  }

  /** Generate an automated test for Python CharScanner (sub)classes. */
  protected void genLexerTest() {
    String className = grammar.getClassName();
    println("if __name__ == '__main__' :");
    tabs++;
    println("import sys");
    println("import antlr");
    println("import " + className);
    println("");
    println("### create lexer - shall read from stdin");
    println("try:");
    tabs++;
    println("for token in " + className + ".Lexer():");
    tabs++;
    println("print token");
    println("");
    tabs--;
    tabs--;
    println("except antlr.TokenStreamException, e:");
    tabs++;
    println("print \"error: exception caught while lexing: \", e");
    tabs--;
    tabs--;
  }

  private void genLiteralsTest() 
  {
    println("### option { testLiterals=true } ");
    println("_ttype = self.testLiteralsTable(_ttype)");
  }

  private void genLiteralsTestForPartialToken() {
    println("_ttype = self.testLiteralsTable(self.text.getString(), _begin, self.text.length()-_begin, _ttype)");
  }

  protected void genMatch(BitSet b) {
  }

  protected void genMatch(GrammarAtom atom) {
    if (atom instanceof StringLiteralElement) {
      if (grammar instanceof LexerGrammar) {
        genMatchUsingAtomText(atom);
      }
      else {
        genMatchUsingAtomTokenType(atom);
      }
    }
    else if (atom instanceof CharLiteralElement) {
      if (grammar instanceof LexerGrammar) {
        genMatchUsingAtomText(atom);
      }
      else {
        antlrTool.error("cannot ref character literals in grammar: " + atom);
      }
    }
    else if (atom instanceof TokenRefElement) {
      genMatchUsingAtomText(atom);
    }
    else if (atom instanceof WildcardElement) {
      gen((WildcardElement)atom);
    }
  }

  protected void genMatchUsingAtomText(GrammarAtom atom) {
    // match() for trees needs the _t cursor
    String astArgs = "";
    if (grammar instanceof TreeWalkerGrammar) {
      astArgs = "_t,";
    }

    // if in lexer and ! on element, save buffer index to kill later
    if (grammar instanceof LexerGrammar && 
        (!saveText || atom.getAutoGenType() == GrammarElement.AUTO_GEN_BANG)) 
    {
      println("_saveIndex = self.text.length()");
    }
    
    
    print(atom.not ? "self.matchNot(" : "self.match(");
    _print(astArgs);

    // print out what to match
    if (atom.atomText.equals("EOF")) {
      // horrible hack to handle EOF case
      _print("EOF_TYPE");
    }
    else {
      _print(atom.atomText);
    }
    _println(")");

    if (grammar instanceof LexerGrammar && (!saveText || atom.getAutoGenType() == GrammarElement.AUTO_GEN_BANG)) {
      println("self.text.setLength(_saveIndex)");   // kill text atom put in buffer
    }
  }

  protected void genMatchUsingAtomTokenType(GrammarAtom atom) {
    // match() for trees needs the _t cursor
    String astArgs = "";
    if (grammar instanceof TreeWalkerGrammar) {
      astArgs = "_t,";
    }

    // If the literal can be mangled, generate the symbolic constant instead
    String mangledName = null;
    String s = astArgs + getValueString(atom.getType(),true);

    // matching 
    println((atom.not ? "self.matchNot(" : "self.match(") + s + ")");
  }

  /** Generate the nextToken() rule.  nextToken() is a synthetic
   * lexer rule that is the implicit OR of all user-defined
   * lexer rules.
   */
  public 
  void genNextToken() {
    // Are there any public rules?  If not, then just generate a
    // fake nextToken().
    boolean hasPublicRules = false;
    for (int i = 0; i < grammar.rules.size(); i++) {
      RuleSymbol rs = (RuleSymbol)grammar.rules.elementAt(i);
      if (rs.isDefined() && rs.access.equals("public")) {
        hasPublicRules = true;
        break;
      }
    }
    if (!hasPublicRules) {
      println("");
      println("def nextToken(self): ");
      tabs++;
      println("try:");
      tabs++;
      println("self.uponEOF()");
      tabs--;
      println("except antlr.CharStreamIOException, csioe:");
      tabs++;
      println("raise antlr.TokenStreamIOException(csioe.io)");
      tabs--;
      println("except antlr.CharStreamException, cse:");
      tabs++;
      println("raise antlr.TokenStreamException(str(cse))");
      tabs--;
      println("return antlr.CommonToken(type=EOF_TYPE, text=\"\")");
      tabs--;
      return;
    }

    // Create the synthesized nextToken() rule
    RuleBlock nextTokenBlk = 
      MakeGrammar.createNextTokenRule(grammar, grammar.rules, "nextToken");

    // Define the nextToken rule symbol
    RuleSymbol nextTokenRs = new RuleSymbol("mnextToken");
    nextTokenRs.setDefined();
    nextTokenRs.setBlock(nextTokenBlk);
    nextTokenRs.access = "private";
    grammar.define(nextTokenRs);
    // Analyze the nextToken rule
    boolean ok = grammar.theLLkAnalyzer.deterministic(nextTokenBlk);

    // Generate the next token rule
    String filterRule = null;
    if (((LexerGrammar)grammar).filterMode) {
      filterRule = ((LexerGrammar)grammar).filterRule;
    }

    println("");
    println("def nextToken(self):");
    tabs++;
    println("while True:");
    tabs++;
    println("try: ### try again ..");
    tabs++;
    println("while True:");
    tabs++;
    int _tabs_ = tabs; // while block
    println("_token = None");
    println("_ttype = INVALID_TYPE");
    if (((LexerGrammar)grammar).filterMode) 
    {
      println("self.setCommitToPath(False)");
      if (filterRule != null) 
      {
        // Here's a good place to ensure that the filter rule actually exists
        if (!grammar.isDefined(CodeGenerator.encodeLexerRuleName(filterRule))) {
          grammar.antlrTool.error(
            "Filter rule " + filterRule + " does not exist in this lexer");
        }
        else 
        {
          RuleSymbol rs = (RuleSymbol)grammar.getSymbol(
            CodeGenerator.encodeLexerRuleName(filterRule));
          if (!rs.isDefined()) {
            grammar.antlrTool.error(
              "Filter rule " + filterRule + " does not exist in this lexer");
          }
          else if (rs.access.equals("public")) {
            grammar.antlrTool.error(
              "Filter rule " + filterRule + " must be protected");
          }
        }
        println("_m = self.mark()");
      }
    }
    println("self.resetText()");

    println("try: ## for char stream error handling");
    tabs++;
    _tabs_ = tabs; // inner try

    // Generate try around whole thing to trap scanner errors
    println("try: ##for lexical error handling");
    tabs++;
    _tabs_ = tabs; // inner try


    // Test for public lexical rules with empty paths
    for (int i = 0; i < nextTokenBlk.getAlternatives().size(); i++) 
    {
      Alternative a = nextTokenBlk.getAlternativeAt(i);
      if (a.cache[1].containsEpsilon()) 
      {
        //String r = a.head.toString();
        RuleRefElement rr = (RuleRefElement)a.head;
        String r = CodeGenerator.decodeLexerRuleName(rr.targetRule);
        antlrTool.warning("public lexical rule "+r+" is optional (can match \"nothing\")");
      }
    }

    // Generate the block
    String newline = System.getProperty("line.separator");

    /* generate the common block */
    PythonBlockFinishingInfo howToFinish = 
      genCommonBlock(nextTokenBlk, false);

    /* how to finish the block */
    String errFinish = "";

    // Is this a filter? if so we  need to change the default handling.
    // In non filter mode we generate EOF token on EOF and stop, other-
    // wise an error gets generated. In  filter  mode we just continue
    // by consuming the unknown character till EOF is seen.
    if (((LexerGrammar)grammar).filterMode) 
    {
      /* filter */
      if (filterRule == null) 
      {
        /* no specical filter rule has been given. */
        errFinish += "self.filterdefault(self.LA(1))";
      }
      else 
      {
        errFinish += 
          "self.filterdefault(self.LA(1), self.m" + filterRule + ", False)";
      }
    }
    else 
    {
      /* non filter */

      /* if an IF has been generated (in the default clause), indendation
      ** is not correct. In that case we need to close on level++. 
      **/

      errFinish = "self.default(self.LA(1))" ;
    }


    /* finish the block */
    genBlockFinish1(howToFinish, errFinish);

    // alt block has finished .., reset tabs!
    tabs = _tabs_;

    // at this point a valid token has been matched, undo "mark" that was done
    if (((LexerGrammar)grammar).filterMode && filterRule != null) {
      println("self.commit()");
    }

    // Generate literals test if desired
    // make sure _ttype is set first; note _returnToken must be
    // non-null as the rule was required to create it.
    println("if not self._returnToken:");
    tabs++;
    println("raise antlr.TryAgain ### found SKIP token");
    tabs--;

    // There's one literal test (in Lexer) after the large switch
    // in 'nextToken'.
    if (((LexerGrammar)grammar).getTestLiterals()) 
    {
      println("### option { testLiterals=true } ");
      //genLiteralsTest();
      println("self.testForLiteral(self._returnToken)");
    }

    // return token created by rule reference in switch
    println("### return token to caller");
    println("return self._returnToken");

    // Close try block
    tabs--;
    println("### handle lexical errors ....");
    println("except antlr.RecognitionException, e:");
    tabs++;
    if (((LexerGrammar)grammar).filterMode) 
    {
      if (filterRule == null) 
      {
        println("if not self.getCommitToPath():");
        tabs++;
        println("self.consume()");
        println("raise antlr.TryAgain()");
        tabs--;
      }
      else 
      {
        println("if not self.getCommitToPath(): ");
        tabs++;
        println("self.rewind(_m)");
        println("self.resetText()");
        println("try:");
        tabs++;
        println("self.m" + filterRule + "(False)");
        tabs--;
        println("except antlr.RecognitionException, ee:");
        tabs++;
        println("### horrendous failure: error in filter rule");
        println("self.reportError(ee)");
        println("self.consume()");
        tabs--;
        println("raise antlr.TryAgain()");
        tabs--;
      }
    }
    if (nextTokenBlk.getDefaultErrorHandler()) {
      println("self.reportError(e)");
      println("self.consume()");
    }
    else {
      // pass on to invoking routine
      println("raise antlr.TokenStreamRecognitionException(e)");
    }
    tabs--;
    //println("");
    //println("### shall never be reached ");
    //println("assert 0");
    // close CharStreamException try
    tabs--;
    println("### handle char stream errors ...");
    println("except antlr.CharStreamException,cse:");
    tabs++;
    println("if isinstance(cse, antlr.CharStreamIOException):");
    tabs++;
    println("raise antlr.TokenStreamIOException(cse.io)");
    tabs--;
    println("else:");
    tabs++;
    println("raise antlr.TokenStreamException(str(cse))");
    tabs--;
    tabs--;
    //println("### shall never be reached ");
    //println("assert 0");
    // close for-loop
    tabs--;
    //println("### <end of inner while>");
    //println("### shall never be reached ");
    //println("assert 0");
    tabs--;
    //println("### <matching outer try>");
    println("except antlr.TryAgain:");
    tabs++;
    println("pass");
    tabs--;
    // close method nextToken
    tabs--;
    //println("### <end of outer while>");
    //println("### shall never be reached");
    //println("assert 0");
    //println("### <end of method nextToken>");
  }

  /** Gen a named rule block.
   * ASTs are generated for each element of an alternative unless
   * the rule or the alternative have a '!' modifier.
   *
   * If an alternative defeats the default tree construction, it
   * must set <rule>_AST to the root of the returned AST.
   *
   * Each alternative that does automatic tree construction, builds
   * up root and child list pointers in an ASTPair structure.
   *
   * A rule finishes by setting the returnAST variable from the
   * ASTPair.
   *
   * @param rule The name of the rule to generate
   * @param startSymbol true if the rule is a start symbol (i.e., not referenced elsewhere)
   */
  public void genRule(RuleSymbol s, boolean startSymbol, int ruleNum) {

    tabs=1;
    if (!s.isDefined()) {
      antlrTool.error("undefined rule: " + s.getId());
      return;
    }

    // Generate rule return type, name, arguments
    RuleBlock rblk = s.getBlock();

    currentRule = rblk;
    currentASTResult = s.getId();

    // clear list of declared ast variables..
    declaredASTVariables.clear();

    // Save the AST generation state, and set it to that of the rule
    boolean savegenAST = genAST;
    genAST = genAST && rblk.getAutoGen();

    // boolean oldsaveTest = saveText;
    saveText = rblk.getAutoGen();

    // print javadoc comment if any
    genJavadocComment(s);

    // Gen method name
    print("def " + s.getId() + "(");

    // Additional rule parameters common to all rules for this grammar
    _print(commonExtraParams);
    if (commonExtraParams.length() != 0 && rblk.argAction != null) {
      _print(",");
    }


    // Gen arguments
    if (rblk.argAction != null) {
      // Has specified arguments
      _println("");
      tabs++;
      println(rblk.argAction);
      tabs--;
      print("):");
    }
    else {
      // No specified arguments
      _print("):");
    }

    println("");
    tabs++;

    // Convert return action to variable declaration
    if (rblk.returnAction != null) {
      if (rblk.returnAction.indexOf('=') >= 0)
        println(rblk.returnAction);
      else {
        // mx
        println(extractIdOfAction(rblk.returnAction, rblk.getLine(), rblk.getColumn()) + " = None"); }
    }

    // print out definitions needed by rules for various grammar types
    println(commonLocalVars);

    if (grammar.traceRules) {
      if (grammar instanceof TreeWalkerGrammar) {
        println("self.traceIn(\"" + s.getId() + "\",_t)");
      }
      else {
        println("self.traceIn(\"" + s.getId() + "\")");
      }
    }

    if (grammar instanceof LexerGrammar) {
      // lexer rule default return value is the rule's token name
      // This is a horrible hack to support the built-in EOF lexer rule.
      if (s.getId().equals("mEOF"))
        println("_ttype = EOF_TYPE");
      else
        println("_ttype = " + s.getId().substring(1));
      println("_saveIndex = 0");    // used for element! (so we can kill text matched for element)
    }

    // if debugging, write code to mark entry to the rule
    if (grammar.debuggingOutput)
      if (grammar instanceof ParserGrammar)
        println("self.fireEnterRule(" + ruleNum + ", 0)");
      else if (grammar instanceof LexerGrammar)
        println("self.fireEnterRule(" + ruleNum + ", _ttype)");

    // Generate trace code if desired
    if (grammar.debuggingOutput || grammar.traceRules) {
      println("try: ### debugging");
      tabs++;
    }

    // Initialize AST variables
    if (grammar instanceof TreeWalkerGrammar) {
      // "Input" value for rule
      println(s.getId() + "_AST_in = None");
      println("if _t != antlr.ASTNULL:"); 
      tabs++;
      println(s.getId() + "_AST_in = _t");
      tabs--;
    }
    if (grammar.buildAST) 
    {
      // Parser member used to pass AST returns from rule invocations
      println("self.returnAST = None");
      println("currentAST = antlr.ASTPair()");
      // User-settable return value for rule.
      println(s.getId() + "_AST = None");
    }

    genBlockPreamble(rblk);
    genBlockInitAction(rblk);

    // Search for an unlabeled exception specification attached to the rule
    ExceptionSpec unlabeledUserSpec = rblk.findExceptionSpec("");

    // Generate try block around the entire rule for  error handling
    if (unlabeledUserSpec != null || rblk.getDefaultErrorHandler()) {
      println("try:      ## for error handling");
      tabs++;
    }
    int _tabs_ = tabs;
    // Generate the alternatives
    if (rblk.alternatives.size() == 1) 
    {
      // One alternative -- use simple form
      Alternative alt = rblk.getAlternativeAt(0);
      String pred = alt.semPred;
      if (pred != null)
        genSemPred(pred, currentRule.line);
      if (alt.synPred != null) 
      {
        antlrTool.warning(
          "Syntactic predicate ignored for single alternative",
          grammar.getFilename(),
          alt.synPred.getLine(),
          alt.synPred.getColumn()
          );
      }
      genAlt(alt, rblk);
    }
    else 
    {
      // Multiple alternatives -- generate complex form
      boolean ok = grammar.theLLkAnalyzer.deterministic(rblk);

      PythonBlockFinishingInfo howToFinish = genCommonBlock(rblk, false);
      genBlockFinish(howToFinish, throwNoViable);
    }
    tabs = _tabs_;
    
    // Generate catch phrase for error handling
    if (unlabeledUserSpec != null || rblk.getDefaultErrorHandler()) {
      // Close the try block
      tabs--;
      println("");
    }
    
    // Generate user-defined or default catch phrases
    if (unlabeledUserSpec != null) {
      genErrorHandler(unlabeledUserSpec);
    }
    else if (rblk.getDefaultErrorHandler()) {
      // Generate default catch phrase
      println("except " + exceptionThrown + ", ex:");
      tabs++;
      // Generate code to handle error if not guessing
      if (grammar.hasSyntacticPredicate) {
        println("if not self.inputState.guessing:");
        tabs++;
      }
      println("self.reportError(ex)");
      if (!(grammar instanceof TreeWalkerGrammar)) {
        // Generate code to consume until token in k==1 follow set
        Lookahead follow = grammar.theLLkAnalyzer.FOLLOW(1, rblk.endNode);
        String followSetName = getBitsetName(markBitsetForGen(follow.fset));
        println("self.consume()");
        println("self.consumeUntil(" + followSetName + ")");
      }
      else {
        // Just consume one token
        println("if _t:");
        tabs++;
        println("_t = _t.getNextSibling()");
        tabs--;
      }
      if (grammar.hasSyntacticPredicate) {
        tabs--;
        // When guessing, rethrow exception
        println("else:");
        tabs++;
        println("raise ex");
        tabs--;
      }
      // Close catch phrase
      tabs--;
      println("");
    }

    // Squirrel away the AST "return" value
    if (grammar.buildAST) {
      println("self.returnAST = " + s.getId() + "_AST");
    }

    // Set return tree value for tree walkers
    if (grammar instanceof TreeWalkerGrammar) {
      println("self._retTree = _t");
    }

    // Generate literals test for lexer rules so marked
    if (rblk.getTestLiterals()) {
      if (s.access.equals("protected")) {
        genLiteralsTestForPartialToken();
      }
      else {
        genLiteralsTest();
      }
    }

    // if doing a lexer rule, dump code to create token if necessary
    if (grammar instanceof LexerGrammar) 
    {
      println("self.set_return_token(_createToken, _token, _ttype, _begin)");
    }

    if(rblk.returnAction != null)
    {
//      if(grammar instanceof LexerGrammar) 
//      {
      println("return " + 
              extractIdOfAction(rblk.returnAction, 
                                rblk.getLine(), 
                                rblk.getColumn()) + "");
//      }
//      else
//      {
//    println("return r");
//      }
    }

    if (grammar.debuggingOutput || grammar.traceRules) {
      tabs--;
      println("finally:  ### debugging");
      tabs++;

      // If debugging, generate calls to mark exit of rule
      if (grammar.debuggingOutput)
        if (grammar instanceof ParserGrammar)
          println("self.fireExitRule(" + ruleNum + ", 0)");
        else if (grammar instanceof LexerGrammar)
          println("self.fireExitRule(" + ruleNum + ", _ttype)");

      if (grammar.traceRules) {
        if (grammar instanceof TreeWalkerGrammar) {
          println("self.traceOut(\"" + s.getId() + "\", _t)");
        }
        else {
          println("self.traceOut(\"" + s.getId() + "\")");
        }
      }
      tabs--;
    }
    tabs--;
    println("");

    // Restore the AST generation state
    genAST = savegenAST;

    // restore char save state
    // saveText = oldsaveTest;
  }

  private void GenRuleInvocation(RuleRefElement rr) {
    // dump rule name
    _print("self." + rr.targetRule + "(");

    // lexers must tell rule if it should set _returnToken
    if (grammar instanceof LexerGrammar) {
      // if labeled, could access Token, so tell rule to create
      if (rr.getLabel() != null) {
        _print("True");
      }
      else {
        _print("False");
      }
      if (commonExtraArgs.length() != 0 || rr.args != null) {
        _print(", ");
      }
    }

    // Extra arguments common to all rules for this grammar
    _print(commonExtraArgs);
    if (commonExtraArgs.length() != 0 && rr.args != null) {
      _print(", ");
    }

    // Process arguments to method, if any
    RuleSymbol rs = (RuleSymbol)grammar.getSymbol(rr.targetRule);
    if (rr.args != null) {
      // When not guessing, execute user arg action
      ActionTransInfo tInfo = new ActionTransInfo();
      String args = processActionForSpecialSymbols(rr.args, 0, currentRule, tInfo);
      if (tInfo.assignToRoot || tInfo.refRuleRoot != null) {
        antlrTool.error("Arguments of rule reference '" + rr.targetRule + "' cannot set or ref #" +
                        currentRule.getRuleName(), grammar.getFilename(), rr.getLine(), rr.getColumn());
      }
      _print(args);

      // Warn if the rule accepts no arguments
      if (rs.block.argAction == null) {
        antlrTool.warning("Rule '" + rr.targetRule + "' accepts no arguments", grammar.getFilename(), rr.getLine(), rr.getColumn());
      }
    }
    else {
      // For C++, no warning if rule has parameters, because there may be default
      // values for all of the parameters
      if (rs.block.argAction != null) {
        antlrTool.warning("Missing parameters on reference to rule " + rr.targetRule, grammar.getFilename(), rr.getLine(), rr.getColumn());
      }
    }
    _println(")");

    // move down to the first child while parsing
    if (grammar instanceof TreeWalkerGrammar) {
      println("_t = self._retTree");
    }
  }

  protected void genSemPred(String pred, int line) {
    // translate $ and # references
    ActionTransInfo tInfo = new ActionTransInfo();
    pred = processActionForSpecialSymbols(pred, line, currentRule, tInfo);

    // ignore translation info...we don't need to do anything with it.
    String escapedPred = charFormatter.escapeString(pred);

    // if debugging, wrap the semantic predicate evaluation in a method
    // that can tell SemanticPredicateListeners the result
    if (grammar.debuggingOutput && 
        ((grammar instanceof ParserGrammar) || (grammar instanceof LexerGrammar)))
      pred = "fireSemanticPredicateEvaluated(antlr.debug.SemanticPredicateEvent.VALIDATING,"
        + addSemPred(escapedPred) + ", " + pred + ")";

    /* always .. */
    println("if not " + pred + ":");
    tabs++;
    println("raise antlr.SemanticException(\"" + escapedPred + "\")");
    tabs--;
  }

  /** Write an array of Strings which are the semantic predicate
   *  expressions.  The debugger will reference them by number only
   */
  protected void genSemPredMap() {
    Enumeration e = semPreds.elements();
    println("_semPredNames = [");
    tabs++;
    while (e.hasMoreElements()) {
      println("\"" + e.nextElement() + "\",");
    }
    tabs--;
    println("]");
  }

  protected void genSynPred(SynPredBlock blk, String lookaheadExpr) {
    if (DEBUG_CODE_GENERATOR) System.out.println("gen=>(" + blk + ")");

    // Dump synpred result variable
    println("synPredMatched" + blk.ID + " = False");
    // Gen normal lookahead test
    println("if " + lookaheadExpr + ":");
    tabs++;

    // Save input state
    if (grammar instanceof TreeWalkerGrammar) {
      println("_t" + blk.ID + " = _t");
    }
    else {
      println("_m" + blk.ID + " = self.mark()");
    }

    // Once inside the try, assume synpred works unless exception caught
    println("synPredMatched" + blk.ID + " = True");
    println("self.inputState.guessing += 1");

    // if debugging, tell listeners that a synpred has started
    if (grammar.debuggingOutput && ((grammar instanceof ParserGrammar) ||
                                    (grammar instanceof LexerGrammar))) {
      println("self.fireSyntacticPredicateStarted()");
    }

    syntacticPredLevel++;
    println("try:");
    tabs++;
    gen((AlternativeBlock)blk);   // gen code to test predicate
    tabs--;
    println("except " + exceptionThrown + ", pe:");
    tabs++;
    println("synPredMatched" + blk.ID + " = False");
    tabs--;

    // Restore input state
    if (grammar instanceof TreeWalkerGrammar) {
      println("_t = _t" + blk.ID + "");
    }
    else {
      println("self.rewind(_m" + blk.ID + ")");
    }

    println("self.inputState.guessing -= 1");

    // if debugging, tell listeners how the synpred turned out
    if (grammar.debuggingOutput && ((grammar instanceof ParserGrammar) ||
                                    (grammar instanceof LexerGrammar))) {
      println("if synPredMatched" + blk.ID + ":");
      tabs++;
      println("self.fireSyntacticPredicateSucceeded()");
      tabs--;
      println("else:");
      tabs++;
      println("self.fireSyntacticPredicateFailed()");
      tabs--;
    }

    syntacticPredLevel--;
    tabs--;

    // Close lookahead test

    // Test synred result
    println("if synPredMatched" + blk.ID + ":");
  }

  /** Generate a static array containing the names of the tokens,
   * indexed by the token type values.  This static array is used
   * to format error messages so that the token identifers or literal
   * strings are displayed instead of the token numbers.
   *
   * If a lexical rule has a paraphrase, use it rather than the
   * token label.
   */
  public void genTokenStrings() {
    // Generate a string for each token.  This creates a static
    // array of Strings indexed by token type.
    int save_tabs = tabs;
    tabs = 0;
    
    println("");
    println("_tokenNames = [");
    tabs++;

    // Walk the token vocabulary and generate a Vector of strings
    // from the tokens.
    Vector v = grammar.tokenManager.getVocabulary();
    for (int i = 0; i < v.size(); i++) {
      String s = (String)v.elementAt(i);
      if (s == null) {
        s = "<" + String.valueOf(i) + ">";
      }
      if (!s.startsWith("\"") && !s.startsWith("<")) {
        TokenSymbol ts = (TokenSymbol)grammar.tokenManager.getTokenSymbol(s);
        if (ts != null && ts.getParaphrase() != null) {
          s = StringUtils.stripFrontBack(ts.getParaphrase(), "\"", "\"");
        }
      }
      print(charFormatter.literalString(s));
      if (i != v.size() - 1) {
        _print(", ");
      }
      _println("");
    }

    // Close the string array initailizer
    tabs--;
    println("]");
    tabs = save_tabs;
  }

  /** Create and set Integer token type objects that map
   *  to Java Class objects (which AST node to create).
   */
  protected void genTokenASTNodeMap() {
    println("");
    println("def buildTokenTypeASTClassMap(self):");
    // Generate a map.put("T","TNode") for each token
    // if heterogeneous node known for that token T.
    tabs++;
    boolean generatedNewHashtable = false;
    int n = 0;
    // Walk the token vocabulary and generate puts.
    Vector v = grammar.tokenManager.getVocabulary();
    for (int i = 0; i < v.size(); i++) {
      String s = (String)v.elementAt(i);
      if (s != null) {
        TokenSymbol ts = grammar.tokenManager.getTokenSymbol(s);
        if (ts != null && ts.getASTNodeType() != null) {
          n++;
          if ( !generatedNewHashtable ) {
            // only generate if we are going to add a mapping
            println("self.tokenTypeToASTClassMap = {}");
            generatedNewHashtable = true;
          }
          println(
            "self.tokenTypeToASTClassMap[" 
            + ts.getTokenType() 
            + "] = " 
            + ts.getASTNodeType() );
        }
      }
    }

    if ( n==0 ) {
      println("self.tokenTypeToASTClassMap = None");
    }
    tabs--;
  }

  /** Generate the token types Java file */
  protected void genTokenTypes(TokenManager tm) throws IOException {
    // Open the token output Python file and set the currentOutput
    // stream
    // SAS: file open was moved to a method so a subclass can override
    //      This was mainly for the VAJ interface
    // setupOutput(tm.getName() + TokenTypesFileSuffix);

    tabs = 0;

    // Generate the header common to all Python files
    // genHeader();
    // Do not use printAction because we assume tabs==0
    // println(behavior.getHeaderAction(""));

    // Generate a definition for each token type
    Vector v = tm.getVocabulary();

    // Do special tokens manually
    println("SKIP                = antlr.SKIP");
    println("INVALID_TYPE        = antlr.INVALID_TYPE");
    println("EOF_TYPE            = antlr.EOF_TYPE");
    println("EOF                 = antlr.EOF");
    println("NULL_TREE_LOOKAHEAD = antlr.NULL_TREE_LOOKAHEAD");
    println("MIN_USER_TYPE       = antlr.MIN_USER_TYPE");

    for (int i = Token.MIN_USER_TYPE; i < v.size(); i++) 
    {
      String s = (String)v.elementAt(i);
      if (s != null) 
      {
        if (s.startsWith("\"")) 
        {
          // a string literal
          StringLiteralSymbol sl = (StringLiteralSymbol)tm.getTokenSymbol(s);
          if (sl == null) 
            antlrTool.panic("String literal " + s + " not in symbol table");

          if (sl.label != null) 
          {
            println(sl.label + " = " + i);
          }
          else 
          {
            String mangledName = mangleLiteral(s);
            if (mangledName != null) {
              // We were able to create a meaningful mangled token name
              println(mangledName + " = " + i);
              // if no label specified, make the label equal to the mangled name
              sl.label = mangledName;
            }
            else 
            {
              println("### " + s + " = " + i);
            }
          }
        }
        else if (!s.startsWith("<")) {
          println(s + " = " + i);
        }
      }
    }

    // Close the interface
    tabs--;

    exitIfError();
  }

  /** Get a string for an expression to generate creation of an AST subtree.
   * @param v A Vector of String, where each element is an expression in the target language yielding an AST node.
   */
  public String getASTCreateString(Vector v) {
    if (v.size() == 0) {
      return "";
    }
    StringBuffer buf = new StringBuffer();
    buf.append("antlr.make(");
    for (int i = 0; i < v.size(); i++) {
      buf.append(v.elementAt(i));
      if(i+1<v.size()) {
        buf.append(", ");
      }
    }
    buf.append(")");
    return buf.toString();
  }

  /** Get a string for an expression to generate creating of an AST node
   * @param atom The grammar node for which you are creating the node
   * @param str The arguments to the AST constructor
   */
  public String getASTCreateString(GrammarAtom atom, String astCtorArgs) 
  {
    if (atom != null && atom.getASTNodeType() != null) 
    {
      // they specified a type either on the reference or in tokens{} section
      return 
        "self.astFactory.create(" + astCtorArgs + ", " + atom.getASTNodeType() + ")";
    }
    else {
      // must be an action or something since not referencing an atom
      return getASTCreateString(astCtorArgs);
    }
  }

  /** Get a string for an expression to generate creating of an AST node.
   *  Parse the first (possibly only) argument looking for the token type.
   *  If the token type is a valid token symbol, ask for it's AST node type
   *  and add to the end if only 2 arguments.  The forms are #[T], #[T,"t"],
   *  and as of 2.7.2 #[T,"t",ASTclassname].
   *
   * @param str The arguments to the AST constructor
   */
  public String getASTCreateString(String astCtorArgs) {
    if ( astCtorArgs==null ) {
      astCtorArgs = "";
    }
    int nCommas = 0;
    for (int i=0; i<astCtorArgs.length(); i++) {
      if ( astCtorArgs.charAt(i)==',' ) {
        nCommas++;
      }
    }
    if ( nCommas<2 ) { // if 1 or 2 args
      int firstComma = astCtorArgs.indexOf(',');
      int lastComma = astCtorArgs.lastIndexOf(',');
      String tokenName = astCtorArgs;
      if ( nCommas>0 ) {
        tokenName = astCtorArgs.substring(0,firstComma);
      }
      TokenSymbol ts = grammar.tokenManager.getTokenSymbol(tokenName);
      if ( ts!=null ) {
        String astNodeType = ts.getASTNodeType();
        String emptyText = "";
        if ( nCommas==0 ) {
          // need to add 2nd arg of blank text for token text
          emptyText = ", \"\"";
        }
        if ( astNodeType!=null ) {
          return "self.astFactory.create(" + astCtorArgs + emptyText + ", " + astNodeType + ")";
        }
        // fall through and just do a regular create with cast on front
        // if necessary (it differs from default "AST").
      }
      if ( labeledElementASTType.equals("AST") ) {
        return "self.astFactory.create("+astCtorArgs+")";
      }
      return 
        "self.astFactory.create("+astCtorArgs+")";
    }
    // create default type or (since 2.7.2) 3rd arg is classname
    return "self.astFactory.create(" + astCtorArgs + ")";
  }
  
  protected String getLookaheadTestExpression(Lookahead[] look, int k) {
    StringBuffer e = new StringBuffer(100);
    boolean first = true;

    e.append("(");
    for (int i = 1; i <= k; i++) {
      BitSet p = look[i].fset;
      if (!first) {
        e.append(") and (");
      }
      first = false;

      // Syn preds can yield <end-of-syn-pred> (epsilon) lookahead.
      // There is no way to predict what that token would be.  Just
      // allow anything instead.
      if (look[i].containsEpsilon()) {
        e.append("True");
      }
      else {
        e.append(getLookaheadTestTerm(i, p)); 

      }
    }
    e.append(")");
    String s = e.toString();
    return s;
  }

  /**Generate a lookahead test expression for an alternate.  This
   * will be a series of tests joined by '&&' and enclosed by '()',
   * the number of such tests being determined by the depth of the lookahead.
   */
  protected String getLookaheadTestExpression(Alternative alt, int maxDepth) {
    int depth = alt.lookaheadDepth;
    if (depth == GrammarAnalyzer.NONDETERMINISTIC) 
    {
      // if the decision is nondeterministic, do the best we can: LL(k)
      // any predicates that are around will be generated later.
      depth = grammar.maxk;
    }

    if (maxDepth == 0) 
    {
      // empty lookahead can result from alt with sem pred
      // that can see end of token.  E.g., A : {pred}? ('a')? ;
      return "True";
    }

    return getLookaheadTestExpression(alt.cache, depth);
  }

  /**Generate a depth==1 lookahead test expression given the BitSet.
   * This may be one of:
   * 1) a series of 'x==X||' tests
   * 2) a range test using >= && <= where possible,
   * 3) a bitset membership test for complex comparisons
   * @param k The lookahead level
   * @param p The lookahead set for level k
   */
  protected String getLookaheadTestTerm(int k, BitSet p) {
    // Determine the name of the item to be compared
    String ts = lookaheadString(k);

    // Generate a range expression if possible
    int[] elems = p.toArray();
    if (elementsAreRange(elems)) {
      String s = getRangeExpression(k, elems);
      return s;
    }

    // Generate a bitset membership test if possible
    StringBuffer e;
    int degree = p.degree();
    if (degree == 0) {
      return "True";
    }

    if (degree >= bitsetTestThreshold) {
      int bitsetIdx = markBitsetForGen(p);
      return getBitsetName(bitsetIdx) + ".member(" + ts + ")";
    }

    // Otherwise, generate the long-winded series of "x==X||" tests
    e = new StringBuffer();
    for (int i = 0; i < elems.length; i++) {
      // Get the compared-to item (token or character value)
      String cs = getValueString(elems[i],true);

      // Generate the element comparison
      if (i > 0) e.append(" or ");
      e.append(ts);
      e.append("==");
      e.append(cs);   
    }
    String x = e.toString();
    return e.toString();
  }

  /** Return an expression for testing a contiguous renage of elements
   * @param k The lookahead level
   * @param elems The elements representing the set, usually from BitSet.toArray().
   * @return String containing test expression.
   */
  public String getRangeExpression(int k, int[] elems) {
    if (!elementsAreRange(elems)) {
      antlrTool.panic("getRangeExpression called with non-range");
    }
    int begin = elems[0];
    int end = elems[elems.length - 1];
    return
      "(" + lookaheadString(k) + " >= " + getValueString(begin,true) + " and " +
      lookaheadString(k) + " <= " + getValueString(end,true) + ")";
  }

  /** getValueString: get a string representation of a token or char value
   * @param value The token or char value
   */
  private String getValueString(int value,boolean wrap) {
    String cs;
    if (grammar instanceof LexerGrammar) {
      cs = charFormatter.literalChar(value);
      if(wrap)
        cs = "u'" + cs + "'";
      return cs;
    }

    // Parser or TreeParser => tokens ..
    TokenSymbol ts = 
      grammar.tokenManager.getTokenSymbolAt(
        value);
      
    if (ts == null) {
      cs = "" + value; // return token type as string
      return cs;
    }
    
    String tId = ts.getId();
    if (!(ts instanceof StringLiteralSymbol))
    {
      cs = tId;
      return cs;
    }

    // if string literal, use predefined label if any
    // if no predefined, try to mangle into LITERAL_xxx.
    // if can't mangle, use int value as last resort
    StringLiteralSymbol sl = (StringLiteralSymbol)ts;
    String label = sl.getLabel();
    if (label != null) {
      cs = label;
    }
    else 
    {
      cs = mangleLiteral(tId);
      if (cs == null) {
        cs = String.valueOf(value);
      }
    }
    return cs;
  }

  /**Is the lookahead for this alt empty? */
  protected boolean lookaheadIsEmpty(Alternative alt, int maxDepth) {
    int depth = alt.lookaheadDepth;
    if (depth == GrammarAnalyzer.NONDETERMINISTIC) {
      depth = grammar.maxk;
    }
    for (int i = 1; i <= depth && i <= maxDepth; i++) {
      BitSet p = alt.cache[i].fset;
      if (p.degree() != 0) {
        return false;
      }
    }
    return true;
  }

  
  private String lookaheadString(int k) {
    if (grammar instanceof TreeWalkerGrammar) {
      return "_t.getType()";
    }
    return "self.LA(" + k + ")";
  }

  /** Mangle a string literal into a meaningful token name.  This is
   * only possible for literals that are all characters.  The resulting
   * mangled literal name is literalsPrefix with the text of the literal
   * appended.
   * @return A string representing the mangled literal, or null if not possible.
   */
  private String mangleLiteral(String s) {
    String mangled = antlrTool.literalsPrefix;
    for (int i = 1; i < s.length() - 1; i++) {
      if (!Character.isLetter(s.charAt(i)) &&
          s.charAt(i) != '_') {
        return null;
      }
      mangled += s.charAt(i);
    }
    if (antlrTool.upperCaseMangledLiterals) {
      mangled = mangled.toUpperCase();
    }
    return mangled;
  }

  /** Map an identifier to it's corresponding tree-node variable.
   * This is context-sensitive, depending on the rule and alternative
   * being generated
   * @param idParam The identifier name to map
   * @return The mapped id (which may be the same as the input), or null if
   * the mapping is invalid due to duplicates
   */
  public String mapTreeId(String idParam, ActionTransInfo transInfo) {
    // if not in an action of a rule, nothing to map.
    if (currentRule == null) return idParam;

    boolean in_var = false;
    String id = idParam;
    if (grammar instanceof TreeWalkerGrammar) {
      if (!grammar.buildAST) {
        in_var = true;
      }
      // If the id ends with "_in", then map it to the input variable
      else if (id.length() > 3 && id.lastIndexOf("_in") == id.length() - 3) {
        // Strip off the "_in"
        id = id.substring(0, id.length() - 3);
        in_var = true;
      }
    }

    // Check the rule labels.  If id is a label, then the output
    // variable is label_AST, and the input variable is plain label.
    for (int i = 0; i < currentRule.labeledElements.size(); i++) {
      AlternativeElement elt = (AlternativeElement)currentRule.labeledElements.elementAt(i);
      if (elt.getLabel().equals(id)) {
        return in_var ? id : id + "_AST";
      }
    }

    // Failing that, check the id-to-variable map for the alternative.
    // If the id is in the map, then output variable is the name in the
    // map, and input variable is name_in
    String s = (String)treeVariableMap.get(id);
    if (s != null) {
      if (s == NONUNIQUE) {
        // There is more than one element with this id
        antlrTool.error("Ambiguous reference to AST element "+id+
                        " in rule "+currentRule.getRuleName());

        return null;
      }
      else if (s.equals(currentRule.getRuleName())) {
        // a recursive call to the enclosing rule is
        // ambiguous with the rule itself.
        antlrTool.error("Ambiguous reference to AST element "+id+
                        " in rule "+currentRule.getRuleName());
        return null;
      }
      else {
        return in_var ? s + "_in" : s;
      }
    }

    // Failing that, check the rule name itself.  Output variable
    // is rule_AST; input variable is rule_AST_in (treeparsers).
    if (id.equals(currentRule.getRuleName())) {
      String r = in_var ? id + "_AST_in" : id + "_AST";
      if (transInfo != null) {
        if (!in_var) {
          transInfo.refRuleRoot = r;
        }
      }
      return r;
    }
    else {
      // id does not map to anything -- return itself.
      return id;
    }
  }

  /** Given an element and the name of an associated AST variable,
   * create a mapping between the element "name" and the variable name.
   */
  private void mapTreeVariable(AlternativeElement e, String name) {
    // For tree elements, defer to the root
    if (e instanceof TreeElement) {
      mapTreeVariable(((TreeElement)e).root, name);
      return;
    }

    // Determine the name of the element, if any, for mapping purposes
    String elName = null;

    // Don't map labeled items
    if (e.getLabel() == null) {
      if (e instanceof TokenRefElement) {
        // use the token id
        elName = ((TokenRefElement)e).atomText;
      }
      else if (e instanceof RuleRefElement) {
        // use the rule name
        elName = ((RuleRefElement)e).targetRule;
      }
    }
    // Add the element to the tree variable map if it has a name
    if (elName != null) {
      if (treeVariableMap.get(elName) != null) {
        // Name is already in the map -- mark it as duplicate
        treeVariableMap.remove(elName);
        treeVariableMap.put(elName, NONUNIQUE);
      }
      else {
        treeVariableMap.put(elName, name);
      }
    }
  }

  /** Lexically process $var and tree-specifiers in the action.
   *  This will replace #id and #(...) with the appropriate
   *  function calls and/or variables etc...
   */
  protected String processActionForSpecialSymbols(String actionStr, int line,
                                                  RuleBlock currentRule,
                                                  ActionTransInfo tInfo) {
    if (actionStr == null || actionStr.length() == 0) 
      return null;

    if(isEmpty(actionStr))
      return "";

    // The action trans info tells us (at the moment) whether an
    // assignment was done to the rule's tree root.
    if (grammar == null) {
      // to be processd by PyCodeFmt??
      return actionStr;
    }

    // Create a lexer to read an action and return the translated version
    antlr.actions.python.ActionLexer lexer =
      new antlr.actions.python.ActionLexer(
        actionStr,
        currentRule,
        this,
        tInfo);

    lexer.setLineOffset(line);
    lexer.setFilename(grammar.getFilename());
    lexer.setTool(antlrTool);

    try {
      lexer.mACTION(true);
      actionStr = lexer.getTokenObject().getText();
    }
    catch (RecognitionException ex) {
      lexer.reportError(ex);
    }
    catch (TokenStreamException tex) {
      antlrTool.panic("Error reading action:" + actionStr);
    }
    catch (CharStreamException io) {
      antlrTool.panic("Error reading action:" + actionStr);
    }
    return actionStr;
  }

  static boolean isEmpty(String s) {
    char c;
    boolean ws = true;

    /* figure out whether there's something to be done */
    for(int i=0;ws && i<s.length();++i) {
      c = s.charAt(i);
      switch(c) {
        case '\n':
        case '\r':
        case ' ' :
        case '\t':
        case '\f':
        {
          break;
        }
        default: {
          ws = false;
        }
      }
    }
    return ws;
  }

  protected String processActionCode(String actionStr,int line) {
    /* shall process action code unconditionally */
    if(actionStr == null || isEmpty(actionStr))
      return "";

    antlr.actions.python.CodeLexer lexer =
      new antlr.actions.python.CodeLexer(
        actionStr,
        grammar.getFilename(),
        line,
        antlrTool
        );

    try {
      lexer.mACTION(true);
      actionStr = lexer.getTokenObject().getText();
    }
    catch (RecognitionException ex) {
      lexer.reportError(ex);
    }
    catch (TokenStreamException tex) {
      antlrTool.panic("Error reading action:" + actionStr);
    }
    catch (CharStreamException io) {
      antlrTool.panic("Error reading action:" + actionStr);
    }
    return actionStr;
  }

  protected void printActionCode(String actionStr,int line) {
    actionStr = processActionCode(actionStr,line);
    printAction(actionStr);
  }

  private void setupGrammarParameters(Grammar g) {
    if (g instanceof ParserGrammar) 
    {
      labeledElementASTType = "";
      if (g.hasOption("ASTLabelType")) 
      {
        Token tsuffix = g.getOption("ASTLabelType");
        if (tsuffix != null) {
          String suffix = StringUtils.stripFrontBack(tsuffix.getText(), "\"", "\"");
          if (suffix != null) {
            labeledElementASTType = suffix;
          }
        }
      }
      labeledElementType = "";
      labeledElementInit = "None";
      commonExtraArgs = "";
      commonExtraParams = "self";
      commonLocalVars = "";
      lt1Value = "self.LT(1)";
      exceptionThrown = "antlr.RecognitionException";
      throwNoViable = "raise antlr.NoViableAltException(self.LT(1), self.getFilename())";
      parserClassName = "Parser";
      if (g.hasOption("className")) 
      {
        Token tcname = g.getOption("className");
        if (tcname != null) {
          String cname = StringUtils.stripFrontBack(tcname.getText(), "\"", "\"");
          if (cname != null) {
            parserClassName = cname;
          }
        }
      }
      return;
    }

    if (g instanceof LexerGrammar) 
    {
      labeledElementType = "char ";
      labeledElementInit = "'\\0'";
      commonExtraArgs = "";
      commonExtraParams = "self, _createToken";
      commonLocalVars = "_ttype = 0\n        _token = None\n        _begin = self.text.length()";
      lt1Value = "self.LA(1)";
      exceptionThrown = "antlr.RecognitionException";
      throwNoViable = "self.raise_NoViableAlt(self.LA(1))";
      lexerClassName = "Lexer";
      if (g.hasOption("className")) 
      {
        Token tcname = g.getOption("className");
        if (tcname != null) {
          String cname = StringUtils.stripFrontBack(tcname.getText(), "\"", "\"");
          if (cname != null) {
            lexerClassName = cname;
          }
        }
      }
      return;
    }

    if (g instanceof TreeWalkerGrammar) 
    {
      labeledElementASTType = "";
      labeledElementType = "";
      if (g.hasOption("ASTLabelType")) {
        Token tsuffix = g.getOption("ASTLabelType");
        if (tsuffix != null) {
          String suffix = StringUtils.stripFrontBack(tsuffix.getText(), "\"", "\"");
          if (suffix != null) {
            labeledElementASTType = suffix;
            labeledElementType = suffix;
          }
        }
      }
      if (!g.hasOption("ASTLabelType")) {
        g.setOption("ASTLabelType", new Token(ANTLRTokenTypes.STRING_LITERAL, "<4>AST"));
      }
      labeledElementInit = "None";
      commonExtraArgs = "_t";
      commonExtraParams = "self, _t";
      commonLocalVars = "";
      lt1Value = "_t";
      exceptionThrown = "antlr.RecognitionException";
      throwNoViable = "raise antlr.NoViableAltException(_t)";
      treeWalkerClassName = "Walker";
      if (g.hasOption("className")) 
      {
        Token tcname = g.getOption("className");
        if (tcname != null) {
          String cname = StringUtils.stripFrontBack(tcname.getText(), "\"", "\"");
          if (cname != null) {
            treeWalkerClassName = cname;
          }
        }
      }
      return;
    }

    /* serious error */
    antlrTool.panic("Unknown grammar type");
  }

  /** This method exists so a subclass, namely VAJCodeGenerator,
   *  can open the file in its own evil way.  JavaCodeGenerator
   *  simply opens a text file...
   */
  public void setupOutput(String className) throws IOException {
    currentOutput = antlrTool.openOutputFile(className + ".py");
  }

  protected boolean isspace(char c) {
    boolean r = true;
    switch (c) {
      case '\n' :
      case '\r' :
      case ' '  :
      case '\t' :
        break;
      default:
        r = false;
        break;
    }
    return r;
  }

  protected void _printAction(String s) {
    if (s == null) {
      return;
    }

    char c;

    int offset; // shall be first no ws character in 's'. We are 
    // going   to  remove  at most this number of ws chars after
    // each newline. This will keep the block formatted as it is.

    // When going to figure out the offset, we  need to rese the
    // counter after each newline has been seen.

    // Skip leading newlines, tabs and spaces
    int start = 0;
    int end = s.length();
    boolean ws;

    offset = 0;
    ws = true;

    while (start < end && ws) 
    {
      c = s.charAt(start++);
      switch (c) {
        case '\n' :
          offset = start;
          break;
        case '\r':
          if( (start)<=end  && s.charAt(start) == '\n')
            start++;
          offset = start;
          break;
        case ' ' :
          break;
        case '\t':
        default:
          ws = false;
          break;
      }
    }
    if(ws == false) {
      start--;
    }
    offset = start - offset;

    // Skip leading newlines, tabs and spaces
    end = end - 1;
    while ((end > start) && isspace(s.charAt(end))) {
      end--;
    }

    boolean newline = false;
    int absorbed;

    for (int i = start; i <= end; ++i) 
    {
      c = s.charAt(i);
      switch (c) {
        case '\n':
          newline = true;
          break;
        case '\r':
          newline = true;
          if ((i+1) <= end && s.charAt(i+1) == '\n') {
            i++;
          }
          break;
        case '\t':
          System.err.println("warning: tab characters used in Python action");
          currentOutput.print("        ");
          break;
        case ' ':
          currentOutput.print(" ");
          break;
        default:
          currentOutput.print(c);
          break;
      }

      if (newline) 
      {
        currentOutput.print("\n");
        printTabs();
        absorbed = 0;
        newline = false;
        // Absorb leading whitespace
        for(i=i+1;i<=end;++i)
        {
          c = s.charAt(i);
          if (!isspace(c)) {
            i--;
            break;
          }
          switch(c) {
            case '\n' :
              newline = true;
              break;
            case '\r':
              if ((i+1) <= end && s.charAt(i+1) == '\n') {
                i++;
              }
              newline = true;
              break;
          }

          if(newline) 
          {
            currentOutput.print("\n");
            printTabs();
            absorbed = 0;
            newline = false;
            continue;
          }

          if(absorbed<offset) {
            absorbed++;
            continue;
          }

          /* stop this loop */
          break;
        }
      }
    }

    currentOutput.println();
  }

  protected void od(String s,int i,int end,String msg) {
    System.out.println(msg);
    char c;
    for(int j=i;j<=end;++j) 
    {
      c = s.charAt(j);
      switch(c) {
        case '\n':
          System.out.print(" nl ");
          break;
        case '\t':
          System.out.print(" ht ");
          break;
        case ' ':
          System.out.print(" sp ");
          break;
        default:
          System.out.print(" " + c + " ");
      }
    }
    System.out.println("");
  }

  protected void printAction(String s) {
    if (s != null) {
      printTabs();
      _printAction(s);
    }
  }

  protected void printGrammarAction(Grammar grammar) {
    println("### user action >>>");
    printAction(
      processActionForSpecialSymbols(
        grammar.classMemberAction.getText(), 
        grammar.classMemberAction.getLine(), 
        currentRule, 
        null)
      );
    println("### user action <<<");
  }

  protected void _printJavadoc(String s) {
    char c;
    int end = s.length();
    int start = 0;
    boolean newline = false;

    currentOutput.print("\n");
    printTabs();
    currentOutput.print("###");

    for (int i = start; i < end; ++i) 
    {
      c = s.charAt(i);
      switch (c) {
        case '\n':
          newline = true;
          break;
        case '\r':
          newline = true;
          if ((i+1) <= end && s.charAt(i+1) == '\n') {
            i++;
          }
          break;
        case '\t':
          currentOutput.print("\t");
          break;
        case ' ':
          currentOutput.print(" ");
          break;
        default:
          currentOutput.print(c);
          break;
      }

      if (newline) 
      {
        currentOutput.print("\n");
        printTabs();
        currentOutput.print("###");
        newline = false;
      }
    }

    currentOutput.println();
  }

  protected void genJavadocComment(Grammar g) {
    // print javadoc comment if any
    if (g.comment != null) {
      _printJavadoc(g.comment);
    }
  } 

  protected void genJavadocComment(RuleSymbol g) {
    // print javadoc comment if any
    if (g.comment != null) {
      _printJavadoc(g.comment);
    }
  }
}

