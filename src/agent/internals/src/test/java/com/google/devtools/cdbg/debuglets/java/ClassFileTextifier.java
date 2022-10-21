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

import static org.objectweb.asm.ClassReader.SKIP_DEBUG;
import static org.objectweb.asm.ClassReader.SKIP_FRAMES;
import static org.objectweb.asm.Opcodes.ACC_NATIVE;
import static org.objectweb.asm.Opcodes.ACC_STATIC;
import static org.objectweb.asm.Opcodes.ASM7;

import com.google.common.base.Joiner;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.ArrayList;
import java.util.Arrays;
import org.objectweb.asm.AnnotationVisitor;
import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.FieldVisitor;
import org.objectweb.asm.Handle;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.util.Textifier;
import org.objectweb.asm.util.TraceClassVisitor;

/**
 * Prints out the instructions of a class for "nanojava_class_file_test" unit test.
 *
 * <p>Skips over fields, annotations and other elements that NanoJavaClassFile ignores.
 *
 * <p>Doesn't print any of the operands of INVOKEDYNAMIC since NanoJavaClassFile doesn't support
 * this instructions.
 */
public final class ClassFileTextifier {
  private static class MethodFilter extends MethodVisitor {
    public MethodFilter(MethodVisitor parentVisitor) {
      super(ASM7, parentVisitor);
    }

    @Override
    public void visitFrame(int type, int nLocal, Object[] local, int nStack, Object[] stack) {}
  }

  private static class ClassFilter extends ClassVisitor {
    public ClassFilter(ClassVisitor parentVisitor) {
      super(ASM7, parentVisitor);
    }

    @Override
    public void visit(
        int version,
        int access,
        String name,
        String signature,
        String superName,
        String[] interfaces) {
      superName = null;
      interfaces = null;
      access = 0;

      super.visit(version, access, name, signature, superName, interfaces);
    }

    @Override
    public FieldVisitor visitField(
        int access, String name, String desc, String signature, Object value) {
      return null;
    }

    @Override
    public AnnotationVisitor visitAnnotation(String desc, boolean visible) {
      return null;
    }

    @Override
    public MethodVisitor visitMethod(
        int access, String name, String desc, String signature, String[] exceptions) {
      access &= ACC_STATIC | ACC_NATIVE;
      return new MethodFilter(super.visitMethod(access, name, desc, signature, exceptions));
    }
  }

  static class FilteredTextifier extends Textifier {
    public FilteredTextifier() {
      super(ASM7);
    }

    @Override
    protected Textifier createTextifier() {
      return new FilteredTextifier();
    }

    @Override
    public Textifier visitMethod(
        int access, String name, String desc, String signature, String[] exceptions) {
      return super.visitMethod(access, name, desc, signature, null);
    }

    @Override
    public void visitInnerClass(String name, String outerName, String innerName, int access) {}

    @Override
    public void visitOuterClass(final String owner, final String name, final String desc) {}

    @Override
    public void visitNestMember(String nestMember) {}

    @Override
    public void visitNestHost(String nestHost) {}

    @Override
    public void visitInvokeDynamicInsn(String name, String desc, Handle bsm, Object... bsmArgs) {
      text.add(tab2);
      text.add("INVOKEDYNAMIC\n");
    }
  }

  public static String textify(Class<?> cls, boolean strip) throws IOException {
    return textifyFromClassReader(new ClassReader(cls.getName()), strip);
  }

  public static String textify(byte[] blob, boolean strip) throws IOException {
    return textifyFromClassReader(new ClassReader(blob), strip);
  }

  private static String textifyFromClassReader(ClassReader classReader, boolean strip) {
    StringWriter output = new StringWriter();
    ClassVisitor visitor =
        new ClassFilter(
            new TraceClassVisitor(null, new FilteredTextifier(), new PrintWriter(output)));
    classReader.accept(visitor, SKIP_DEBUG | SKIP_FRAMES);

    if (!strip) {
      return output.toString();
    }

    ArrayList<String> lines = new ArrayList<>(Arrays.asList(output.toString().split("\n")));

    for (int i = 0; i < lines.size(); ) {
      String line = lines.get(i);
      // Remove all lines with comments and attributes.
      if (line.matches("^\\s*//.*") || line.matches("^\\s*@L.*")) {
        lines.remove(i);
        continue;
      }

      ++i;
    }

    return Joiner.on('\n').join(lines);
  }
}
