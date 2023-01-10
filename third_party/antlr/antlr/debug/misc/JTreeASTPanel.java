package antlr.debug.misc;

/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.cs.usfca.edu
 * Software rights: http://www.antlr.org/license.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.7/antlr/debug/misc/JTreeASTPanel.java#2 $
 */

import java.awt.*;
import javax.swing.*;
import javax.swing.tree.*;
import javax.swing.event.*;

public class JTreeASTPanel extends JPanel {
    JTree tree;

    public JTreeASTPanel(TreeModel tm, TreeSelectionListener listener) {
        // use a layout that will stretch tree to panel size
        setLayout(new BorderLayout());

        // Create tree
        tree = new JTree(tm);

        // Change line style
        tree.putClientProperty("JTree.lineStyle", "Angled");

        // Add TreeSelectionListener
        if (listener != null)
            tree.addTreeSelectionListener(listener);

        // Put tree in a scrollable pane's viewport
        JScrollPane sp = new JScrollPane();
        sp.getViewport().add(tree);

        add(sp, BorderLayout.CENTER);
    }
}
