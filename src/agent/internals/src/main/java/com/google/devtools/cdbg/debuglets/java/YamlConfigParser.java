/**
 * Copyright 2017 Google Inc. All Rights Reserved.
 *
 * <p>Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 * <p>http://www.apache.org/licenses/LICENSE-2.0
 *
 * <p>Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.google.devtools.cdbg.debuglets.java;

import static com.google.devtools.cdbg.debuglets.java.AgentLogger.infofmt;
import static com.google.devtools.cdbg.debuglets.java.AgentLogger.warnfmt;
import static java.nio.charset.StandardCharsets.UTF_8;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import org.yaml.snakeyaml.LoaderOptions;
import org.yaml.snakeyaml.Yaml;
import org.yaml.snakeyaml.constructor.SafeConstructor;
import org.yaml.snakeyaml.error.YAMLException;

/** Finds, loads and parses debugger configuration file. */
public class YamlConfigParser {
  /** A set of blocklist patterns found in the parsed config. */
  private String[] blocklistPatterns;

  /** A set of blocklist pattern exceptions. */
  private String[] blocklistExceptionPatterns;

  /**
   * Parses the given InputStream as YAML with an expected structure.
   *
   * <p>Throws YamlConfigParserException if the stream can not be parsed or if it has an unexpected
   * structure.
   *
   * <p>An example of legal structure would be:
   *
   * <p>blocklist: - com.secure.* - com.foo.MyClass blocklist_exception: - com.secure.testing.*
   *
   * <p>An empty config is also legal.
   */
  public YamlConfigParser(String yamlConfig) throws YamlConfigParserException {
    blocklistPatterns = new String[0];
    blocklistExceptionPatterns = new String[0];

    try (InputStream inputStream = new ByteArrayInputStream(yamlConfig.getBytes(UTF_8))) {
      parseYaml(inputStream);
    } catch (YamlConfigParserException e) {
      warnfmt("%s", e.toString());
      throw e;
    } catch (IOException e) {
      // IOException is not expected on a string reader, but the API contract
      // requires we catch it anyway.
      warnfmt("%s", e.toString());
      throw new YamlConfigParserException("IOException: " + e);
    }

    infofmt(
        "Config Load OK. Added %d blocklist and %d blocklist exception patterns",
        blocklistPatterns.length, blocklistExceptionPatterns.length);
  }

  /**
   * Returns blocklists found in config.
   */
  public String[] getBlocklistPatterns() {
    return blocklistPatterns;
  }

  /**
   * Returns blocklist exceptions found in config.
   */
  public String[] getBlocklistExceptionPatterns() {
    return blocklistExceptionPatterns;
  }

  /**
   * Checks a pattern for legality.
   *
   * <p>For example, a pattern can not contain whitespace or logical characters, such as "+".
   */
  private boolean isLegalPattern(String pattern) {
    int bangCount = 0;
    for (char c : pattern.toCharArray()) {
      if (c != '!' && c != '*' && c != '.' && !Character.isJavaIdentifierPart(c)) {
        // Not a legal character
        return false;
      }
      if (c == '!') {
        // Track the number of ! characters for later assertions
        ++bangCount;
      }
    }

    if (pattern.length() < 1) {
      // Pattern must have at least one character
      return false;
    }

    char firstCharacter = pattern.charAt(0);

    if (firstCharacter == '.') {
      // . can not be the first character
      return false;
    }

    if (Character.isDigit(firstCharacter)) {
      // Can not start with a number
      return false;
    }

    if (bangCount > 1) {
      // multiple ! characters are not allowed
      return false;
    }

    if (bangCount == 1 && firstCharacter != '!') {
      // ! is only allowed at the start of the pattern
      return false;
    }

    if (bangCount == 1 && pattern.length() == 1) {
      // ! is not allowed alone
      return false;
    }

    return true;
  }

  /**
   * Adds a list of patterns to the given set.
   *
   * <p>Throws YamlConfigParserException if the pattern list contains non-strings or illegal pattern
   * characters.
   */
  private void addPatternsToSet(Set<String> set, List<Object> patterns)
      throws YamlConfigParserException {
    for (Object listElement : patterns) {
      if (!(listElement instanceof String)) {
        throw new YamlConfigParserException("Unexpected non-string content: " + listElement);
      }

      String pattern = (String) listElement;
      if (!isLegalPattern(pattern)) {
        throw new YamlConfigParserException("Illegal pattern specified: " + pattern);
      }

      set.add(pattern);
    }
  }

  /**
   * Parses the given InputStream as YAML with an expected structure.
   *
   * <p>Throws YamlConfigParserException if the stream can not be parsed or if it has an unexpected
   * structure.
   */
  private void parseYaml(InputStream yamlConfig) throws YamlConfigParserException {
    Yaml yaml = new Yaml(new SafeConstructor(new LoaderOptions()));
    Set<String> blocklistPatternSet = new HashSet<>();
    Set<String> blocklistExceptionPatternSet = new HashSet<>();

    try {
      // We always expect a Map<String, List<Object>>. Invalid cast is handled in the catch clause.
      @SuppressWarnings("unchecked")
      Map<String, List<Object>> data = (Map<String, List<Object>>) yaml.load(yamlConfig);

      if (data == null) {
        // Nothing was loaded
        return;
      }

      for (Map.Entry<String, List<Object>> entry : data.entrySet()) {
        List<Object> value = entry.getValue();

        switch (entry.getKey()) {
          /**
           * TODO: Finalize the conversion to blocklist
           */
          case "blacklist":
          case "blocklist":
            addPatternsToSet(blocklistPatternSet, value);
            break;
          /**
           * TODO: Finalize the conversion to blocklist
           */
          case "blacklist_exception":
          case "blocklist_exception":
            addPatternsToSet(blocklistExceptionPatternSet, value);
            break;
          default:
            throw new YamlConfigParserException("Unrecognized key in config: " + entry.getKey());
        }
      }
    } catch (YAMLException | ClassCastException e) {
      // Yaml failed to parse
      throw new YamlConfigParserException(e.toString());
    }

    blocklistPatterns = blocklistPatternSet.toArray(new String[0]);
    blocklistExceptionPatterns = blocklistExceptionPatternSet.toArray(new String[0]);
  }
}
