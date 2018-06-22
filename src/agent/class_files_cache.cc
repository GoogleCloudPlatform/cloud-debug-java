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

#include "class_files_cache.h"

namespace devtools {
namespace cdbg {

ClassFilesCache::ClassFilesCache(ClassIndexer* class_indexer, int max_size)
    : class_indexer_(class_indexer),
      max_size_(max_size) {
}


std::unique_ptr<ClassFilesCache::AutoClassFile> ClassFilesCache::Get(
    jobject cls) {
  absl::MutexLock lock(&mu_);

  Item* item = classes_.Find(cls);
  if (item == nullptr) {
    return nullptr;
  }

  return Reference(item);
}


std::unique_ptr<ClassFilesCache::AutoClassFile> ClassFilesCache::GetOrLoad(
    jobject cls,
    bool* loaded) {
  *loaded = false;

  {
    absl::MutexLock lock(&mu_);
    Item* item = classes_.Find(cls);
    if (item != nullptr) {
      return Reference(item);
    }
  }

  std::unique_ptr<ClassFile> class_file =
      ClassFile::Load(class_indexer_, static_cast<jclass>(cls));
  if (class_file == nullptr) {
    LOG(WARNING) << "Failed to load Java class " << GetClassSignature(cls);
    return nullptr;
  }

  LOG(INFO) << "Java class file loaded: " << GetClassSignature(cls);
  *loaded = true;

  {
    absl::MutexLock lock(&mu_);

    // The class could be inserted into the cache by another thread while
    // this thread was calling "ClassFile::Load".
    Item* already_inserted_item = classes_.Find(cls);
    if (already_inserted_item != nullptr) {
      return Reference(already_inserted_item);
    }

    Item item;
    item.class_file = std::move(class_file);
    item.it_lru = lru_.end();
    item.ref_count = 1;

    std::pair<jobject, Item>* inserted = nullptr;
    if (!classes_.Insert(cls, std::move(item), &inserted)) {
      LOG(ERROR) << "Class could not be inserted into cache.";
      return nullptr;
    }

    inserted->second.cls = inserted->first;

    total_size_ += inserted->second.class_file->GetData().size();
    GarbageCollect();
    return std::unique_ptr<ClassFilesCache::AutoClassFile>(
        new AutoClassFile(this, &inserted->second));
  }
}


std::unique_ptr<ClassFilesCache::AutoClassFile> ClassFilesCache::Reference(
    ClassFilesCache::Item* item) {
  if (item->ref_count == 0) {
    DCHECK(item->it_lru != lru_.end());

    lru_.erase(item->it_lru);
    item->it_lru = lru_.end();
  } else {
    DCHECK(item->it_lru == lru_.end());
  }

  ++item->ref_count;

  return std::unique_ptr<ClassFilesCache::AutoClassFile>(
      new AutoClassFile(this, item));
}


void ClassFilesCache::Unref(ClassFilesCache::Item* item) {
  absl::MutexLock lock(&mu_);

  DCHECK_GT(item->ref_count, 0);

  --item->ref_count;

  if (item->ref_count == 0) {
    item->it_lru = lru_.insert(lru_.end(), item->cls);
  }
}


void ClassFilesCache::GarbageCollect() {
  while ((total_size_ > max_size_) && !lru_.empty()) {
    Item* item = classes_.Find(lru_.front());
    DCHECK(item != nullptr);

    LOG(INFO) << "Java class file "
              << GetClassSignature(lru_.front())
              << " removed from cache";

    total_size_ -= item->class_file->GetData().size();
    classes_.Remove(item->cls);
    lru_.pop_front();
  }
}

}  // namespace cdbg
}  // namespace devtools

