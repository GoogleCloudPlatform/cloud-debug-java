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

#ifndef DEVTOOLS_CDBG_DEBUGLETS_JAVA_CLASS_FILES_CACHE_H_
#define DEVTOOLS_CDBG_DEBUGLETS_JAVA_CLASS_FILES_CACHE_H_

#include <list>

#include "nullable.h"
#include "class_file.h"
#include "class_indexer.h"
#include "common.h"
#include "jobject_map.h"
#include "mutex.h"

namespace devtools {
namespace cdbg {

// Loading Java class files from disk is an expensive operation. This class
// implements a simple LRU cache to avoid unnecessary class loads.
//
// Each class in the cache can be in one of two states:
// 1. Referenced by one or more consumers. The same copy of the class file is
//    shared between everyone. While the class is referenced, it will never
//    be garbage collected.
// 2. When the class is not referenced, it moves to LRU list. Classes will be
//    garbage collected from the LRU list when a new class needs to be loaded
//    and the cache has not enough space.
class ClassFilesCache {
 private:
  struct Item {
    // Global reference to the class file represented by this instance. The
    // reference is owned by "JobjectMap".
    jobject cls = nullptr;

    // Loaded Java class file. "ClassFile" is thread safe, so "class_file" is
    // shared between all the threads that referenced it.
    std::unique_ptr<ClassFile> class_file;

    // Reference count of this class file. When this count is zero, the class
    // file is not referenced by anyone and "it_lru" points to the location of
    // this class in LRU list.
    int ref_count = 0;

    // Ignored if "ref_count" is non-zero. Otherwise points to the location
    // of this class in LRU list. We use linked list, since its iterators
    // stay valid when the list is changed.
    std::list<jobject>::iterator it_lru;
  };

 public:
  // Automatically returns the referenced class file when goes out of scope.
  class AutoClassFile {
   public:
    AutoClassFile(ClassFilesCache* owner, Item* item)
       : owner_(owner), item_(item) {
    }

    ~AutoClassFile() { owner_->Unref(item_); }

    ClassFile* get() { return item_->class_file.get(); }

   private:
    ClassFilesCache* const owner_;
    Item* const item_;

    DISALLOW_COPY_AND_ASSIGN(AutoClassFile);
  };

  // Class constructor. "class_indexer" is not owned by this class and must
  // outlive it. It is used to load new class files when not in cache.
  // The actual spaced used by "ClassFilesCache" may exceed this threshold
  // if too many class files are referenced at the same time.
  ClassFilesCache(ClassIndexer* class_indexer, int max_size);

  // Gets the class file for the specified class from cache. Returns nullptr
  // if the class file is not in cache.
  std::unique_ptr<AutoClassFile> Get(jobject cls);

  // Gets the class file for the specified class from cache. Loads the class
  // file if not found. If the class file was actually loaded (rather than
  // retrieved from cache), "loaded" is set to true on return.
  std::unique_ptr<AutoClassFile> GetOrLoad(jobject cls, bool* loaded);

  // Returns the total size in bytes of all the class files in cache. This
  // number can exceed the maximum size if too many class files are referenced
  // at the same time.
  int total_size() const { return total_size_; }

 private:
  // Increases the "ref_count" if the class file is already referenced.
  // Otherwise removes it from LRU list and sets "ref_count" to 1.
  // Must be called with "mu_" locked.
  std::unique_ptr<ClassFilesCache::AutoClassFile> Reference(Item* item);

  // Returns the class file to the cache. If the class file is not referenced
  // any more, adds the class file to LRU cache. Must be called with "mu_"
  // unlocked.
  void Unref(Item* item);

  // Releases class files from LRU list as long as the total space used by
  // the cache exceeds the threshold. Must be called with "mu_" locked.
  void GarbageCollect();

 private:
  // Used to load Java classes. See "ClassFile" for more details.
  ClassIndexer* const class_indexer_;

  // Maximum total size of the class files before the garbage collection kicks
  // in and starts releasing unreferenced class files.
  const int max_size_;

  // Locks changes to "classes_, "lru_" and "total_size_".
  absl::Mutex mu_;

  // All cached class files. We keep "Item" by value because "JobjectMap"
  // guarantees that it will not move when the entries are added or removed
  // from it.
  JobjectMap<JObject_GlobalRef, Item> classes_;

  // Class files that are not referenced and can be released if the cache
  // needs more space. The most recently used classes are at the end. Class
  // files are garbage collected starting from the front.
  std::list<jobject> lru_;

  // Total number of bytes used by "ClassFile" instances of "classes_".
  int total_size_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ClassFilesCache);
};

}  // namespace cdbg
}  // namespace devtools


#endif  // DEVTOOLS_CDBG_DEBUGLETS_JAVA_CLASS_FILES_CACHE_H_
