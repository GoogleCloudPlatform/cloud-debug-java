package com.google.devtools.cdbg.debuglets.java;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.fail;

import java.util.Arrays;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link YamlConfigParser}. */
@RunWith(JUnit4.class)
public class YamlConfigParserTest {

  private static YamlConfigParser parseString(String[] config) throws YamlConfigParserException {
    StringBuilder builder = new StringBuilder();
    for (String element : config) {
      builder.append(element);
      builder.append('\n');
    }
    return new YamlConfigParser(builder.toString());
  }

  private YamlConfigParser parseStringOK(String[] config) {
    try {
      return parseString(config);
    } catch (YamlConfigParserException e) {
      fail(e.toString());
    }
    return null;
  }

  private void parseStringError(String[] config) {
    try {
      parseString(config);
      fail("Config unexpectedly parsed OK");
    } catch (YamlConfigParserException e) {
      // Do nothing, as this exception is expected
    }
  }

  @Test
  public void emptyConfig() {
    String[] config = {};
    YamlConfigParser parser = parseStringOK(config);
    assertThat(parser.getBlocklistPatterns()).isEmpty();
    assertThat(parser.getBlocklistExceptionPatterns()).isEmpty();
  }

  /**
   * TODO(b/185048957): Finalize the conversion to blocklist by removing this test.
   */
  @Test
  public void oneBlacklist() {
    String[] config = {
      "blacklist:", "  - com.java.*",
    };
    YamlConfigParser parser = parseStringOK(config);
    assertThat(parser.getBlocklistExceptionPatterns()).isEmpty();
    assertThat(Arrays.asList(parser.getBlocklistPatterns())).containsExactly("com.java.*");
  }

  @Test
  public void oneBlocklist() {
    String[] config = {
      "blocklist:", "  - com.java.*",
    };
    YamlConfigParser parser = parseStringOK(config);
    assertThat(parser.getBlocklistExceptionPatterns()).isEmpty();
    assertThat(Arrays.asList(parser.getBlocklistPatterns())).containsExactly("com.java.*");
  }

  /**
   * TODO(b/185048957): Finalize the conversion to blocklist by removing this test.
   */
  @Test
  public void oneWhitelist() {
    String[] config = {
      "blacklist_exception:", "  - com.java.*",
    };
    YamlConfigParser parser = parseStringOK(config);
    assertThat(parser.getBlocklistPatterns()).isEmpty();
    assertThat(Arrays.asList(parser.getBlocklistExceptionPatterns())).containsExactly("com.java.*");
  }

  @Test
  public void oneAllowlist() {
    String[] config = {
      "blocklist_exception:", "  - com.java.*",
    };
    YamlConfigParser parser = parseStringOK(config);
    assertThat(parser.getBlocklistPatterns()).isEmpty();
    assertThat(Arrays.asList(parser.getBlocklistExceptionPatterns())).containsExactly("com.java.*");
  }

  /**
   * TODO(b/185048957): Finalize the conversion to blocklist by removing this test.
   */
  @Test
  public void duplicateBlacklist() {
    String[] config = {
      "blacklist:", "  - com.java.*", "  - com.java.*",
    };
    YamlConfigParser parser = parseStringOK(config);
    assertThat(parser.getBlocklistExceptionPatterns()).isEmpty();
    assertThat(Arrays.asList(parser.getBlocklistPatterns())).containsExactly("com.java.*");
  }

  @Test
  public void duplicateBlocklist() {
    String[] config = {
      "blocklist:", "  - com.java.*", "  - com.java.*",
    };
    YamlConfigParser parser = parseStringOK(config);
    assertThat(parser.getBlocklistExceptionPatterns()).isEmpty();
    assertThat(Arrays.asList(parser.getBlocklistPatterns())).containsExactly("com.java.*");
  }

  /**
   * TODO(b/185048957): Finalize the conversion to blocklist by removing this test.
   */
  @Test
  public void duplicateWhitelist() {
    String[] config = {
      "blacklist_exception:", "  - com.java.*", "  - com.java.*",
    };
    YamlConfigParser parser = parseStringOK(config);
    assertThat(parser.getBlocklistPatterns()).isEmpty();
    assertThat(Arrays.asList(parser.getBlocklistExceptionPatterns())).containsExactly("com.java.*");
  }

  @Test
  public void duplicateAllowlist() {
    String[] config = {
      "blocklist_exception:", "  - com.java.*", "  - com.java.*",
    };
    YamlConfigParser parser = parseStringOK(config);
    assertThat(parser.getBlocklistPatterns()).isEmpty();
    assertThat(Arrays.asList(parser.getBlocklistExceptionPatterns())).containsExactly("com.java.*");
  }

