/**
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.devtools.cdbg.debuglets.java;

import static com.google.devtools.cdbg.debuglets.java.AgentLogger.infofmt;
import static com.google.devtools.cdbg.debuglets.java.AgentLogger.warnfmt;

import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;
import java.util.SortedMap;
import java.util.TreeMap;
import org.objectweb.asm.Attribute;
import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.Label;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;

/**
 * Loads and queries source information of all the classes defined in the same source file.
 *
 * <p>Note that while regular methods are always defined in a single source code block, this is not
 * the case with constructors and static initializers. A member or static variable initializer
 * can be defined anywhere in the class, but it is compiled to be part of constructor. For example:
 * {@code
 *   1. class A {
 *   2.   public A() {
 *   3.     System.out.println("I am constructor");
 *   4.   }
 *   5.   public void f() {
 *   6.     System.out.println("I am f");
 *   7.   }
 *   8.   private int x = 17;
 *   9. }
 * }
 * Class A will have two methods. Method f will have statements at lines 5 and 6 and the 
 * constructor at lines 2, 3 and 8.
 */
final class SourceFileMapper {
  /**
   * Maximum number of lines that the mapper is allowed to move the queried source line. For
   * example if we have a 20 line statement, the mapper will figure out that 15th line is still
   * part of the statement, but will consider line 16th as line without code. We need this
   * because Java debug information doesn't distinguish between empty spaces and multi-line
   * statements. In practice statements are unlikely to be longer than 2-3 lines because
   * function calls spanning multiple lines usually involve additional statements to compute
   * parameters.
   */
  private static final int MAX_LINE_ADJUSTMENT = 15;

  /**
   * Helper class implementing a visitor pattern for ASM library to query line number table.
   *
   * <p>Unimplemented visitor methods are handled by {@code ClassVisitor}. Since we are not
   * providing {@code ClassVisitor} to delegate, these visitor methods are doing nothing.
   */
  class MapperClassVisitor extends ClassVisitor {
    /**
     * Java signature of the currently visited class.
     */
    private String currentClassSignature;

    /**
     * True if the currently visited class is identified to be unsafe.
     **/
    private boolean isUnsafeClass;

    /**
     * Whether the currently visited class is a synthetic (i.e., compiler generated) class that has
     * no real source code corresponding to it.
     */
    private boolean isSyntheticClass;

    public MapperClassVisitor() {
      super(Opcodes.ASM6);
    }

    @Override
    public void visit(
        int version,
        int access,
        String name,
        String signature,
        String superName,
        String[] interfaces) {
      // JVMTI defines class signatures as "Lcom/prod/MyClass;".
      currentClassSignature = 'L' + name + ';';

      isSyntheticClass = (access & Opcodes.ACC_SYNTHETIC) != 0;

      // Blacklisted classes themselves are also unsafe.
      if (blacklistedClasses.contains(name)) {
        isUnsafeClass = true;
      }
    }

    @Override
    public void visitEnd() {
      if (isUnsafeClass) {
        unsafeClassSignatures.add(currentClassSignature);
      }
    }

    @Override
    public MethodVisitor visitMethod(
        final int access,
        final String name,
        final String descriptor,
        final String signature,
        final String[] exceptions) {
      return new MethodVisitor(Opcodes.ASM6) {
        @Override
        public void visitMethodInsn(
            int opcode,
            String owner,
            String name,
            String desc,
            boolean itf) {
          if (!isUnsafeClass && blacklistedClasses.contains(owner)) {
            isUnsafeClass = true;
          }
        }

        @Override
        public void visitLineNumber(int line, Label start) {
          if (isSyntheticClass) {
            // Ignore methods inside synthetic classes that are auto-generated by the compiler. This
            // prevents setting breakpoints in compiler-generated locations, and instead, forces
            // breakpoints to be placed at user visible source locations (e.g., enum switch map).
            return;
          }

          if (statements.containsKey(line)) {
            // There might be multiple statements corresponding the same source line number. Pick
            // the earliest since we want breakpoint to stop before the line is evaluated.
            return;
          }

          statements.put(
              line,
              new ResolvedSourceLocation(currentClassSignature, name, descriptor, line));
        }
      };
    }
  }

  /**
   * List of statements in the mapped source file sorted by line number. This includes all the
   * class resources provided in the constructor.
   */
  private SortedMap<Integer, ResolvedSourceLocation> statements = new TreeMap<>();

  /**
   * Classes whose methods are known to potentially create unsafe Java objects. Any class that calls
   * methods of these classes is also potentially unsafe, and hence, setting breakpoints inside such
   * a class will also not allowed.
   */
  private static final Set<String> blacklistedClasses =
      new HashSet<>(Arrays.asList("sun/misc/Unsafe", "com/google/protobuf/UnsafeUtil"));

