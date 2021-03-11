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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_JOBJECT_MAP_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_JOBJECT_MAP_H_

#include <cstdint>
#include <functional>
#include <list>
#include <unordered_map>

#include "common.h"

namespace devtools {
namespace cdbg {

// Non thread safe dictionary from Java class to a structure. Java classes
// don't have a unique pointer that can be used to uniquely idetify the object.
// Instead each Java object has a hash code (that might not be unique) and JNI
// provides an API to compare two objects.
// Also to store Java object either a strong or a weak reference needs to be
// taken. For more details see:
// http://docs.oracle.com/javase/6/docs/technotes/guides/jni/.
template <typename TRef, typename TData>
class JobjectMap {
 public:
  // Keep the dictionary of objects in a hash table that maps hash code to
  // list of objects with that hash code. The usage of std::unordered_map and
  // std::list is important: "Find" returns pointer to the stored data and
  // these two data structures don't relocate the data when they grow (unlike
  // std::vector).
  // We could use hash table from jobject to TData and override hash function
  // and equality operations to go to JNI. Benchmark showed no significant
  // difference in performance between the two approaches.
  typedef std::unordered_map<int32_t, std::list<std::pair<jobject, TData>>> Map;

 public:
  // Default constructor (no explicit cleanup of element values on removal).
  JobjectMap() { }

  ~JobjectMap() {
    RemoveAll();
  }

  // Enables cleanup of element values on removal.
  explicit JobjectMap(
      std::function<void(jobject, TData*)> cleanup_routine)
      : cleanup_routine_(cleanup_routine) {
  }

  // Checks whether the specified Java object is already contained in the map.
  bool Contains(jobject obj) const;

  // Looks up for the data corresponding to the specified Java object. Returns
  // nullptr if object is not in the dictionary or if error occurs.
  TData* Find(jobject obj);
  const TData* Find(jobject obj) const;

  // Inserts a new entry to the map. If the object is already present in the
  // map, the function returns false and the data structure is not changed.
  // If an error occurs taking ref, the function also returns false.
  // "inserted" is set to the actual object and data pair stored in the map.
  bool Insert(jobject obj, TData data, std::pair<jobject, TData>** inserted);

  bool Insert(jobject obj, TData data);

  // Removes the specified object from the dictionary (releasing the ref).
  // Returns true if the object was actually removed.
  bool Remove(jobject obj);

  // Removes all entries from the dictionary (releasing the refs). This
  // function doesn't have to be called on Agent_OnUnload because JVM discards
  // all the references on shutdown.
  void RemoveAll();

 private:
  // Operation to invoke upon removal of an entry from the dictionary.
  std::function<void(jobject, TData*)> cleanup_routine_;

  // Hash table of Java objects
  Map map_;

  DISALLOW_COPY_AND_ASSIGN(JobjectMap);
};


// Weak reference policy allowing JVM garbage collector to reclaim the
// referenced object.
class JObject_WeakRef {
 public:
  static jobject Create(jobject obj) {
    return jni()->NewWeakGlobalRef(obj);
  }

  static void Delete(jobject obj) {
    jni()->DeleteWeakGlobalRef(obj);
  }
};

// Global reference policy ensuring that the referenced Java object will
// be accessible as long as it is in the dictionary.
class JObject_GlobalRef {
 public:
  static jobject Create(jobject obj) {
    return jni()->NewGlobalRef(obj);
  }

  static void Delete(jobject obj) {
    jni()->DeleteGlobalRef(obj);
  }
};

// Doesn't take any references. This is useful if the dictionary only lives
// in the context of a single JNI call and is populated with local
// references.
class JObject_NoRef {
 public:
  static jobject Create(jobject obj) {
    return obj;
  }

  static void Delete(jobject obj) { }
};



template <typename TRef, typename TData>
TData* JobjectMap<TRef, TData>::Find(jobject obj) {
  jint hash_code = 0;
  jvmtiError err = jvmti()->GetObjectHashCode(obj, &hash_code);
  if (err != JVMTI_ERROR_NONE) {
    return nullptr;
  }

  auto it = map_.find(hash_code);
  if (it == map_.end()) {
    return nullptr;
  }

  for (auto& entry : it->second) {
    if (jni()->IsSameObject(obj, entry.first)) {
      return &entry.second;
    }
  }

  return nullptr;
}


template <typename TRef, typename TData>
const TData* JobjectMap<TRef, TData>::Find(jobject obj) const {
  return const_cast<JobjectMap*>(this)->Find(obj);
}


template <typename TRef, typename TData>
bool JobjectMap<TRef, TData>::Contains(jobject obj) const {
  return Find(obj) != nullptr;
}


template <typename TRef, typename TData>
bool JobjectMap<TRef, TData>::Insert(
    jobject obj,
    TData data,
    std::pair<jobject, TData>** inserted) {
  DCHECK(obj != nullptr);

  *inserted = nullptr;

  jint hash_code = 0;
  jvmtiError err = jvmti()->GetObjectHashCode(obj, &hash_code);
  if (err != JVMTI_ERROR_NONE) {
    return false;
  }

  auto in = map_.insert(
      std::make_pair(hash_code, std::list<std::pair<jobject, TData>>()));

  if (!in.second) {
    for (auto& entry : in.first->second) {
      if (jni()->IsSameObject(obj, entry.first)) {
        *inserted = &entry;
        return false;
      }
    }
  }

  jobject ref = TRef::Create(obj);
  if (ref == nullptr) {
    return false;
  }

  in.first->second.push_back(std::make_pair(ref, std::move(data)));
  *inserted = &in.first->second.back();

  return true;
}


template <typename TRef, typename TData>
bool JobjectMap<TRef, TData>::Insert(jobject obj, TData data) {
  std::pair<jobject, TData>* inserted;
  return Insert(obj, std::move(data), &inserted);
}


template <typename TRef, typename TData>
bool JobjectMap<TRef, TData>::Remove(jobject obj) {
  jint hash_code = 0;
  jvmtiError err = jvmti()->GetObjectHashCode(obj, &hash_code);
  if (err != JVMTI_ERROR_NONE) {
    return false;
  }

  auto map_it = map_.find(hash_code);
  if (map_it == map_.end()) {
    return false;
  }

  auto& list = map_it->second;

  for (auto list_it = list.begin(); list_it != list.end(); ++list_it) {
    if (jni()->IsSameObject(obj, list_it->first)) {
      TRef::Delete(list_it->first);

      list.erase(list_it);

      if (list.empty()) {
        map_.erase(map_it);
      }

      return true;
    }
  }

  return false;
}


template <typename TRef, typename TData>
void JobjectMap<TRef, TData>::RemoveAll() {
  for (auto& map_entry : map_) {
    for (auto& list_entry : map_entry.second) {
      if (cleanup_routine_ != nullptr) {
        cleanup_routine_(list_entry.first, &list_entry.second);
      }
      TRef::Delete(list_entry.first);
    }
  }

  map_.clear();
}


}  // namespace cdbg
}  // namespace devtools

#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_JOBJECT_MAP_H_
