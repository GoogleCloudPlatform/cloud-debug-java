# Copyright 2015 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""Generates C++ static definition with content of binary files.

This script is used to embed content of binary files (such as Java .class
files) inside the C++ binary. It generates C++ code that statically
initializes byte array with the content of the specified binary files.

This script supports adding one or more files from either a directory or
a .zip archive. Files are filtered out through regular expression associated
with each such entry.

In addition to byte array with the content of each file, this script builds
a directory of imported files.

For example running:
    python static_def_gen.py \
        out.inl \
        /tmp/file.jar:^(a/b/c|home)/.*\\.(class|bin)$

Will produce something like:
    static constexpr uint8 kMyFile_bin[] = { 68, 69, 70, 88, 89, 90, };
    static constexpr char kMyFile_bin_path[] = "a/b/c/MyFile.bin";
    static constexpr uint8 kOne_class[] = { 71, 73, 72, };
    static constexpr char kOne_class_path[] = "a/b/c/One.class";
    static constexpr uint8 kTwo_class[] = { 65, 66, 67, };
    static constexpr char kTwo_class_path[] = "home/Two.class";

    static constexpr struct {
      const char* path;
      const char* extension;
      const uint8* data;
      int data_size;
    } kBinaryFiles[] = {
      {
        kMyFile_bin_path,
        "bin",
        kMyFile_bin,
        arraysize(kMyFile_bin)
      },
      {
        kOne_class_path,
        "class",
        kOne_class,
        arraysize(kOne_class)
      },
      {
        kTwo_class_path,
        "class",
        kTwo_class,
        arraysize(kTwo_class)
      },
    };

If /tmp/file.jar had a/b/c/file.txt or foreign/Main.class, these files would
be ignored.

Syntax:
  static_def_gen.py <cc file> <entry>:<pattern> <entry>:<pattern> ...
  where each entry is either a directory with .class files or a .jar file
  and pattern is a regular expression selecting the files
"""

from __future__ import print_function
import os
import re
import sys
import zipfile


def EnumerateInputs(inputs):
  """Emumerates binary files in the provided paths."""
  for current_input in inputs:
    components = current_input.split(':', 2)
    entry = components[0]
    pattern = re.compile(components[1])
    if os.path.isdir(entry):
      for root, unused_subdirs, files in os.walk(entry):
        for file_name in files:
          full_path = os.path.join(root, file_name)
          relative_path = os.path.relpath(full_path, entry)
          if pattern.match(relative_path):
            yield {
                'path': relative_path,
                'open': lambda full_path=full_path: open(full_path, 'rb')
            }
    else:
      jar = zipfile.ZipFile(entry, 'r')
      for entry in jar.namelist():
        if pattern.match(entry):
          yield {
              'path': entry,
              'open': lambda jar=jar, entry=entry: jar.open(entry)
          }


def _GetName(item):
  """Returns name of the C++ variable to use for the binary file data."""
  return os.path.basename(item['path']).replace('.', '_').replace('$', '_')


def Generate(cc_file_path, inputs):
  """Main function of code generator."""

  cc_file = open(cc_file_path, 'w+')

  binary_files = sorted(EnumerateInputs(inputs),
                        key=lambda binary_file: binary_file['path'])
  for binary_file in binary_files:
    path = binary_file['path']
    name = _GetName(binary_file)
    cc_file.write('static constexpr uint8 k%s[] = { ' % name)
    with binary_file['open']() as f:
      byte = f.read(1)
      while byte:
        # Convert the byte read to a 8-bit unsigned integer.
        cc_file.write('%s, ' % str(ord(byte)))
        byte = f.read(1)
    cc_file.write('};\n')

    cc_file.write('static constexpr char k%s_path[] = "%s";\n' % (name, path))

  cc_file.write('\n')

  cc_file.write('static constexpr struct {\n')
  cc_file.write('  const char* path;\n')
  cc_file.write('  const char* extension;\n')
  cc_file.write('  const uint8* data;\n')
  cc_file.write('  int data_size;\n')
  cc_file.write('} kBinaryFiles[] = {\n')
  for binary_file in binary_files:
    path = binary_file['path']
    name = _GetName(binary_file)
    ext = re.sub('^\\.', '', os.path.splitext(path)[1])
    cc_file.write('  {\n')
    cc_file.write('    k%s_path,\n' % name)
    cc_file.write('    "%s",\n' % ext)
    cc_file.write('    k%s,\n' % name)
    cc_file.write('    arraysize(k%s)\n' % name)
    cc_file.write('  },\n')
  cc_file.write('};\n')

  cc_file.close()


def main():
  if len(sys.argv) < 3 or sys.argv[1] in ['-?', '--?', '-h', '--help']:
    print(__doc__)
    sys.exit(-1)

  Generate(sys.argv[1], sys.argv[2:])

if __name__ == '__main__':
  main()
