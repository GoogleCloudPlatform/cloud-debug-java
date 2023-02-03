/*
 * Copyright 2014 Google LLC
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


package com.google.devtools.cdbg.debuglets.java; /* BPTAG: PACKAGE */

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.fail;
import static org.objectweb.asm.Opcodes.ACC_FINAL;
import static org.objectweb.asm.Opcodes.ACC_PUBLIC;
import static org.objectweb.asm.Opcodes.ACC_SUPER;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.security.NoSuchAlgorithmException;
import java.util.HashMap;
import java.util.Map;
import java.util.Properties;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.objectweb.asm.ClassWriter;

/**
 * Unit test for {@link com.google.devtools.cdbg.debuglets.java.ClassPathLookup} class.
 *
 * <p>This test class feeds itself to the product code as an input. It then verifies that various
 * statements within the class are resolved as expected.
 */
@RunWith(JUnit4.class)
public class ClassPathLookupTest { // BPTAG: CLASS_PATH_LOOKUP_TEST_OPEN
  private static final String SIGNATURE_BASE =
      "com/google/devtools/cdbg/debuglets/java/ClassPathLookupTest";

  /** Builds classes dynamically for test purposes. */
  private static class DynamicClassLoader extends ClassLoader {
    public DynamicClassLoader() {
      super(ClassPathLookupTest.class.getClassLoader());
    }

    public Class<?> defineDynamicClass(String internalName, byte[] classData) {
      return defineClass(internalName.replace('/', '.'), classData, 0, classData.length);
    }
  }

  /**
   * This class is only used as an input to the product code. It doesn't have any test related
   * logic.
   */
  class InnerClass {
    public int doSomething() {
      return 2; /* BPTAG: INNER_CLASS_DO_SOMETHING_RETURN */
    } /* BPTAG: INNER_CLASS_DO_SOMETHING_CLOSE */
    /* BPTAG: INNER_CLASS_EMPTY_SPACE */
    public int doSomethingElse() {
      return 3;
    }
  }

  /**
   * This class is only used as an input to the product code. It doesn't have any test related
   * logic.
   */
  static class StaticClass {
    static {
      fail("Class initialization not expected"); /* BPTAG: STATIC_CLASS_CLINIT_STATEMENT */
    }

    class StaticInnerClass {
      public StaticInnerClass() {
        fail("Class constructor not expected"); /* BPTAG: STATIC_INNER_CLASS_CONSTRUCTOR */
      }
    }

    static class StaticStaticClass {
      static { // BPTAG: STATIC2_CLASS_CLINIT_OPEN
        fail("Class initialization not expected"); /* BPTAG: STATIC2_CLASS_CLINIT_STATEMENT */
      }
    }
  }

  enum Vegetable {
    KALE,
    ARUGULA
  }

  int getVegetableNumber(Vegetable v) {
    // Java compiler generates a synthetic class called ClassPathLookupTest$1 for the switch
    // statement, and the generated class also has line table information for the next line.
    switch (v) { // BPTAG: IGNORE_SYNTHETIC_CLASS
      case KALE:
        return 1;

      case ARUGULA:
        return 2;

      default:
        return -1;
    }
  }

  // It takes a relatively long time to instantiate ClassPathLookup, so we only do it once.
  // Tests should not interfere with each other, since ClassPathLookup is stateless.
  private static final ClassPathLookup DEFAULT_CLASS_PATH_LOOKUP = new ClassPathLookup(true, null);

  @Rule public TemporaryFolder temporaryFolder1 = new TemporaryFolder();

  @Rule public TemporaryFolder temporaryFolder2 = new TemporaryFolder();

  static {
    // See tests/agent/test_logger.cc
    System.loadLibrary("test_logger");
  }

  public ClassPathLookupTest() {
    myInteger++;
  }

