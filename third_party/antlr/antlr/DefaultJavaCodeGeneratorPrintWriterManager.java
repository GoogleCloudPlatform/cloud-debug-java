package antlr;

import java.io.IOException;
import java.io.PrintWriter;
import java.util.HashMap;
import java.util.Map;

public class DefaultJavaCodeGeneratorPrintWriterManager implements JavaCodeGeneratorPrintWriterManager {
	private Grammar grammar;
	private PrintWriterWithSMAP smapOutput;
	private PrintWriter currentOutput;
	private Tool tool;
	private Map sourceMaps = new HashMap();
	private String currentFileName; 
	
    public PrintWriter setupOutput(Tool tool, Grammar grammar) throws IOException {
		return setupOutput(tool, grammar, null);
    }
    
    public PrintWriter setupOutput(Tool tool, String fileName) throws IOException {
		return setupOutput(tool, null, fileName);
    }
    
    public PrintWriter setupOutput(Tool tool, Grammar grammar, String fileName) throws IOException {
    	this.tool = tool;
		this.grammar = grammar;

		if (fileName == null)
			fileName = grammar.getClassName();
		
		smapOutput = new PrintWriterWithSMAP(tool.openOutputFile(fileName + ".java"));
		currentFileName = fileName + ".java";
		currentOutput = smapOutput;
        return currentOutput;
    }
    
    public void startMapping(int sourceLine) {
		smapOutput.startMapping(sourceLine);
    }

	public void startSingleSourceLineMapping(int sourceLine) {
		smapOutput.startSingleSourceLineMapping(sourceLine);
	}

	public void endMapping() {
		smapOutput.endMapping();
    }
    
    public void finishOutput() throws IOException {
        currentOutput.close();
        if (grammar != null) {
        	PrintWriter smapWriter;
			smapWriter = tool.openOutputFile(grammar.getClassName() + ".smap");
	    	String grammarFile = grammar.getFilename();
	    	grammarFile = grammarFile.replace('\\', '/');
	    	int lastSlash = grammarFile.lastIndexOf('/');
	   		if (lastSlash != -1)
	   			grammarFile = grammarFile.substring(lastSlash+1);
        	smapOutput.dump(smapWriter, grammar.getClassName(), grammarFile);
        	sourceMaps.put(currentFileName, smapOutput.getSourceMap());
        }
        currentOutput = null;    	
    }

	public Map getSourceMaps() {
		return sourceMaps;
	}

	public int getCurrentOutputLine()
	{
		return smapOutput.getCurrentOutputLine();
	}
}
