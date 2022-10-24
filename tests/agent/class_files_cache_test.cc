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

#include "src/agent/class_files_cache.h"

#include <functional>
#include <memory>

#include "gtest/gtest.h"

#include "jni_proxy_classfiletextifier.h"
#include "jni_proxy_classloader.h"
#include "jni_proxy_classpathlookup.h"
#include "jni_proxy_jasmin_main.h"
#include "jni_proxy_java_net_url.h"
#include "jni_proxy_java_net_urlclassloader.h"
#include "src/agent/jni_utils.h"
#include "src/agent/jvm_class_indexer.h"
#include "src/agent/type_util.h"
#include "tests/agent/file_utils.h"

namespace devtools {
namespace cdbg {

// Smallest class file that this test can produce.
static constexpr int kDefaultClassFileSize = 150;

class ClassFileCacheTest : public testing::Test {
 protected:
  ClassFileCacheTest() : cache_(&class_indexer_, 10000) {}

  void SetUp() override {
    CHECK(BindSystemClasses());
    CHECK(jniproxy::BindClassPathLookup());
    CHECK(jniproxy::BindClassFileTextifier());
    CHECK(jniproxy::BindURL());
    CHECK(jniproxy::BindURLClassLoader());
    CHECK(jniproxy::jasmin::BindMain());

    temp_path_.reset(new TempPath());

    class_indexer_.Initialize();
  }

  void TearDown() override {
    jniproxy::CleanupClassPathLookup();
    jniproxy::CleanupClassFileTextifier();
    jniproxy::CleanupURL();
    jniproxy::CleanupURLClassLoader();
    jniproxy::jasmin::CleanupMain();
    CleanupSystemClasses();
  }

  // Dynamically builds a class file of a requested size on first
  // call to ClassIndexer::Type::FindClass(). The generated class just contains
  // a lot of NOPs, but "ClassFilesCache" doesn't care about it.
  JniLocalRef GenerateClass(int size) {
    const std::string signature = NextSignature();

    // Generate bytecode for a method that will take exactly "size_" bytes.
    // We assume that NOP takes exactly one byte.
    std::string asm_code;
    asm_code += ".class public " +
                BinaryNameFromJObjectSignature(signature) +
                "\n";
    asm_code += ".super java/lang/Object\n";
    asm_code += ".method public static test()V\n";

    for (int i = 0; i < size - kDefaultClassFileSize; ++i) {
      asm_code += "nop\n";
    }

    asm_code += ".end method\n";

    const std::string source_path = JoinPath(temp_path_->path(), "source.j");
    EXPECT_TRUE(SetFileContents(source_path, asm_code));

    // Compile the assembly code into a class file.
    jniproxy::jasmin::Main()->assemble(temp_path_->path(), source_path, false)
        .Release(ExceptionAction::LOG_AND_IGNORE);

    // Verify that output file is there and that it's the right size.
    std::string internal_name = signature;
    internal_name = internal_name.substr(1, internal_name.size() - 2);

    std::string blob;
    EXPECT_TRUE(GetFileContents(
        JoinPath(temp_path_->path(), internal_name + ".class"),
        &blob));

    EXPECT_EQ(blob.size(), size);

    // Create a class loader to load the newly generated class.
    JniLocalRef url =
        jniproxy::URL()->NewObject("file:" + temp_path_->path() + "/")
            .Release(ExceptionAction::LOG_AND_IGNORE);
    EXPECT_TRUE(url != nullptr);

    JniLocalRef urls(
        jni()->NewObjectArray(1, jniproxy::URL()->GetClass(), nullptr));
    urls = CatchOr("NewObjectArray", std::move(urls))
        .Release(ExceptionAction::LOG_AND_IGNORE);
    EXPECT_TRUE(urls != nullptr);

    jni()->SetObjectArrayElement(
        static_cast<jobjectArray>(urls.get()),
        0,
        url.get());
    CatchOr("SetObjectArrayElement", nullptr)
        .Release(ExceptionAction::LOG_AND_IGNORE);

    JniLocalRef loader =
        jniproxy::URLClassLoader()->NewObject(urls.get())
            .Release(ExceptionAction::LOG_AND_IGNORE);
    EXPECT_TRUE(loader != nullptr);

    // Load the generated class.
    const std::string binary_name = BinaryNameFromJObjectSignature(signature);
    JniLocalRef local_ref =
        jniproxy::ClassLoader()->loadClass(loader.get(), binary_name)
            .Release(ExceptionAction::LOG_AND_IGNORE);
    EXPECT_TRUE(local_ref != nullptr);

    return local_ref;
  }

