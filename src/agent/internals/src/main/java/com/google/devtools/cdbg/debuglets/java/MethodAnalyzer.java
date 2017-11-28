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

import static org.objectweb.asm.Opcodes.AASTORE;
import static org.objectweb.asm.Opcodes.ACC_NATIVE;
import static org.objectweb.asm.Opcodes.ACC_PUBLIC;
import static org.objectweb.asm.Opcodes.ACC_SYNCHRONIZED;
import static org.objectweb.asm.Opcodes.ASM6;
import static org.objectweb.asm.Opcodes.ASTORE;
import static org.objectweb.asm.Opcodes.BASTORE;
import static org.objectweb.asm.Opcodes.CASTORE;
import static org.objectweb.asm.Opcodes.DASTORE;
import static org.objectweb.asm.Opcodes.FASTORE;
import static org.objectweb.asm.Opcodes.GETFIELD;
import static org.objectweb.asm.Opcodes.GETSTATIC;
import static org.objectweb.asm.Opcodes.IASTORE;
import static org.objectweb.asm.Opcodes.INVOKESTATIC;
import static org.objectweb.asm.Opcodes.LASTORE;
import static org.objectweb.asm.Opcodes.NEW;
import static org.objectweb.asm.Opcodes.SASTORE;

import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Objects;
import java.util.Set;
import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.Handle;
import org.objectweb.asm.Label;
import org.objectweb.asm.MethodVisitor;

/**
 * Checks whether the methods of a given class are valid.
 *
 * <p>The rules to determine whether a method is valid are the following:
 * <ul>
 * <li>Loops are not permitted.</li>
 * <li>Instance or class variables cannot be modified.</li>
 * <li>Object references cannot be assigned.</li>
 * <li>Array references cannot be assigned.</li>
 * </ul>
 */
public class MethodAnalyzer {

  /**
   * Container class that has information about a method.
   */
  public static class MethodInfo {

    private final String owner;
    private final String name;
    private final String signature;

    /**
     * @param owner the class that owns the method that will be represented.
     * @param name the name of the method that will be represented.
     * @param signature the signature of the method that will be represented. This signature is the
     *        method's descriptor, according to the JVM specification.
     * @see <a
     *      href="http://docs.oracle.com/javase/specs/jvms/se7/html/jvms-4.html#jvms-4.3.3">http://docs.oracle.com/javase/specs/jvms/se7/html/jvms-4.html#jvms-4.3.3</a>
     */
    public MethodInfo(final String owner, final String name, final String signature) {
      this.owner = Objects.requireNonNull(owner, "Owner class can't be null");
      this.name = Objects.requireNonNull(name, "Method's name can't be null");
      this.signature = Objects.requireNonNull(signature, "Method's signature can't be null");
    }

    @Override
    public int hashCode() {
      final int prime = 31;
      int result = 1;
      result = prime * result + ((name == null) ? 0 : name.hashCode());
      result = prime * result + ((owner == null) ? 0 : owner.hashCode());
      result = prime * result + ((signature == null) ? 0 : signature.hashCode());
      return result;
    }

    @Override
    public boolean equals(Object obj) {
      if (obj == null) {
        return false;
      }
      if (getClass() != obj.getClass()) {
        return false;
      }
      MethodInfo other = (MethodInfo) obj;
      return owner.equals(other.owner) && name.equals(other.name)
          && signature.equals(other.signature);
    }

  }

  private static final Set<Integer> INVALID_ARRAY_STORE_OPCODES = new HashSet<>(
      Arrays.asList(IASTORE, LASTORE, FASTORE, DASTORE, AASTORE, BASTORE, CASTORE, SASTORE));

  /**
   * Helper class used to visit methods.
   *
   * <p>Methods overriden by this class will be called in any order, depending on the bytecode of
   * the method that is being visited. Some overriden methods might not be called. The only
   * exception is that {@link ClassVisitor#visitEnd()} is always called and it is the last method to
   * be called.
   */
  // TODO(jmozzino): Change to a whitelist approach, instead of a blacklist.
  private static class ValidatorMethodVisitor extends MethodVisitor {

    /**
     * Set containing the labels that have been seen, when scanning the bytecode from "top to
     * bottom"
     */
    private final Set<String> visitedLabels = new HashSet<>();
    private final String currentClass;
    private final MethodsFilter methodsFilter;

    private boolean methodIsImmutable = true;

    public ValidatorMethodVisitor(final MethodsFilter methodsFilter, final String currentClass) {
      super(ASM6);
      this.currentClass = currentClass;
      this.methodsFilter = methodsFilter;
    }

    @Override
    public void visitLabel(final Label label) {
      if (!methodIsImmutable) {
        return;
      }

      visitedLabels.add(label.toString());
    }

    @Override
    public void visitJumpInsn(int opcode, Label label) {
      if (!methodIsImmutable) {
        return;
      }

      // For now, jumps to previous labels flag the method as an invalid method since this can
      // introduce long running/infinite loops.
      if (visitedLabels.contains(label.toString())) {
        methodIsImmutable = false;
      }
    }

    @Override
    public void visitInsn(int opcode) {
      if (!methodIsImmutable) {
        return;
      }

      if (INVALID_ARRAY_STORE_OPCODES.contains(opcode)) {
        methodIsImmutable = false;
      }
    }

