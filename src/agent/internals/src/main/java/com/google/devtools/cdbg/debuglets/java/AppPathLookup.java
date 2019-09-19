package com.google.devtools.cdbg.debuglets.java;

import static com.google.devtools.cdbg.debuglets.java.AgentLogger.warnfmt;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.FileSystems;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.Set;

/**
 * Lookup class for finding files in application directories.
 *
 * <p>This class performs an additional search for resources in the case where the normal
 * ClassPathLookup has failed to find any. It looks in all of the directories containing jar files
 * in the classpath, and also in any directory which is a parent of a directory containing a jar
 * file, and which contains a source-context.json file.
 *
 * <p>For example, if the class path includes the element /a/b/c/foo.jar, and there exists a file
 * named /a/b/source-context.json, it would search /a/b for resources because /a/b is a parent
 * directory of /a/b/c which contains a source-context.json file.
 *
 * <p>This logic is separate from ClassPathLookup because we only want to search for source contexts
 * in these additional locations (we don't want to search for all classes in these locations).
 */
public class AppPathLookup {
  private static final String JAVA_CLASS_PATH = "java.class.path";
  private static final String JAR_EXTENSION = ".jar";
  private static final String SOURCE_CONTEXT_NAME = "source-context.json";

  /** The list of directories to search. */
  private final String[] appDirs;

  public AppPathLookup() {
    this(System.getProperty(JAVA_CLASS_PATH));
  }

  public AppPathLookup(String jvmClassPath) {
    Set<Path> jarDirs = new LinkedHashSet<>();
    ArrayList<String> allDirNames = new ArrayList<>();

    for (String pathElement : jvmClassPath.split(File.pathSeparator)) {
      if (!pathElement.endsWith(JAR_EXTENSION)) {
        // Not a jar file.
        continue;
      }

      Path jarDir = FileSystems.getDefault().getPath(pathElement).toAbsolutePath().getParent();
      if (jarDir == null) {
        // No containing directory, skip.
        continue;
      }

      if (jarDirs.contains(jarDir)) {
        // Already handled.
        continue;
      }

      // Add the containing directory
      jarDirs.add(jarDir);
      allDirNames.add(jarDir.toString());
    }

    // Add the source-context.json directories after the classpath directories.
    for (Path jarDir : jarDirs) {

      Path parentDir = jarDir.getParent();
      // Find the nearest parent directory containing a source-context.json, if any.
      while (parentDir != null && !new File(parentDir.toString(), SOURCE_CONTEXT_NAME).exists()) {
        parentDir = parentDir.getParent();
      }

      if (parentDir != null && !jarDirs.contains(parentDir)) {
        // There was a directory containing a source-context.json, and we haven't seen it directly
        // yet.
        allDirNames.add(parentDir.toString());
      }
    }

    appDirs = allDirNames.toArray(new String[0]);
  }

  /*
   * Searches for the application resource files that match {@code resourcePath}, reads them as
   * UTF-8 encoded string and returns the string. If no matches are found, returns an empty array.
   *
   * <p>Application resource files are retrieved the directories containing .jar files or their
   * nearest parent directory containing a source-context.json file. The source-context.json
   * directory is searched because gcloud app deploy will place source context files in the
   * deployment root directory.
   *
   * <p>A resource with the same name may appear in multiple directories. While it is not too
   * interesting for .class resource files, this is an important scenario for source context
   * file that may show up multiple .jar files.
   *
   * @param resourcePath The name of the resource (usually a file name) to load.
   * @return An array containing the contents of all resources of the given name, as UTF-8 strings.
   */
  public String[] readApplicationResource(String resourcePath) {
    ArrayList<String> contents = new ArrayList<>();
    for (String dir : appDirs) {
      File file = new File(dir, resourcePath);
      try {
        if (file.exists()) {
          contents.add(new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8));
        }
      } catch (IOException e) {
        // Ignore this error and continue. It's better to return partial results, than to
        // fail the entire operation just because of some bad .jar file.
        warnfmt(e, "Failed to load application resource %s", file.getPath());
      }
    }

    return contents.toArray(new String[0]);
  }
}