  // Generate next unique class signature.
  static std::string NextSignature() {
    static int counter = 0;

    std::string current = std::to_string(++counter);
    current = std::string(4 - current.size(), '0') + current;

    return "Lmy/test/NopClass" + current + ";";
  }

 protected:
  std::unique_ptr<TempPath> temp_path_;
  JvmClassIndexer class_indexer_;
  ClassFilesCache cache_;
};


TEST_F(ClassFileCacheTest, Empty) {
  EXPECT_EQ(0, cache_.total_size());
}


TEST_F(ClassFileCacheTest, GetSmallFromEmpty) {
  JniLocalRef cls = GenerateClass(250);

  bool loaded = false;
  auto class_file = cache_.GetOrLoad(cls.get(), &loaded);
  EXPECT_TRUE(class_file != nullptr);
  EXPECT_EQ(250, cache_.total_size());
  EXPECT_TRUE(loaded);
}


TEST_F(ClassFileCacheTest, GetAndDiscard) {
  JniLocalRef cls = GenerateClass(250);
  bool loaded = false;
  EXPECT_TRUE(cache_.GetOrLoad(cls.get(), &loaded) != nullptr);
}


TEST_F(ClassFileCacheTest, GetNotAvailable) {
  jclass cls = class_indexer_.GetReference("Ljava/lang/String;")->FindClass();
  ASSERT_NE(nullptr, cls);

  EXPECT_EQ(nullptr, cache_.Get(cls));
}


TEST_F(ClassFileCacheTest, Cache) {
  JniLocalRef cls = GenerateClass(1000);

  ClassFile* class_file;
  {
    bool loaded = false;
    auto ref = cache_.GetOrLoad(cls.get(), &loaded);
    EXPECT_TRUE(ref != nullptr);
    EXPECT_TRUE(loaded);
    class_file = ref->get();
  }

  {
    bool loaded = false;
    auto ref = cache_.GetOrLoad(cls.get(), &loaded);
    EXPECT_TRUE(ref != nullptr);
    EXPECT_EQ(class_file, ref->get());
    EXPECT_FALSE(loaded);
  }

  {
    auto ref = cache_.Get(cls.get());
    EXPECT_TRUE(ref != nullptr);
    EXPECT_EQ(class_file, ref->get());
  }
}


TEST_F(ClassFileCacheTest, MultipleReferences) {
  JniLocalRef cls_big = GenerateClass(10000);

  bool loaded;
  auto ref1 = cache_.GetOrLoad(cls_big.get(), &loaded);
  EXPECT_TRUE(ref1 != nullptr);

  // Verify that class [1] is not going to be garbage collected.
  {
    JniLocalRef cls_small = GenerateClass(1000);
    auto small = cache_.GetOrLoad(cls_small.get(), &loaded);
    EXPECT_TRUE(small != nullptr);
    EXPECT_EQ(11000, cache_.total_size());
  }

  auto ref2 = cache_.GetOrLoad(cls_big.get(), &loaded);
  EXPECT_TRUE(ref2 != nullptr);

  // Verify that class [1] still is not going to be garbage collected.
  {
    JniLocalRef cls_small = GenerateClass(1000);
    auto small = cache_.GetOrLoad(cls_small.get(), &loaded);
    EXPECT_TRUE(small != nullptr);
    EXPECT_EQ(11000, cache_.total_size());
  }

  ref1 = nullptr;

  // Class [1] is still referenced through a different instance.
  {
    JniLocalRef cls_small = GenerateClass(1000);
    auto small = cache_.GetOrLoad(cls_small.get(), &loaded);
    EXPECT_TRUE(small != nullptr);
    EXPECT_EQ(11000, cache_.total_size());
  }

  ref2 = nullptr;

  // Now it will be garbage collected
  {
    JniLocalRef cls_small = GenerateClass(1000);
    auto small = cache_.GetOrLoad(cls_small.get(), &loaded);
    EXPECT_TRUE(small != nullptr);
    EXPECT_EQ(1000, cache_.total_size());
  }
}


TEST_F(ClassFileCacheTest, ReferenceFromLRU) {
  JniLocalRef cls1 = GenerateClass(1000);
  JniLocalRef cls2 = GenerateClass(10000);
  bool loaded;

  // Reference the class and return it back to LRU.
  ClassFile* class_file1;
  {
    auto ref1 = cache_.GetOrLoad(cls1.get(), &loaded);
    EXPECT_TRUE(ref1 != nullptr);
    class_file1 = ref1->get();
  }

  // Reference the class from the LRU.
  ClassFile* class_file2;
  {
    auto ref1 = cache_.GetOrLoad(cls1.get(), &loaded);
    EXPECT_TRUE(ref1 != nullptr);
    class_file2 = ref1->get();

    // Make sure the first class is not garbage collected.
    auto ref2 = cache_.GetOrLoad(cls2.get(), &loaded);
    EXPECT_NE(nullptr, ref2);

    EXPECT_EQ(11000, cache_.total_size());
  }

  EXPECT_EQ(class_file1, class_file2);
}


TEST_F(ClassFileCacheTest, KeepInCacheWithinLimit) {
  JniLocalRef cls1 = GenerateClass(1000);
  JniLocalRef cls2 = GenerateClass(1001);
  JniLocalRef cls3 = GenerateClass(1002);
  bool loaded;

  EXPECT_TRUE(cache_.GetOrLoad(cls1.get(), &loaded) != nullptr);
  EXPECT_TRUE(cache_.GetOrLoad(cls2.get(), &loaded) != nullptr);
  EXPECT_TRUE(cache_.GetOrLoad(cls3.get(), &loaded) != nullptr);

  EXPECT_EQ(1000 + 1001 + 1002, cache_.total_size());
}


TEST_F(ClassFileCacheTest, KeepInCacheBeyondLimit) {
  JniLocalRef cls1 = GenerateClass(4000);
  JniLocalRef cls2 = GenerateClass(4001);
  JniLocalRef cls3 = GenerateClass(4002);
  bool loaded;

  auto refs = {
      cache_.GetOrLoad(cls1.get(), &loaded),
      cache_.GetOrLoad(cls2.get(), &loaded),
      cache_.GetOrLoad(cls3.get(), &loaded)
  };

  for (const auto& ref : refs) {
    EXPECT_TRUE(ref != nullptr);
  }

  // Verify that all the classes are still in cache, because all 3 are borrowed.
  EXPECT_EQ(4000 + 4001 + 4002, cache_.total_size());
}


TEST_F(ClassFileCacheTest, GarbageCollectSingle) {
  JniLocalRef cls1 = GenerateClass(4000);
  JniLocalRef cls2 = GenerateClass(4001);
  bool loaded;

  EXPECT_TRUE(cache_.GetOrLoad(cls1.get(), &loaded) != nullptr);
  EXPECT_TRUE(cache_.GetOrLoad(cls2.get(), &loaded) != nullptr);

  // Verify that the first two classes are in cache.
  EXPECT_EQ(4000 + 4001, cache_.total_size());

  // Now load the third class.
  JniLocalRef cls3 = GenerateClass(4002);
  auto class_file3 = cache_.GetOrLoad(cls3.get(), &loaded);
  EXPECT_TRUE(class_file3 != nullptr);

  // Verify that generator1 was garbage collected.
  EXPECT_EQ(4001 + 4002, cache_.total_size());
}


TEST_F(ClassFileCacheTest, GarbageCollectMultiple) {
  JniLocalRef cls1 = GenerateClass(1000);
  JniLocalRef cls2 = GenerateClass(1001);
  JniLocalRef cls3 = GenerateClass(1002);
  bool loaded;

  EXPECT_TRUE(cache_.GetOrLoad(cls1.get(), &loaded) != nullptr);
  EXPECT_TRUE(cache_.GetOrLoad(cls2.get(), &loaded) != nullptr);
  EXPECT_TRUE(cache_.GetOrLoad(cls3.get(), &loaded) != nullptr);

  // Verify that the first three classes are in cache.
  EXPECT_EQ(1000 + 1001 + 1002, cache_.total_size());

  // Now load another class that will displace [1] and [2].
  JniLocalRef cls4 = GenerateClass(10000 - 1002);
  auto class_file4 = cache_.GetOrLoad(cls4.get(), &loaded);
  EXPECT_TRUE(class_file4 != nullptr);

  // Verify that [1] and [2] were garbage collected.
  EXPECT_EQ(10000, cache_.total_size());
}


TEST_F(ClassFileCacheTest, ClassFileNotAvailable) {
  JniLocalRef cls = GenerateClass(1000);

  // Delete the .class file from disk.
  temp_path_ = nullptr;

  bool loaded;
  EXPECT_EQ(nullptr, cache_.GetOrLoad(cls.get(), &loaded));
}

}  // namespace cdbg
}  // namespace devtools