    // Handles GETFIELD, GETSTATIC, PUTFIELD, PUTSTATIC
    @Override
    public void visitFieldInsn(int opcode, String owner, String name, String desc) {
      if (!methodIsImmutable) {
        return;
      }

      // The only admitted opcodes are GETFIELD and GETSTATIC, since they don't change the state of
      // any objects.
      // However, if the opcode is GETSTATIC, the class that owns the field might be loaded, which
      // would change the state of the application. Therefore, only GETSTATIC to the current class
      // is allowed.
      // We don't worry about changing methodIsImmutable from false to true, because if it was
      // false, the method would've returned earlier.
      // TODO(jmozzino): Check if the class is loaded.
      methodIsImmutable =
          (opcode == GETFIELD || (opcode == GETSTATIC && owner.equals(currentClass)));
    }

    @Override
    public void visitInvokeDynamicInsn(String name, String desc, Handle bsm, Object... bsmArgs) {
      if (!methodIsImmutable) {
        return;
      }

      methodIsImmutable = false;
    }

    @Override
    public void visitMethodInsn(int opcode, String owner, String name, String desc, boolean itf) {
      if (!methodIsImmutable) {
        return;
      }

      methodIsImmutable = 
          (methodsFilter.isSafeMethod(owner, name, opcode == INVOKESTATIC) == Boolean.TRUE);
    }

    @Override
    public void visitVarInsn(int opcode, int var) {
      if (!methodIsImmutable) {
        return;
      }

      if (opcode == ASTORE) {
        methodIsImmutable = false;
      }
    }

    @Override
    public void visitTypeInsn(int opcode, String name) {
      if (!methodIsImmutable) {
        return;
      }

      if (opcode == NEW && !name.equals(currentClass)) {
        methodIsImmutable = false;
      }
    }

    @Override
    public void visitEnd() {
      if (!methodIsImmutable) {
        return;
      }
    }
  }

  /**
   * Helper class that visits the given class.
   *
   * <p>{@link ClassVisitor#visit(int, int, String, String, String, String[])} will be called first
   * and then {@link ClassVisitor#visitMethod(int, String, String, String, String[])} is called for
   * each method in the class. The only method that will be analyzed is the method identified by the
   * name and the parameters specified.
   */
  private static class AnalyzerClassVisitor extends ClassVisitor {

    private final String methodName;
    private final String methodSignature;
    private final MethodsFilter methodsFilter;

    private boolean isClassPublic;
    private boolean isMethodPublic;
    private String className;
    private ValidatorMethodVisitor methodVisitor;

    /**
     * @param methodName the name of the method to be analyzed.
     * @param methodSignature the method descriptor, according to the jvm specification. Eg.
     *
     *        <p>void m(); -> ()V
     *        <p>int m(); -> ()I
     *        <p>int m(int, int); -> (II)I
     *        <p>Object m(Object); -> (Ljava/lang/Object;)Ljava/lang/Object;
     *
     * @see <a
     *      href="http://docs.oracle.com/javase/specs/jvms/se7/html/jvms-4.html#jvms-4.3.3">http://docs.oracle.com/javase/specs/jvms/se7/html/jvms-4.html#jvms-4.3.3</a>
     */
    public AnalyzerClassVisitor(final MethodsFilter methodsFilter, final String methodName,
        final String methodSignature) {
      super(ASM6);
      this.methodName = Objects.requireNonNull(methodName, "Method name can't be null");
      this.methodSignature =
          Objects.requireNonNull(methodSignature, "Method signature can't be null");
      this.methodsFilter = Objects.requireNonNull(methodsFilter, "MethodsFilter can't be null");
    }
    
    public boolean isPublic() {
      return isClassPublic && isMethodPublic;
    }

    @Override
    public void visit(int version,
        int access,
        String name,
        String signature,
        String superName,
        String[] interfaces) {
      className = name;
      isClassPublic = ((access & ACC_PUBLIC) != 0);
    }

    @Override
    public MethodVisitor visitMethod(final int access, final String name, final String desc,
        final String signature, final String[] exception) {
      if (!methodName.equals(name) || !methodSignature.equals(desc)) {
        return null;
      }
      
      // Never treat native and synchronized methods as safe.
      if ((access & (ACC_NATIVE | ACC_SYNCHRONIZED)) != 0) {
        return null;
      }
      
      // We can be sure className has been initialized, because the api specifies that visit()
      // will always be called before visitMethod()
      isMethodPublic = ((access & ACC_PUBLIC) != 0);
      return methodVisitor = new ValidatorMethodVisitor(methodsFilter, className);
    }
  }

  /**
   * @param resource an input stream with the bytecode of the class whose methods should be
   *        validated.
   * @param name the name of the method.
   * @param signature the signature of the method
   * 
   * @throws IOException if an exception occurs while reading the class.
   * @return true if the method is considered immutable.
   */
  public static boolean analyzeMethod(MethodsFilter methodsFilter, InputStream resource,
      String name, String signature, boolean requirePublic) throws IOException {
    ClassReader classReader = new ClassReader(resource);
    AnalyzerClassVisitor visitor = new AnalyzerClassVisitor(methodsFilter, name, signature);
    classReader.accept(visitor, 0);
    if (visitor.methodVisitor == null) {
      // methodVisitor was not initialized. This means the method wasn't found
      // or the method is native or synchronized.
      return false;
    }
    
    return visitor.methodVisitor.methodIsImmutable && (!requirePublic || visitor.isPublic());
  }
}