  @Test
  public void resolveSourceLocationPositive() throws BreakpointTagException {
    String[] sourceFiles = // BPTAG: RESOLVE_POSITIVE
        new String[] { 
          SIGNATURE_BASE + ".java",
          "/home/" + SIGNATURE_BASE + ".java",
          "/home/myproj/" + SIGNATURE_BASE + ".java",
          "ClassPathLookupTest.java",
          "cdbg/ClassPathLookupTest.java",
        };

    for (String sourceFile : sourceFiles) {
      // Very basic lookup.
      verifyResolveSourceLocation(
          DEFAULT_CLASS_PATH_LOOKUP,
          sourceFile,
          "RESOLVE_POSITIVE",
          SIGNATURE_BASE,
          "resolveSourceLocationPositive",
          "()V",
          "RESOLVE_POSITIVE");

      // Verify that we can find source line in a constructor.
      verifyResolveSourceLocation(
          DEFAULT_CLASS_PATH_LOOKUP,
          sourceFile,
          "MY_INTEGER_BASE",
          SIGNATURE_BASE,
          "<init>",
          "()V",
          "MY_INTEGER_BASE");

      // Verify that we can find source line in an inner class.
      verifyResolveSourceLocation(
          DEFAULT_CLASS_PATH_LOOKUP,
          sourceFile,
          "INNER_CLASS_DO_SOMETHING_RETURN",
          SIGNATURE_BASE + "$InnerClass",
          "doSomething",
          "()I",
          "INNER_CLASS_DO_SOMETHING_RETURN");

      // Verify that we can find source line in a static class.
      verifyResolveSourceLocation(
          DEFAULT_CLASS_PATH_LOOKUP,
          sourceFile,
          "STATIC_CLASS_CLINIT_STATEMENT",
          SIGNATURE_BASE + "$StaticClass",
          "<clinit>",
          "()V",
          "STATIC_CLASS_CLINIT_STATEMENT");

      // Verify that we can find source line in a static class inside another static class.
      verifyResolveSourceLocation(
          DEFAULT_CLASS_PATH_LOOKUP,
          sourceFile,
          "STATIC_INNER_CLASS_CONSTRUCTOR",
          SIGNATURE_BASE + "$StaticClass$StaticInnerClass",
          "<init>",
          "(Lcom/google/devtools/cdbg/debuglets/java/ClassPathLookupTest$StaticClass;)V",
          "STATIC_INNER_CLASS_CONSTRUCTOR");

      // Verify that we can find source line in a static initializer.
      verifyResolveSourceLocation(
          DEFAULT_CLASS_PATH_LOOKUP,
          sourceFile,
          "STATIC2_CLASS_CLINIT_STATEMENT",
          SIGNATURE_BASE + "$StaticClass$StaticStaticClass",
          "<clinit>",
          "()V",
          "STATIC2_CLASS_CLINIT_STATEMENT");

      // Verify that we do not match the source line information inside the synthetic class
      // generated for the switch statement.
      verifyResolveSourceLocation(
          DEFAULT_CLASS_PATH_LOOKUP,
          sourceFile,
          "IGNORE_SYNTHETIC_CLASS",
          SIGNATURE_BASE,
          "getVegetableNumber",
          "(Lcom/google/devtools/cdbg/debuglets/java/ClassPathLookupTest$Vegetable;)I",
          "IGNORE_SYNTHETIC_CLASS");
    }
  }

  @Test
  public void resolveSourceLocationExternalJar() throws BreakpointTagException {
    File jarFile = TestDataPath.get("DynamicJar.jar");
    String[] extraClassPath = new String[] {"badpath1", jarFile.getAbsolutePath(), "badpath2"};

    ClassPathLookup classPathLookupExtra = new ClassPathLookup(true, extraClassPath);

    String sourcePath = "org/myorg/myprod/test/DynamicJarClass.java";
    int line =
        BreakpointDefinition.createBuilder("DynamicJarClass.java", "DYNAMIC_JAR_CLASS_CONSTRUCTOR")
            .getLocation()
            .getLine();

    assertNotNull(
        DEFAULT_CLASS_PATH_LOOKUP.resolveSourceLocation(sourcePath, line).getErrorMessage());

    ResolvedSourceLocation location = classPathLookupExtra.resolveSourceLocation(sourcePath, line);

    assertNotNull(location);
    assertNull(location.getErrorMessage());
    assertEquals("Lorg/myorg/myprod/test/DynamicJarClass;", location.getClassSignature());
    assertEquals("<init>", location.getMethodName());
    assertEquals("()V", location.getMethodDescriptor());
  }

