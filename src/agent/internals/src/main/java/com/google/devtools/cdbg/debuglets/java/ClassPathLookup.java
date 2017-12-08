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
import static java.nio.charset.StandardCharsets.UTF_8;

import com.google.devtools.cdbg.debuglets.java.ResourceIndexer.FileSystemResourcesSource;
import com.google.devtools.cdbg.debuglets.java.ResourceIndexer.ResourcesSource;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.Properties;
import java.util.Set;
import java.util.SortedSet;
import java.util.TreeSet;

/**
 * Queries information about available packages and resources in the Java application.
 *
 * <p>The JVMTI and JNI interfaces that feed the native part of the debugger agent only tell us
 * about loaded classes. At the same time the end user can set a breakpoint in a code that will only
 * be loaded in the future. From JVMTI and JNI perspective this code just doesn't exist, so the
 * debugger can't tell whether the breakpoint is valid. This class enumerates the resources that an
 * application *could* load directly.
 */
final class ClassPathLookup {
  private static final String JAVA_CLASS_PATH = "java.class.path";
  private static final String JAVA_EXTENSION = ".java";
  private static final String SCALA_EXTENSION = ".scala";

  /**
   * Enables indexing of classes specified in Java class path.
   */
  private final boolean useDefaultClassPath;

  /**
   * File system locations to search for .class and .jar files beyond what's normally specified in
   * Java class path. This is needed for debugger to find classes that are loaded through custom
   * class loaders (such as the case with application classes in Jetty).
   */
  private final String[] extraClassPath;

  /**
   * Indexes resources (.class files and other files) that the application may load.
   */
  private ResourceIndexer resourceIndexer;

  /**
   * Indexes classes available to the application. Each instance of {@link ClassResourcesIndexer}
   * corresponds to a different source (e.g. different .jar file).
   */
  private Collection<ClassResourcesIndexer> classResourcesIndexers;
  
  // TODO(vlif): refactor to get rid of this
  static ClassPathLookup defaultInstance;
  
  /**
   * Class constructor.
   *
   * @param useDefaultClassPath when true classes referenced by default Java class path will be
   *        indexed
   * @param extraClassPath optional file system locations to search for .class and .jar files beyond
   *        what's normally specified in Java class path
   */
  public ClassPathLookup(boolean useDefaultClassPath, String[] extraClassPath) {
    infofmt("Initializing ClassPathLookup, default classpath: %b, extra classpath: %s",
        useDefaultClassPath,
        (extraClassPath == null) ? "<null>" : Arrays.asList(extraClassPath));

    // Try to guess where application classes might be besides class path. We only do it if the
    // debugger uses default configuration. If the user sets additional directories, we use that and
    // not try to guess anything.
    if (useDefaultClassPath && ((extraClassPath == null) || (extraClassPath.length == 0))) {
      extraClassPath = findExtraClassPath();
    }

    this.useDefaultClassPath = useDefaultClassPath;
    this.extraClassPath = extraClassPath;

    indexApplicationResources();

    // TODO(vlif): GcpHubClient needs ClassPathLookup to compute uniquifier. This needs to
    // be refactored. Either GcpHubClient should not require ClassPathLookup or ClassPathLookup
    // should become static.
    defaultInstance = this;
  }

