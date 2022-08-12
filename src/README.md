# Running the tests

## Install Bazel

Running the tests requires [Bazel](https://bazel.build/), see
[here](https://docs.bazel.build/versions/main/install.html) for installation
instructions.

## Sample Commands

**From any directory, run all tests found in the workspace:**
```
bazel test //src/...
```

**Run all tests in current directory**
```
# E.g. in directory `...test/java/com/google/devtools/cdbg/debuglets/java`
bazel test :all
```

**Run individual test in current directory**:
```
# E.g. in directory `...test/java/com/google/devtools/cdbg/debuglets/java`
bazel test :YamlConfigParserTest
```