  @Test
  public void noDefaultClassPath() throws BreakpointTagException {
    File jarFile = TestDataPath.get("DynamicJar.jar");
    String[] extraClassPath = new String[] {jarFile.getAbsolutePath()};

    ClassPathLookup classPathLookup = new ClassPathLookup(false, extraClassPath);

    String sourcePath;
    int line;
    ResolvedSourceLocation location;

    // Verify that DynamicJarClass from extraClassPath can be found.
    sourcePath = "org/myorg/myprod/test/DynamicJarClass.java";
    line =
        BreakpointDefinition.createBuilder("DynamicJarClass.java", "DYNAMIC_JAR_CLASS_CONSTRUCTOR")
            .getLocation()
            .getLine();
    location = classPathLookup.resolveSourceLocation(sourcePath, line);
    assertNotNull(location);
    assertNull(location.getErrorMessage());

    // Verify that this class (which is in default class path doesn't resolve).
    sourcePath = SIGNATURE_BASE + ".java";
    line = getTagLineNumber("NO_DEFAULT_CLASS_PATH"); /* BPTAG: NO_DEFAULT_CLASS_PATH */
    location = classPathLookup.resolveSourceLocation(sourcePath, line);
    assertNotNull(location);
    assertNotNull(location.getErrorMessage());

    // Now just make sure that the same location can be resolved with default instance.
    location = DEFAULT_CLASS_PATH_LOOKUP.resolveSourceLocation(sourcePath, line);
    assertNotNull(location);
    assertNull(location.getErrorMessage());
  }

  // Deliberately define this member variable in the middle of the source file to make
  // constructor line table more interesting.
  @SuppressWarnings("unused") /* BPTAG: MY_INTEGER_BASE */
  private int myInteger = 1 + 2 // BPTAG: MY_INTEGER_2
          + Math.abs(-4);

  @Test
  public void resolveSourceLocationAdjust() throws BreakpointTagException {
    final String sourceFile = SIGNATURE_BASE + ".java";

    verifyResolveSourceLocation( // BPTAG: ADJUST_1_BASE
        DEFAULT_CLASS_PATH_LOOKUP, /* BPTAG: ADJUST_1_A */
        sourceFile,
        "ADJUST_1_A",
        SIGNATURE_BASE,
        "resolveSourceLocationAdjust",
        "()V",
        "ADJUST_1_BASE"); /* BPTAG: ADJUST_1_B */
    /* BPTAG: ADJUST_1_C */
    verifyResolveSourceLocation( // BPTAG: ADJUST_2
        DEFAULT_CLASS_PATH_LOOKUP,
        sourceFile,
        "ADJUST_1_B",
        SIGNATURE_BASE,
        "resolveSourceLocationAdjust",
        "()V",
        "ADJUST_1_BASE");

    verifyResolveSourceLocation(
        DEFAULT_CLASS_PATH_LOOKUP,
        sourceFile,
        "ADJUST_1_C",
        SIGNATURE_BASE,
        "resolveSourceLocationAdjust",
        "()V",
        "ADJUST_1_BASE");

    verifyResolveSourceLocation(
        DEFAULT_CLASS_PATH_LOOKUP,
        sourceFile,
        "ADJUST_2",
        SIGNATURE_BASE,
        "resolveSourceLocationAdjust",
        "()V",
        "ADJUST_2");

    verifyResolveSourceLocation(
        DEFAULT_CLASS_PATH_LOOKUP,
        sourceFile,
        "MY_INTEGER_2",
        SIGNATURE_BASE,
        "<init>",
        "()V",
        "MY_INTEGER_BASE");
  }