  /**
   * Searches for a statement in a method corresponding to the specified source line.
   *
   * <p>We assume that the resource path (like "com/myprod/MyClass") corresponds to the source Java
   * file. The source file might be located in one of the inner or static classes.
   *
   * <p>The returned instance of {@code ResolvedSourceLocation} may have a different line number.
   * This will happen when {@code lineNumber} doesn't point to any statement within the function. In
   * such cases the line corresponding to the closest prior statement is returned. There is no way
   * right now to distinguish between empty line inside a function and multi-line statement because
   * JVM only provides start location for each statement.
   *
   * <p>The function makes no assumption about which classes have already been loaded and which
   * haven't. This code has zero impact on the running application. Specifically no new application
   * classes are being loaded.
   *
   * @param sourcePath full path to the source .java file used to compile the code
   * @param lineNumber source code line number (first line has a value of 1)
   *
   * @return new instance of {@code ResolvedSourceLocation} with a {class, method, adjusted line
   *         number} tuple if the resolution was successful or an error message otherwise.
   */
  public ResolvedSourceLocation resolveSourceLocation(String sourcePath, int lineNumber) {
    if ((sourcePath == null) || sourcePath.isEmpty()) {
      return new ResolvedSourceLocation(
          new FormatMessage(Messages.UNDEFINED_BREAKPOINT_LOCATION));
    }

    if (lineNumber < 1) {
      return new ResolvedSourceLocation(
          new FormatMessage(Messages.INVALID_LINE_NUMBER, Integer.toString(lineNumber)));
    }

    if (!sourcePath.endsWith(JAVA_EXTENSION) && !sourcePath.endsWith(SCALA_EXTENSION)) {
      return new ResolvedSourceLocation(
          new FormatMessage(Messages.UNSUPPORTED_SOURCE_FILE_EXTENSION));
    }

    // Retrieve the class resources matching the source file. There might be several of those
    // if the source file contains inner or static classes or multiple outer classes.
    Collection<InputStream> resources = new ArrayList<>();
    for (ClassResourcesIndexer indexer : classResourcesIndexers) {
      Collection<String> resourcePaths = indexer.mapSourceFile(sourcePath);
      if (resourcePaths != null) {
        for (String resourcePath : resourcePaths) {
          try {
            resources.add(indexer.getSource().getResource(resourcePath));
          } catch (IOException e) {
            warnfmt(e, "Failed to open application resource %s", resourcePath);
          }
        }
      }
    }

    if (resources.isEmpty()) {
      return new ResolvedSourceLocation(
          new FormatMessage(Messages.SOURCE_FILE_NOT_FOUND_IN_EXECUTABLE));
    }

    SourceFileMapper mapper = new SourceFileMapper(resources);
    return mapper.map(lineNumber);
  }

  /**
   * Gets the list of class signatures for the specified class name.
   *
   * <p>@see ClassResourcesIndexer#findClassesByName(String) for more details.
   *
   * @param classTypeName class name without the package (inner classes are separated with '.')
   * 
   * @return list of matches or null or empty array if not found.
   */
  public String[] findClassesByName(String classTypeName) {
    for (ClassResourcesIndexer indexer : classResourcesIndexers) {
      String[] rc = indexer.findClassesByName(classTypeName);
      if ((rc != null) && (rc.length > 0)) {
        return rc;
      }
    }
    
    return null;
  }

  /**
   * Computes a hash code of all the binaries in the class path. This hash code is used to
   * distinguish different builds of the same project.
   *
   * <p>The caller can't handle any errors, so this function silently swallows all problems,
   * returning the best result it can get.
   *
   * @param initializationVector initialization vector for the hash computation to further randomize
   *        different deployments that have exactly the same binary. For example AppEngine debuglet
   *        sets this parameter to concatenated strings of project ID, module, major version and
   *        minor version.
   */
  public String computeDebuggeeUniquifier(String initializationVector)
      throws NoSuchAlgorithmException {
    SortedSet<String> applicationFiles = new TreeSet<>();
    for (FileSystemResourcesSource source : resourceIndexer.getFileSystemSources()) {
      for (ResourcesDatabase.Directory directory : source.getResourcesDatabase().directories()) {
        for (String resourcePath : directory.getFilePaths()) {
          applicationFiles.add(source.getResourceFile(resourcePath).getAbsolutePath());
        }
      }
    }
    
    UniquifierComputer computer = new UniquifierComputer(initializationVector, applicationFiles);
    return computer.getUniquifier();
  }

