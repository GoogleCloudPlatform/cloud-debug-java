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

#include "jvm_eval_call_stack.h"

#include <algorithm>
#include <cstdint>

#include "common.h"
#include "jni_utils.h"
#include "jvmti_buffer.h"

ABSL_FLAG(int32, cdbg_max_stack_depth, 20,
          "Maximum number of stack frames to unwind");

namespace devtools {
namespace cdbg {

void JvmEvalCallStack::Read(jthread thread, std::vector<JvmFrame>* result) {
  jvmtiError err = JVMTI_ERROR_NONE;

  result->clear();

  // Block JvmtiOnCompiledMethodUnload as long as this function is executing.
  // This is to make sure Java methods don't get unloaded while this function
  // is executing.
  absl::MutexLock jmethods_reader_lock(&jmethods_mu_);

  // Load call stack through JVMTI.
  jint frames_count = 0;
  std::unique_ptr<jvmtiFrameInfo[]> frames(
      new jvmtiFrameInfo[absl::GetFlag(FLAGS_cdbg_max_stack_depth)]);
  err = jvmti()->GetStackTrace(thread, 0,
                               absl::GetFlag(FLAGS_cdbg_max_stack_depth),
                               frames.get(), &frames_count);
  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "Failed to get stack trace, error: " << err;
    return;
  }

  // Evaluate all the call frames.
  for (int i = 0; i < frames_count; ++i) {
    result->push_back({ frames[i], DecodeFrame(frames[i]) });
  }
}


const JvmEvalCallStack::FrameInfo& JvmEvalCallStack::ResolveCallFrameKey(
    int key) const {
  absl::MutexLock data_reader_lock(&data_mu_);

  DCHECK((key >= 0) && (key < frames_.size()));

  // The FrameInfo is immutable and never gets deleted (as long as this
  // instance is alive). Therefore it is safe to return pointer to it.
  return *frames_[key];
}


int JvmEvalCallStack::InjectFrame(const FrameInfo& frame_info) {
  absl::MutexLock lock(&data_mu_);

  frames_.push_back(std::unique_ptr<FrameInfo>(new FrameInfo(frame_info)));
  return frames_.size() - 1;
}


// Note: JNIEnv* is not available through jni() call.
void JvmEvalCallStack::JvmtiOnCompiledMethodUnload(jmethodID method) {
  absl::MutexLock jmethods_writer_lock(&jmethods_mu_);
  absl::MutexLock data_writer_lock(&data_mu_);

  method_cache_.erase(method);
}


int JvmEvalCallStack::DecodeFrame(const jvmtiFrameInfo& frame_info) {
  absl::MutexLock data_writer_lock(&data_mu_);

  // Fetch or load method information.
  auto in = method_cache_.insert(
      std::make_pair(frame_info.method, MethodCache()));
  MethodCache& method_cache = in.first->second;
  if (in.second) {
    // The method was not in cache, need to load it.
    LoadMethodCache(frame_info.method, &method_cache);
  }

  // Check whether the current frame location is already in cache.
  auto it_frames_cache = method_cache.frames_cache.find(frame_info.location);
  if (it_frames_cache == method_cache.frames_cache.end()) {
    FrameInfo* fi = new FrameInfo();
    fi->class_signature = method_cache.class_signature;
    fi->class_generic = method_cache.class_generic;
    fi->method_name = method_cache.method_name;
    fi->source_file_name = method_cache.source_file_name;
    fi->line_number = GetMethodLocationLineNumber(frame_info);

    frames_.push_back(std::unique_ptr<FrameInfo>(fi));

    const int key = frames_.size() - 1;

    it_frames_cache = method_cache.frames_cache.insert(
        method_cache.frames_cache.end(),
        std::make_pair(frame_info.location, key));
  }

  return it_frames_cache->second;
}


void JvmEvalCallStack::LoadMethodCache(
    jmethodID method,
    MethodCache* method_cache) {
  jvmtiError err = JVMTI_ERROR_NONE;

  // Read method name.
  JvmtiBuffer<char> method_name;
  err = jvmti()->GetMethodName(
      method,
      method_name.ref(),
      nullptr,
      nullptr);
  if (err == JVMTI_ERROR_NONE) {
    method_cache->method_name = method_name.get();
  } else {
    LOG(ERROR) << "GetMethodName failed, error: " << err;
  }

  // Read class information.
  jclass method_class = nullptr;
  err = jvmti()->GetMethodDeclaringClass(method, &method_class);
  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "GetMethodDeclaringClass failed, error: " << err;
    method_class = nullptr;
  }

  JniLocalRef method_class_ref(method_class);

  if (method_class != nullptr) {
    // Class signature.
    JvmtiBuffer<char> class_signature;
    JvmtiBuffer<char> class_generic;
    err = jvmti()->GetClassSignature(
        method_class,
        class_signature.ref(),
        class_generic.ref());
    if (err == JVMTI_ERROR_NONE) {
      method_cache->class_signature = class_signature.get();

      if (class_generic.get() != nullptr) {
        method_cache->class_generic = class_generic.get();
      }
    } else {
      LOG(ERROR) << "GetClassSignature failed, error: " << err;
    }

    // Source file name.
    JvmtiBuffer<char> source_file_name;
    jvmtiError err = jvmti()->GetSourceFileName(
        method_class,
        source_file_name.ref());
    if (err == JVMTI_ERROR_NONE) {
      method_cache->source_file_name = source_file_name.get();
    } else if (err == JVMTI_ERROR_ABSENT_INFORMATION) {
      LOG(WARNING) << "Class doesn't have source file debugging information";
    } else if (err != JVMTI_ERROR_NONE) {
      LOG(ERROR) << "GetSourceFileName failed, error: " << err;
    }
  }
}


int JvmEvalCallStack::GetMethodLocationLineNumber(
    const jvmtiFrameInfo& frame_info) {
  jvmtiError err = JVMTI_ERROR_NONE;

  // Get the line numbers corresponding to the code statements of the method.
  jint line_entires_count = 0;
  JvmtiBuffer<jvmtiLineNumberEntry> line_entries;
  err = jvmti()->GetLineNumberTable(
      frame_info.method,
      &line_entires_count,
      line_entries.ref());

  if (err == JVMTI_ERROR_NATIVE_METHOD) {
    return -1;
  }

  if (err == JVMTI_ERROR_ABSENT_INFORMATION) {
    LOG(WARNING) << "Class doesn't have line number debugging information";
    return -1;
  }

  if (err != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "GetLineNumberTable failed, error: " << err;
    return -1;
  }

  if (line_entires_count == 0) {
    LOG(WARNING) << "GetLineNumberTable returned empty set";
    return -1;
  }

  // Find line_entries.start_location that is closest to frame_info.location
  // from the right side. The line numbers table is not necessarily sorted.
  const jvmtiLineNumberEntry* frame_entry = std::min_element(
      line_entries.get(),
      line_entries.get() + line_entires_count,
      [&frame_info] (
          const jvmtiLineNumberEntry& e1,
          const jvmtiLineNumberEntry& e2) -> bool {
        // Cast to uint64 to overflow negative numbers.
        const uint64 diff1 =
            static_cast<uint64>(frame_info.location - e1.start_location);
        const uint64 diff2 =
            static_cast<uint64>(frame_info.location - e2.start_location);

        return diff1 < diff2;
      });

  return frame_entry->line_number;
}


}  // namespace cdbg
}  // namespace devtools