  @Test
  public void resolveSourceLocationMaxAdjustment() throws BreakpointTagException {
    final String sourceFile = SIGNATURE_BASE + ".java";

    @SuppressWarnings("unused")
    int x = /* BPTAG: MAX_ADJUSTMENT_BASE */
        1
            + 2
            + 3
            + 4
            + 5
            + 6
            + 7
            + 8
            + 9
            + 10
            + 11
            + 12
            + 13
            + 14
            + 15 /* BPTAG: MAX_ADJUSTMENT_15 */
            + 16; /* BPTAG: MAX_ADJUSTMENT_16 */

    verifyResolveSourceLocation(
        DEFAULT_CLASS_PATH_LOOKUP,
        sourceFile,
        "MAX_ADJUSTMENT_15",
        SIGNATURE_BASE,
        "resolveSourceLocationMaxAdjustment",
        "()V",
        "MAX_ADJUSTMENT_BASE");

    int lineNumber = getTagLineNumber("MAX_ADJUSTMENT_16");
    ResolvedSourceLocation location =
        DEFAULT_CLASS_PATH_LOOKUP.resolveSourceLocation(sourceFile, lineNumber);

    assertNotNull("errorMessage is not null", location.getErrorMessage());
    assertEquals(Messages.NO_CODE_FOUND_AT_LINE, location.getErrorMessage().getFormat());
    assertEquals(1, location.getErrorMessage().getParameters().length);
    assertEquals("" + lineNumber, location.getErrorMessage().getParameters()[0]);
  }
  /* BPTAG: TEST_EMPTY_SPACE */
  @Test
  public void resolveSourceLocationNegative() throws BreakpointTagException {
    Map<String, String> sourceFiles = new HashMap<>();
    sourceFiles.put(SIGNATURE_BASE + ".java", null);
    sourceFiles.put(SIGNATURE_BASE + ".html", Messages.UNSUPPORTED_SOURCE_FILE_EXTENSION);
    sourceFiles.put(SIGNATURE_BASE + "A.java", Messages.SOURCE_FILE_NOT_FOUND_IN_EXECUTABLE);
    sourceFiles.put(SIGNATURE_BASE + "A.scala", Messages.SOURCE_FILE_NOT_FOUND_IN_EXECUTABLE);
    sourceFiles.put(SIGNATURE_BASE + ".java.java", Messages.SOURCE_FILE_NOT_FOUND_IN_EXECUTABLE);
    sourceFiles.put(SIGNATURE_BASE + ".scala.scala", Messages.SOURCE_FILE_NOT_FOUND_IN_EXECUTABLE);
    sourceFiles.put("B/" + SIGNATURE_BASE + "A.java", Messages.SOURCE_FILE_NOT_FOUND_IN_EXECUTABLE);
    sourceFiles.put(
        "B/" + SIGNATURE_BASE + "A.scala", Messages.SOURCE_FILE_NOT_FOUND_IN_EXECUTABLE);
    sourceFiles.put("", Messages.UNDEFINED_BREAKPOINT_LOCATION);
    sourceFiles.put(null, Messages.UNDEFINED_BREAKPOINT_LOCATION);
    sourceFiles.put("sdfasdf", Messages.UNSUPPORTED_SOURCE_FILE_EXTENSION);
    sourceFiles.put(".java", Messages.SOURCE_FILE_NOT_FOUND_IN_EXECUTABLE);
    sourceFiles.put(".scala", Messages.SOURCE_FILE_NOT_FOUND_IN_EXECUTABLE);
    sourceFiles.put("x.java", Messages.SOURCE_FILE_NOT_FOUND_IN_EXECUTABLE);
    sourceFiles.put("x.scala", Messages.SOURCE_FILE_NOT_FOUND_IN_EXECUTABLE);

    String[] tags =
        new String[] {
          "PACKAGE",
          "CLASS_PATH_LOOKUP_TEST_OPEN",
          "INNER_CLASS_DO_SOMETHING_CLOSE",
          "INNER_CLASS_EMPTY_SPACE",
          "TEST_EMPTY_SPACE",
          "BEYOND_END"
        };

    for (String sourceFile : sourceFiles.keySet()) {
      for (String tag : tags) {
        int lineNumber = getTagLineNumber(tag);

        ResolvedSourceLocation location =
            DEFAULT_CLASS_PATH_LOOKUP.resolveSourceLocation(sourceFile, lineNumber);

        if (location.getErrorMessage() == null) {
          fail(
              String.format(
                  "resolveSourceLocation(%s, %d) did not return error, "
                      + "resolved class: %s, method: %s, adjusted line: %d",
                  sourceFile,
                  lineNumber,
                  location.getClassSignature(),
                  location.getMethodName(),
                  location.getAdjustedLineNumber()));
        }

        String msg = String.format("resolveSourceLocation(%s, %d)", sourceFile, lineNumber);

        String sourceFileError = sourceFiles.get(sourceFile);
        if (sourceFileError != null) {
          assertEquals(msg, sourceFileError, location.getErrorMessage().getFormat());
          assertEquals(msg, 0, location.getErrorMessage().getParameters().length);
        } else {
          assertEquals(msg, Messages.NO_CODE_FOUND_AT_LINE, location.getErrorMessage().getFormat());
          assertEquals(msg, 1, location.getErrorMessage().getParameters().length);
          assertEquals(msg, "" + lineNumber, location.getErrorMessage().getParameters()[0]);
        }
      }
    }
  }