  /**
   * Signatures of all the unsafe classes found in the mapped source file. We only record classes
   * that directly invoke methods from the blacklisted classes. This does not guarantee
   * safety/unsafety, but it is a good heuristic that covers most of the common cases.
   */
  private final Set<String> unsafeClassSignatures = new HashSet<>();

  /**
   * Loads set of specified classes. For each class we load the methods and the line number table.
   * Those are stored for subsequent queries.
   *
   * <p>No Java classes are actually loaded during this process.
   *
   * @param classResources resources of all Java classes defined in the mapped source file
   */
  public SourceFileMapper(Iterable<InputStream> classResources) {
    for (InputStream classResource : classResources) {
      try {
        loadClass(classResource);
      } catch (IOException e) {
        warnfmt(e, "Class file could not be loaded for source line mapping");
      }
    }
  }

  private void loadClass(InputStream resource) throws IOException {
    ClassReader classReader;
    classReader = new ClassReader(resource);

    // We don't need to parse any attributes while loading the class.
    Attribute[] attributes = new Attribute[0];

    classReader.accept(new MapperClassVisitor(), attributes, ClassReader.SKIP_FRAMES);
  }

  /**
   * Finds code statement at the specified source line. The scope of the lookup is the source
   * file corresponding with the top level class provided in constructor.
   */
  public ResolvedSourceLocation map(int lineNumber) {
    // Find closest statement at or prior to {@code lineNumber}.
    SortedMap<Integer, ResolvedSourceLocation> headMap = statements.headMap(lineNumber + 1);
    if (headMap.isEmpty()) {
      return noCodeAtLineError(lineNumber);
    }

    int candidateLineNumber = headMap.lastKey();
    ResolvedSourceLocation candidate = statements.get(candidateLineNumber);

    // Don't allow to move the line too far away.
    if (lineNumber - candidateLineNumber > MAX_LINE_ADJUSTMENT) {
      infofmt("Closest statement too far away, mapped line number: %d, statement: %d",
          lineNumber, candidateLineNumber);
      return noCodeAtLineError(lineNumber);
    }

    // If we hit a statement at different line than the one we queried, we need some more
    // checks. It is possible that {@code lineNumber} refers to some empty space between two
    // functions. In such cases we don't want to resolve the line to the last statement of
    // a prior function.
    if (lineNumber != candidateLineNumber) {
      // We test it by looking at the immediate next statement. We then only allow line number
      // adjustment if the next statement still belongs to the same function. Unfortunately this
      // condition is not perfect. We are possibly missing out a perfectly valid scenario, but
      // the chances are that the {@code lineNumber} points to some blank space.
      //
      // Possible valid case that we are ignoring here:
      //   1. class A {
      //   2.   private int myMember = computeMyMember(
      //   3.         1,
      //   4.         2);
      //        /* end of class, next function or next declaration */
      // Line 2 has the last statement of the class. If we try to resolve line 4, we will hit
      // one of the conditions below and return null.

      SortedMap<Integer, ResolvedSourceLocation> tail = statements.tailMap(candidateLineNumber + 1);
      if (tail.isEmpty()) {
        // Statement at {@code candidate} is the last one in the source file we are mapping.
        return noCodeAtLineError(lineNumber);
      }

      int nextStatementLineNumber = tail.firstKey();
      ResolvedSourceLocation nextStatement = statements.get(nextStatementLineNumber);

      if (!candidate.getClassSignature().equals(nextStatement.getClassSignature())
          || !candidate.getMethodName().equals(nextStatement.getMethodName())) {
        // The statement right after {@code candidate} is already in a different function.
        return noCodeAtLineError(lineNumber);
      }
    }

    // Verify that the target location is safe to set breakpoints on.
    if (unsafeClassSignatures.contains(candidate.getClassSignature())) {
      return unsafeClassError();
    }

    return candidate;
  }

  /**
   * Returns "no code at line $0" error in {@code ResolvedSourceLocation}.
   */
  private ResolvedSourceLocation noCodeAtLineError(int lineNumber) {
    return new ResolvedSourceLocation(
        new FormatMessage(Messages.NO_CODE_FOUND_AT_LINE, Integer.toString(lineNumber)));
  }

  /**
   * Returns "enclosing class contains unsafe Java code" error in {@code ResolvedSourceLocation}.
   */
  private ResolvedSourceLocation unsafeClassError() {
    return new ResolvedSourceLocation(
        new FormatMessage(Messages.BREAKPOINT_INSIDE_UNSAFE_CLASS));
  }
}
