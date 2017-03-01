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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_COMMON_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_COMMON_H_


#include <string.h>
#include <stdint.h>
#include <memory>
#include "jni.h"  // NOLINT
#include "jvmti.h"  // NOLINT
#include "classfile_constants.h"  // NOLINT
#include "glog/logging.h"

#define DISALLOW_COPY_AND_ASSIGN(TypeName)  \
    TypeName(const TypeName&) = delete;  \
    void operator=(const TypeName&) = delete

template <typename T, size_t N>
char (&ArraySizeHelper(const T (&array)[N]))[N];

#define arraysize(array) (sizeof(ArraySizeHelper(array)))

typedef signed char         int8;
typedef short               int16;
typedef int                 int32;
typedef long long           int64;
typedef unsigned char       uint8;
typedef unsigned short      uint16;
typedef unsigned int        uint32;
typedef unsigned long long  uint64;

using std::string;

using google::LogSink;  // NOLINT
using google::LogSeverity;  // NOLINT
using google::AddLogSink;  // NOLINT
using google::RemoveLogSink;  // NOLINT

// MOE: begin_strip
// The open source build uses gflags, which uses the traditional (v1) flags APIs
// to define/declare/access command line flags. The internal build has upgraded
// to use v2 flags API (DEFINE_FLAG/DECLARE_FLAG/GetFlag/SetFlag), which is not
// supported by gflags yet (and absl is not released to open source yet).
// Here, we use simple, dummy v2 flags wrappers around v1 flags implementation.
// This allows us to use the same flags APIs both internally and externally.
// MOE: end_strip

#define DEFINE_FLAG(type, name, default_value, help) \
    DEFINE_##type(name, default_value, help)

#define DECLARE_FLAG(type, name) \
    DECLARE_##type(name)

namespace base {
  // Return the value of an old-style flag.  Not thread-safe.
  inline bool GetFlag(bool flag) { return flag; }
  inline int32 GetFlag(int32 flag) { return flag; }
  inline int64 GetFlag(int64 flag) { return flag; }
  inline uint64 GetFlag(uint64 flag) { return flag; }
  inline double GetFlag(double flag) { return flag; }
  inline string GetFlag(const string& flag) { return flag; }

  // Change the value of an old-style flag.  Not thread-safe.
  inline void SetFlag(bool* f, bool v) { *f = v; }
  inline void SetFlag(int32* f, int32 v) { *f = v; }
  inline void SetFlag(int64* f, int64 v) { *f = v; }
  inline void SetFlag(uint64* f, uint64 v) { *f = v; }
  inline void SetFlag(double* f, double v) { *f = v; }
  inline void SetFlag(string* f, const string& v) { *f = v; }
}  // namespace base


#define STRINGIZE(s)    #s
#define STRINGIFY(s)    STRINGIZE(s)

#include "jvm_env.h"

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_COMMON_H_