  @Test
  public void computeDebuggeeUniquifierSuccess() throws NoSuchAlgorithmException {
    assertThat(DEFAULT_CLASS_PATH_LOOKUP.computeDebuggeeUniquifier("abc")).isNotEmpty();
  }

  @Test
  public void uniquifierIVs() throws NoSuchAlgorithmException {
    assertEquals(
        DEFAULT_CLASS_PATH_LOOKUP.computeDebuggeeUniquifier(""),
        DEFAULT_CLASS_PATH_LOOKUP.computeDebuggeeUniquifier(""));
    assertEquals(
        DEFAULT_CLASS_PATH_LOOKUP.computeDebuggeeUniquifier("abc"),
        DEFAULT_CLASS_PATH_LOOKUP.computeDebuggeeUniquifier("abc"));
    assertFalse(
        DEFAULT_CLASS_PATH_LOOKUP
            .computeDebuggeeUniquifier("abc")
            .equals(DEFAULT_CLASS_PATH_LOOKUP.computeDebuggeeUniquifier("abd")));
    assertFalse(
        DEFAULT_CLASS_PATH_LOOKUP
            .computeDebuggeeUniquifier("abc")
            .equals(DEFAULT_CLASS_PATH_LOOKUP.computeDebuggeeUniquifier("Abc")));
  }

  @Test
  public void readGitPropertiesResourceSuccess() throws IOException {
    Properties props = new Properties();

    // Populate two properties we really care about
    props.setProperty("git.commit.id", "SomeId");
    props.setProperty("git.remote.origin.url", "SomeUrl");

    File propertiesFile = temporaryFolder1.newFile("my.file");
    props.store(new FileOutputStream(propertiesFile), "This is an optional header comment string");

    String expectedResult =
        "{\n"
            + " \"git\": {\n"
            + "  \"revisionId\": \"SomeId\",\n"
            + "  \"url\": \"SomeUrl\"\n"
            + " }\n"
            + "}";

    String[] extraClassPath = new String[] {temporaryFolder1.getRoot().getAbsolutePath()};

    ClassPathLookup classPathLookup = new ClassPathLookup(true, extraClassPath);
    assertThat(classPathLookup.readGitPropertiesResourceAsSourceContext("my.file")).hasLength(1);
    assertEquals(
        expectedResult, classPathLookup.readGitPropertiesResourceAsSourceContext("my.file")[0]);
  }

  @Test
  public void readGitPropertiesResourceFailureWithoutCommit() throws IOException {
    Properties props = new Properties();

    // Populate url property but miss commit id
    props.setProperty("git.remote.origin.url", "SomeUrl");

    File propertiesFile = temporaryFolder1.newFile("my.file");
    props.store(new FileOutputStream(propertiesFile), "This is an optional header comment string");

    String[] extraClassPath = new String[] {temporaryFolder1.getRoot().getAbsolutePath()};

    ClassPathLookup classPathLookup = new ClassPathLookup(true, extraClassPath);
    assertThat(classPathLookup.readGitPropertiesResourceAsSourceContext("my.file")).isEmpty();
  }

  @Test
  public void readGitPropertiesResourceFailureWithoutUrl() throws IOException {
    Properties props = new Properties();

    // Populate commit id property but miss url
    props.setProperty("git.commit.id", "SomeId");

    File propertiesFile = temporaryFolder1.newFile("my.file");
    props.store(new FileOutputStream(propertiesFile), "This is an optional header comment string");

    String[] extraClassPath = new String[] {temporaryFolder1.getRoot().getAbsolutePath()};

    ClassPathLookup classPathLookup = new ClassPathLookup(true, extraClassPath);
    assertThat(classPathLookup.readGitPropertiesResourceAsSourceContext("my.file")).isEmpty();
  }

