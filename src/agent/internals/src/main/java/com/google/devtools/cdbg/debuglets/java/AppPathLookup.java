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
 * Lookup class for finding file in application directories.
 *
 * <p>This class performs an additional search for resources in the case where the normal
 * ClassPathLookup has failed to find any. It looks in all of the directories containing jar files;
 * and also in any directory which is a parent of a directory containing a jar file, and which
 * contains an app.yaml file.
 *
 * <p>For example, if the class path includes the element /a/b/c/foo.jar, and there exists a file
 * named /a/b/app.yaml, it would search /a/b for source context because /a/b is a parent directory
 * of /a/b/c which contains an app.yaml file.
 *
 * <p>This logic is separate from ClassPathLookup because we only want to search for source contexts
 * in these additional locations (we don't want to search for all classes in these locations).
 */
public class AppPathLookup {
  private static final String JAVA_CLASS_PATH = "java.class.path";
  private static final String JAR_EXTENSION = ".jar";
  private static final String APP_YAML_NAME = "app.yaml";

  /** The list of directories to search. */
  private final String[] appDirs;

  public AppPathLookup() {
    Set<Path> seenDirs = new LinkedHashSet<>();
    ArrayList<String> allDirs = new ArrayList<>();
    ArrayList<String> appYamlDirs = new ArrayList<>();

    String jvmClassPath = System.getProperty(JAVA_CLASS_PATH);
    for (String pathElement : jvmClassPath.split(File.pathSeparator)) {
      if (!pathElement.endsWith(JAR_EXTENSION)) {
        // Not a jar file.
        continue;
      }

      Path dirPath = FileSystems.getDefault().getPath(pathElement).getParent();
      if (dirPath == null) {
        // No parent directory, skip.
        continue;
      }

      if (seenDirs.contains(dirPath)) {
        // Already handled.
        continue;
      }

      seenDirs.add(dirPath);
      allDirs.add(dirPath.toString());

      // Find the parent app.yaml directory, if any.
      while (dirPath != null && !new File(dirPath.toString(), APP_YAML_NAME).exists()) {
        dirPath = dirPath.getParent();
      }

      if (dirPath != null && !seenDirs.contains(dirPath)) {
        seenDirs.add(dirPath);
        appYamlDirs.add(dirPath.toString());
      }
    }

    // Put the app.yaml directories after the jar directories, so that files in the jar directory
    // will be returned before files in the app yaml directories. Source context files get created
    // in the app.yaml directory during deployment. A file next to a jar file was likely generated
    // earlier in the build, and is therefore likely to be more accurate.
    allDirs.addAll(appYamlDirs);

    appDirs = allDirs.toArray(new String[0]);
  }

  /*
   * Searches for the application resource files that match {@code resourcePath}, reads them as
   * UTF-8 encoded string and returns the string. If no matches are found, returns an empty array.
   *
   * <p>Application resource files are retrieved the directories containing .jar files or their
   * nearest parent directory containing an app.yaml file. The app.yaml directory is searched
   * because gcloud app deploy will place source context files in the app.yaml directory
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
