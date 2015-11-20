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


#define STRINGIZE(s)    #s
#define STRINGIFY(s)    STRINGIZE(s)

#include "jvm_env.h"

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_COMMON_H_