  @Test
  public void readApplicationResourceSuccessSingle() throws IOException {
    String content = "";
    for (int i = 0; i < 10000; ++i) {
      content += (char) ('A' + (i % 25));
    }

    File sourceContextFile = temporaryFolder1.newFile("my.file");
    try (PrintWriter writer = new PrintWriter(sourceContextFile)) {
      writer.write(content);
    }

    String[] extraClassPath = new String[] {temporaryFolder1.getRoot().getAbsolutePath()};

    ClassPathLookup classPathLookup = new ClassPathLookup(true, extraClassPath);
    assertEquals(1, classPathLookup.readApplicationResource("my.file").length);
    assertEquals(content, classPathLookup.readApplicationResource("my.file")[0]);
  }

  @Test
  public void readApplicationResourceSuccessFolder() throws IOException {
    String content = "Vertebrates are animals that are members of the subphylum Vertebrata";

    File folder = temporaryFolder1.newFolder("META-INF");

    File sourceContextFile = new File(folder, "source-context.json");
    sourceContextFile.createNewFile();

    try (PrintWriter writer = new PrintWriter(sourceContextFile)) {
      writer.write(content);
    }

    String[] extraClassPath = new String[] {temporaryFolder1.getRoot().getAbsolutePath()};
    String resourcePath = "META-INF/source-context.json";

    ClassPathLookup classPathLookup = new ClassPathLookup(true, extraClassPath);
    assertEquals(1, classPathLookup.readApplicationResource(resourcePath).length);
    assertEquals(content, classPathLookup.readApplicationResource(resourcePath)[0]);
  }

  @Test
  public void readApplicationResourceSuccessMultiple() throws IOException {
    File myFile1 = temporaryFolder1.newFile("my.file");
    try (PrintWriter writer = new PrintWriter(myFile1)) {
      writer.write("abc");
    }

    File myFile2 = temporaryFolder2.newFile("my.file");
    try (PrintWriter writer = new PrintWriter(myFile2)) {
      writer.write("def");
    }

    File folder = temporaryFolder2.newFolder("folder");
    File myFileInFolder = new File(folder, "my.file");
    try (PrintWriter writer = new PrintWriter(myFileInFolder)) {
      writer.write("unexpected");
    }

    String[] extraClassPath =
        new String[] {
          temporaryFolder1.getRoot().getAbsolutePath(), temporaryFolder2.getRoot().getAbsolutePath()
        };

    ClassPathLookup classPathLookup = new ClassPathLookup(true, extraClassPath);
    assertEquals(2, classPathLookup.readApplicationResource("my.file").length);
    assertEquals("abc", classPathLookup.readApplicationResource("my.file")[0]);
    assertEquals("def", classPathLookup.readApplicationResource("my.file")[1]);
  }

  @Test
  public void readApplicationResourceDuplicateClassPathEntries() throws IOException {
    File sourceContextFile = temporaryFolder1.newFile("my.file");
    try (PrintWriter writer = new PrintWriter(sourceContextFile)) {
      writer.write("abc");
    }

    String[] extraClassPath =
        new String[] {
          temporaryFolder1.getRoot().getAbsolutePath(), temporaryFolder1.getRoot().getAbsolutePath()
        };

    ClassPathLookup classPathLookup = new ClassPathLookup(true, extraClassPath);
    assertEquals(1, classPathLookup.readApplicationResource("my.file").length);
  }

  @Test
  public void readApplicationResourceFailure() {
    ClassPathLookup classPathLookup = new ClassPathLookup(true, null);
    assertEquals(0, classPathLookup.readApplicationResource("my.file").length);
  }

  @Test
  public void readClassFileFailure() throws ClassNotFoundException, IOException {
    String internalName = "com/google/dynamic/ReadClassFileFailureTest";
    ClassWriter classWriter = new ClassWriter(0);
    classWriter.visit(
        51, ACC_PUBLIC | ACC_FINAL | ACC_SUPER, internalName, null, "java/lang/Object", null);
    classWriter.visitEnd();

    DynamicClassLoader classLoader = new DynamicClassLoader();
    Class<?> dynamicClass = classLoader.defineDynamicClass(internalName, classWriter.toByteArray());

    assertThrows(ClassNotFoundException.class, () -> ClassPathLookup.readClassFile(dynamicClass));
  }