  /**
   * Searches for the application resource files that match {@code resourcePath}, reads them as
   * UTF-8 encoded string and returns the string. If no matches are found, returns an empty array.
   *
   * <p>Application resource files are retrieved from .jar files or from directories referenced in
   * JVM class path. {@code resourcePath} without any '/' characters corresponds to a resource in a
   * top level package.
   *
   * <p>A resource with the same name may appear in multiple directories referenced in class path or
   * in multiple .jar files. While it is not too interesting for .class resource files, this is an
   * important scenario for source context file that may show up in every .jar file.
   */
  public String[] readApplicationResource(String resourcePath) {
    ArrayList<String> resourcesContent = new ArrayList<>();
    for (ResourcesSource source : resourceIndexer.getSources()) {
      if (source.getResourcesDatabase().containsFile(resourcePath)) {
        try (InputStream stream = source.getResource(resourcePath)) {
          ByteArrayOutputStream data = new ByteArrayOutputStream();

          byte[] buffer = new byte[1024];
          int bytesRead;
          do {
            bytesRead = stream.read(buffer);
            if (bytesRead > 0) {
              data.write(buffer, 0, bytesRead);
            }
          } while (bytesRead != -1);

          resourcesContent.add(data.toString(UTF_8.name()));
        } catch (IOException e) {
          // Ignore this error and continue. It's better to return partial results, than to
          // fail the entire operation just because of some bad .jar file.
          warnfmt(e, "Failed to load application resource %s", resourcePath);
        }
      }
    }

    return resourcesContent.toArray(new String[0]);
  }

  /**
   * Searches for the application resource files that match {@code resourcePath}, reads them and
   * converts to git source context if specific URL and COMMIT lines are found. The git.properties
   * file is generated by https://github.com/ktoso/maven-git-commit-id-plugin which allows some
   * flexibility in configuring lines prefixes, but we currently support only default 'git.'
   * prefix (otherwise we cannot use java.util.Properties class ans will have to read the file
   * line-by-line looking for known patterns and assuming knowledge of the line format to extract
   * value for specific key).
   */
  public String[] readGitPropertiesResourceAsSourceContext(String resourcePath) {
    ArrayList<String> resourcesContent = new ArrayList<>();
    for (ResourcesSource source : resourceIndexer.getSources()) {
      if (source.getResourcesDatabase().containsFile(resourcePath)) {
        try (InputStream stream = source.getResource(resourcePath)) {
          if (stream != null) {
            Properties gitProperties = new Properties();
            gitProperties.load(stream);

            String url = gitProperties.getProperty("git.remote.origin.url");
            String commit = gitProperties.getProperty("git.commit.id");

            // If we have both url and commit values - generate git source context
            if (url != null && commit != null) {
              String gitSourceContext =
                  String.format(
                      "{\n \"git\": {\n  \"revisionId\": \"%s\",\n  \"url\": \"%s\"\n }\n}",
                      commit, url);

              infofmt("found Git properties and generated source context:\n%s", gitSourceContext);
              resourcesContent.add(gitSourceContext);
            }
          }
        } catch (IOException e) {
          // Ignore this error and continue. It's better to return partial results, than to
          // fail the entire operation just because of some bad .jar file.
          warnfmt(e, "Failed to load application resource %s", resourcePath);
        }
      }
    }

    return resourcesContent.toArray(new String[0]);
  }

  /**
   * Loads class file of the specified class.
   * 
   * <p>It does not include any inner classes. They need to be loaded separately.
   * 
   * <p>The returned class file might be slightly different from one actually loaded into the JVM.
   * This can happen if a Java agent transformed the class.
   * 
   * <p>This function does not check that the class was verified by JVM. The caller should not
   * assume that the returned BLOB represents a well behaved Java class.
   *
   * @param cls class object originated from the class file to load.
   * @return class file BLOB.
   * @throws ClassNotFoundException thrown if the class doesn't have an associated class file. This
   *    will happen for dynamically generated classes (and JDK has a few of those). 
   * @throws IOException thrown if the class file could not be read.
   */
  public static byte[] readClassFile(Class<?> cls) throws ClassNotFoundException, IOException {
    try (InputStream resourceStream = getResource(cls)) {
      byte[] classFile = null;
      byte[] buffer = new byte[65536];  // Big enough buffer to fit most class files. 
      int bytesRead;
      while ((bytesRead = resourceStream.read(buffer)) != -1) {
        if (classFile == null) {
          classFile = new byte[bytesRead];
          System.arraycopy(buffer, 0, classFile, 0, bytesRead);
        } else {
          byte[] expandedClassFile = Arrays.copyOf(classFile, classFile.length + bytesRead);
          System.arraycopy(buffer, 0, expandedClassFile, classFile.length, bytesRead);
          classFile = expandedClassFile;
        }
      }
      
      if (classFile == null) {
        throw new IOException();
      }
      
      return classFile;
    }
  }
  