  @Test
  public void multipleEntries() {
    String[] config = {
      "blocklist:",
      "  - com.java.blocklist.*",
      "  - com.java.blocklist.util.*",
      "  - foo.bar.blocklist.Baz",
      "blocklist_exception:",
      "  - com.java.blocklist_exception.*",
      "  - com.java.blocklist_exception.util.*",
      "  - foo.bar.blocklist_exception.Baz",
    };
    YamlConfigParser parser = parseStringOK(config);

    assertThat(Arrays.asList(parser.getBlocklistPatterns()))
        .containsExactly(
            "com.java.blocklist.*", "com.java.blocklist.util.*", "foo.bar.blocklist.Baz");

    assertThat(Arrays.asList(parser.getBlocklistExceptionPatterns()))
        .containsExactly(
            "com.java.blocklist_exception.*",
            "com.java.blocklist_exception.util.*",
            "foo.bar.blocklist_exception.Baz");
  }
  /**
   * TODO(b/185048957): Finalize the conversion to blocklist by removing this test.
   */
  @Test
  public void multipleEntriesBothListKeyNamesUsed() {
    String[] config = {
      "blacklist:",
      "  - com.java.blacklist.*",
      "  - com.java.blacklist.util.*",
      "blocklist:",
      "  - com.java.blocklist.*",
      "  - com.java.blocklist.util.*",
      "blacklist_exception:",
      "  - com.java.blacklist_exception.*",
      "  - com.java.blacklist_exception.util.*",
      "blocklist_exception:",
      "  - com.java.blocklist_exception.*",
      "  - com.java.blocklist_exception.util.*",
    };
    YamlConfigParser parser = parseStringOK(config);

    assertThat(Arrays.asList(parser.getBlocklistPatterns()))
        .containsExactly(
            "com.java.blacklist.*",
            "com.java.blacklist.util.*",
            "com.java.blocklist.*",
            "com.java.blocklist.util.*");

    assertThat(Arrays.asList(parser.getBlocklistExceptionPatterns()))
        .containsExactly(
            "com.java.blacklist_exception.*",
            "com.java.blacklist_exception.util.*",
            "com.java.blocklist_exception.*",
            "com.java.blocklist_exception.util.*");
  }

  @Test
  public void hasBang() {
    String[] config = {
      "blocklist:", "  - \"!a\"",
    };
    YamlConfigParser parser = parseStringOK(config);
    assertThat(parser.getBlocklistPatterns()).hasLength(1);
  }

  @Test
  public void simpleList() {
    String[] config = {
      "- com.java.*",
    };
    parseStringError(config);
  }

  @Test
  public void misspelled() {
    String[] config = {
      "blucklist:", "  - com.java.*",
    };
    parseStringError(config);
  }

  @Test
  public void keyValue() {
    String[] config = {
      "blocklist: foo",
    };
    parseStringError(config);
  }

  @Test
  public void emptyColon() {
    String[] config = {
      ":",
    };
    parseStringError(config);
  }

  @Test
  public void yamlSyntaxError() {
    String[] config = {
      "blocklist:", "  - com.java.foo", "- com.java.bar.*",
    };
    parseStringError(config);
  }

  @Test
  public void nestedKey() {
    String[] config = {
      "blocklist:", "  nested: foo",
    };
    parseStringError(config);
  }

  @Test
  public void nestedKeyList() {
    String[] config = {
      "blocklist:", "  nested:", "    - com.java.*",
    };
    parseStringError(config);
  }

  @Test
  public void numberInList() {
    String[] config = {
      "blocklist:", "  - 7",
    };
    parseStringError(config);
  }

  @Test
  public void multipleEntriesWithExtraNesting() {
    String[] config = {
      "blocklist:",
      "  - com.java.blocklist.*",
      "  - com.java.blocklist.util.*",
      "    - foo.bar.blocklist.Baz",
      "blocklist_exception:",
      "  - com.java.blocklist_exception.*",
      "    - com.java.blocklist_exception.util.*",
      "    - foo.bar.blocklist_exception.Baz",
    };
    parseStringError(config);
  }

  @Test
  public void illegalSpaces() {
    String[] config = {
      "blocklist:", "  - \"Hello World\"",
    };
    parseStringError(config);
  }

  @Test
  public void illegalPunctuation() {
    String[] config = {
      "blocklist:", "  - com.java.myMethod()",
    };
    parseStringError(config);
  }

  @Test
  public void bangOnly() {
    String[] config = {
      "blocklist:", "  - \"!\"",
    };
    parseStringError(config);
  }

  @Test
  public void multipleBangs() {
    String[] config = {
      "blocklist:", "  - \"!a!\"",
    };
    parseStringError(config);
  }

  @Test
  public void bangNotAtStart() {
    String[] config = {
      "blocklist:", "  - \"a!a\"",
    };
    parseStringError(config);
  }

  @Test
  public void empty() {
    String[] config = {
      "blocklist:", "  - \"\"",
    };
    parseStringError(config);
  }

  @Test
  public void startsWithNumber() {
    String[] config = {
      "blocklist:", "  - 5a",
    };
    parseStringError(config);
  }

  @Test
  public void startsWithDot() {
    String[] config = {
      "blocklist:", "  - .a",
    };
    parseStringError(config);
  }
}