  @Test
  public void readClassFileSuccess() throws ClassNotFoundException, IOException {
    Class<?>[] classesToLoad =
        new Class<?>[] {Integer.class, StringBuilder.class, ClassPathLookup.class};

    for (Class<?> classToLoad : classesToLoad) {
      assertThat(ClassPathLookup.readClassFile(classToLoad).length).isAtLeast(1000);
    }
  }

  @Test
  public void findClassesByName() {
    assertThat(DEFAULT_CLASS_PATH_LOOKUP.findClassesByName("ClassPathLookupTest"))
        .asList()
        .containsExactly("Lcom/google/devtools/cdbg/debuglets/java/ClassPathLookupTest;");
  }

  @Test
  public void emptyAutoClassPath() {
    assertThat(ClassPathLookup.findExtraClassPath()).isEmpty();
  }

  @Test
  public void directoryNotExistAutoClassPath() {
    try {
      System.setProperty("catalina.base", "/tomcat");
      System.setProperty("jetty.base", "/jetty");

      assertThat(ClassPathLookup.findExtraClassPath()).isEmpty();
    } finally {
      System.clearProperty("catalina.base");
      System.clearProperty("jetty.base");
    }
  }

  @Test
  public void completeAutoClassPath() {
    File root = temporaryFolder1.getRoot();
    File tomcatRoot = new File(root, "tomcat");
    File jettyRoot = new File(root, "jetty");

    String[] suffixes = new String[] {"webapps/ROOT/WEB-INF/lib", "webapps/ROOT/WEB-INF/classes"};
    for (File prefix : new File[] {tomcatRoot, jettyRoot}) {
      for (String suffix : suffixes) {
        new File(prefix, suffix).mkdirs();
      }
    }

    try {
      System.setProperty("catalina.base", tomcatRoot.toString());
      System.setProperty("jetty.base", jettyRoot.toString());

      assertThat(ClassPathLookup.findExtraClassPath())
          .asList()
          .containsExactly(
              tomcatRoot + "/webapps/ROOT/WEB-INF/lib",
              tomcatRoot + "/webapps/ROOT/WEB-INF/classes",
              jettyRoot + "/webapps/ROOT/WEB-INF/lib",
              jettyRoot + "/webapps/ROOT/WEB-INF/classes");
    } finally {
      System.clearProperty("catalina.base");
      System.clearProperty("jetty.base");
    }
  }

  private int getTagLineNumber(String tag) throws BreakpointTagException {
    return BreakpointDefinition.createBuilder("ClassPathLookupTest.java", tag)
        .getLocation()
        .getLine();
  }

  private void verifyResolveSourceLocation(
      ClassPathLookup classPathLookup,
      String lookupSourceFile,
      String lookupLineNumberTag,
      String expectedClassSignature,
      String expectedMethodName,
      String expectedMethodDescriptor,
      String expectedLineNumberTag)
      throws BreakpointTagException {
    int lookupLineNumber = getTagLineNumber(lookupLineNumberTag);
    int expectedLineNumber = getTagLineNumber(expectedLineNumberTag);

    String msg =
        String.format(
            "classPathLookup.resolveSourceLocation(\"%s\", %d = tag %s)",
            lookupSourceFile, lookupLineNumber, lookupLineNumberTag);

    ResolvedSourceLocation location =
        classPathLookup.resolveSourceLocation(lookupSourceFile, lookupLineNumber);

    assertNotNull(msg, location);
    assertNull(msg, location.getErrorMessage());
    assertEquals(msg, "L" + expectedClassSignature + ";", location.getClassSignature());
    assertEquals(msg, expectedMethodName, location.getMethodName());
    assertEquals(msg, expectedMethodDescriptor, location.getMethodDescriptor());
    assertEquals(msg, expectedLineNumber, location.getAdjustedLineNumber());
  }
}
                                                    /* BPTAG: BEYOND_END */