  /**
   * Opens the .class file of the specified class.
   * 
   * @param cls Java class to load.
   * @throws ClassNotFoundException if the class resource could not be found (for example if
   *     if the class was dynamically generated).
   */
  private static InputStream getResource(Class<?> cls) throws ClassNotFoundException {
    String classBinaryName = cls.getName();
    String resourceName = classBinaryName.replace('.', '/') + ".class";

    InputStream resourceStream;
    
    ClassLoader classLoader = cls.getClassLoader();
    if (classLoader == null) {
      resourceStream = ClassLoader.getSystemResourceAsStream(resourceName);
    } else {
      resourceStream = classLoader.getResourceAsStream(resourceName);
    }

    if (resourceStream == null) {
      throw new ClassNotFoundException(classBinaryName);
    }
    
    return resourceStream;
  }

  /**
   * Tries to figure out additional directories where application class might be.
   *
   * Usually all application classes are located within the directories and .JAR files specified
   * in a class path. There are some exceptions to it though. An application can use a custom
   * class loader and load application classes from virtually anywhere. It is impossible to infer
   * all the locations of potential application classes.
   *
   * Web servers (e.g. jetty, tomcat) are a special case. They always load application classes from
   * a directory that's not on the class path. Since the scenario of debugging an application that
   * runs in a web server is important, we want to try to guess where the application classes might
   * be. The alternative is to have the user always specify the location, but it complicates the
   * debugger deployment.
   *
   * In general case, it's hard to determine the location of the web application. We would need to
   * read configuration files of the web server, and each web server has a different one. We
   * therefore only support the simplest, but the most common case, when the web application lives
   * in the default ROOT directory.
   *
   * This function is marked as public for unit tests.
   */
  public static String[] findExtraClassPath() {
    Set<String> paths = new HashSet<>();

    // Tomcat.
    addSystemPropertyRelative(paths, "catalina.base", "webapps/ROOT/WEB-INF/lib");
    addSystemPropertyRelative(paths, "catalina.base", "webapps/ROOT/WEB-INF/classes");

    // Jetty: newer Jetty versions renamed ROOT to root.
    addSystemPropertyRelative(paths, "jetty.base", "webapps/ROOT/WEB-INF/lib");
    addSystemPropertyRelative(paths, "jetty.base", "webapps/ROOT/WEB-INF/classes");
    addSystemPropertyRelative(paths, "jetty.base", "webapps/root/WEB-INF/lib");
    addSystemPropertyRelative(paths, "jetty.base", "webapps/root/WEB-INF/classes");

    return paths.toArray(new String[0]);
  }

  /**
   * Appends a path relative to the base path defined in a system property.
   *
   * No effect if the system property is not defined or the combined path does not exist.
   */
  private static void addSystemPropertyRelative(Set<String> paths, String name, String suffix) {
    String value = System.getProperty(name);
    if ((value == null) || value.isEmpty()) {
      return;
    }

    File path = new File(value, suffix);
    if (!path.exists()) {
      return;
    }

    paths.add(path.toString());
  }

  /**
   * Finds paths of all .class and .jar files listed through JVM class path and
   * {@code extraClassPath}.
   */
  private void indexApplicationResources() {
    // Merge JVM class path and extra class path. Preserve order of elements in the list,
    // but remove any potential duplicates.
    Set<String> effectiveClassPath = new LinkedHashSet<>();

    if (useDefaultClassPath) {
      String jvmClassPath = System.getProperty(JAVA_CLASS_PATH);
      effectiveClassPath.addAll(Arrays.asList(jvmClassPath.split(File.pathSeparator)));
    }

    if (extraClassPath != null) {
      effectiveClassPath.addAll(Arrays.asList(extraClassPath));
    }

    resourceIndexer = new ResourceIndexer(effectiveClassPath);
    classResourcesIndexers = new ArrayList<>();
    for (ResourcesSource source : resourceIndexer.getSources()) {
      classResourcesIndexers.add(new ClassResourcesIndexer(source));
    }
  }
}
