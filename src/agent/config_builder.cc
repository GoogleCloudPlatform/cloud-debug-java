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

#include "config_builder.h"

#include <cstdint>
#include <sstream>

#include "safe_caller_proxies.h"

// Multiple items in flags like "extra_allowed_methods" are separated with
// a colon. Method names are specified as "class#method".

ABSL_FLAG(bool, enable_safe_caller, true,
          "Allows any method without side effects in expressions");

ABSL_FLAG(string, extra_blocked_methods, "",
          "Additional methods to block for testing purposes");

ABSL_FLAG(string, extra_allowed_methods, "",
          "Additional methods to allowed for testing purposes");

ABSL_FLAG(string, extra_whitelisted_classes, "",
          "Internal names of additional classes to allow for testing purposes");

ABSL_FLAG(int32, expression_max_classes_load_quota, 5,
          "Maximum number of classes that the NanoJava interpreter is allowed "
          "to load while evaluating a single breakpoint expression");

ABSL_FLAG(
    int32, expression_max_interpreter_instructions_quota, 1000,
    "Maximum number of instructions that the NanoJava interpreter is allowed "
    "to execute while evaluating a single breakpoint expression");

ABSL_FLAG(int32, pretty_printers_max_classes_load_quota, 5,
          "Maximum number of classes that the NanoJava interpreter is allowed "
          "to load while formatting some well known data structures");

ABSL_FLAG(
    int32, pretty_printers_max_interpreter_instructions_quota, 1000,
    "Maximum number of instructions that the NanoJava interpreter is allowed "
    "to execute while formatting some well known data structures");

ABSL_FLAG(int32, dynamic_log_max_classes_load_quota, 5,
          "Maximum number of classes that the NanoJava interpreter is allowed "
          "to load while evaluating all expressions in a single dynamic log "
          "statement");

ABSL_FLAG(
    int32, dynamic_log_max_interpreter_instructions_quota, 1000,
    "Maximum number of instructions that the NanoJava interpreter is allowed "
    "to execute while evaluating all expressions in a single dynamic log "
    "statement");

ABSL_FLAG(
    int32, safe_caller_max_array_elements, 65536,
    "Maximum allowed size of the array to copy or allocate in safe caller"
    "(copying or allocating larger arrays is considered to be too expensive "
    "and will be blocked)");

ABSL_FLAG(int32, safe_caller_max_interpreter_stack_depth, 20,
          "Maximum stack depth that safe caller will allow");

namespace devtools {
namespace cdbg {

// Helper class to build Config::Method in a single statement without explicitly
// defined local variables.
class MethodRuleBuilder {
 public:
  explicit MethodRuleBuilder(Config::Method rule) : rule_(std::move(rule)) {
  }

  MethodRuleBuilder& signature(const std::string& value) {
    rule_.signature = value;
    return *this;
  }

  MethodRuleBuilder& pre_call(
      std::function<MethodCallResult(
          SafeMethodCaller*,
          jobject source,
          std::vector<JVariant>*)> callback) {
    DCHECK(rule_.action == Config::Method::CallAction::Allow);
    rule_.thunk = callback;
    return *this;
  }

  MethodRuleBuilder& applies_to_derived_classes() {
    // Derived class can define method with the same name, but different
    // signature. It is unsafe to allow it. It would also be nice to assert
    // here that the method is either static or final, but there is no easy
    // way to do that.
    DCHECK(!rule_.signature.empty()) << "Unsafe configuration";

    rule_.applies_to_derived_classes = true;
    return *this;
  }

  MethodRuleBuilder& require_temporary_object() {
    DCHECK(rule_.action == Config::Method::CallAction::Allow);
    rule_.require_temporary_object = true;
    return *this;
  }

  MethodRuleBuilder& returns_temporary_object() {
    DCHECK(rule_.action == Config::Method::CallAction::Allow);
    rule_.returns_temporary_object = true;
    return *this;
  }

  const Config::Method& build() const {
    return rule_;
  }

 private:
  Config::Method rule_;
};


static MethodRuleBuilder AllowAll() {
  Config::Method rule;
  rule.action = Config::Method::CallAction::Allow;
  return MethodRuleBuilder(rule);
}

static MethodRuleBuilder Allow(std::string method_name) {
  Config::Method rule;
  rule.action = Config::Method::CallAction::Allow;
  rule.name = std::move(method_name);
  return MethodRuleBuilder(rule);
}

static MethodRuleBuilder BlockAll() {
  Config::Method rule;
  rule.action = Config::Method::CallAction::Block;
  return MethodRuleBuilder(rule);
}

static MethodRuleBuilder Block(std::string method_name) {
  Config::Method rule;
  rule.action = Config::Method::CallAction::Block;
  rule.name = std::move(method_name);
  return MethodRuleBuilder(rule);
}

static MethodRuleBuilder InterpretAll() {
  Config::Method rule;
  rule.action = absl::GetFlag(FLAGS_enable_safe_caller)
                    ? Config::Method::CallAction::Interpret
                    : Config::Method::CallAction::Block;
  return MethodRuleBuilder(rule);
}

static MethodRuleBuilder Interpret(std::string method_name) {
  Config::Method rule;
  rule.action = absl::GetFlag(FLAGS_enable_safe_caller)
                    ? Config::Method::CallAction::Interpret
                    : Config::Method::CallAction::Block;
  rule.name = std::move(method_name);
  return MethodRuleBuilder(rule);
}

static std::vector<Config::Method> ToMethods(
    std::initializer_list<MethodRuleBuilder> rules) {
  std::vector<Config::Method> methods;
  methods.reserve(rules.size());
  for (const MethodRuleBuilder& rule : rules) {
    methods.push_back(rule.build());
  }

  return methods;
}


// Split string separated by a semicolon delimiter.
static std::vector<std::string> SplitString(const std::string& s) {
  std::stringstream ss(s);
  std::string item;
  std::vector<std::string> items;
  while (std::getline(ss, item, ':')) {
    items.push_back(std::move(item));
  }

  return items;
}

// Splits "class#method" into the two strings.
static std::pair<std::string, std::string> SplitMethod(const std::string& s) {
  size_t pos = s.find('#');
  if (pos == std::string::npos) {
    DCHECK(false) << s;
    return std::make_pair(std::string(), std::string());
  }

  return std::make_pair(s.substr(0, pos), s.substr(pos + 1));
}

// Builds default configuration of safe method calling.
static std::map<std::string, std::vector<Config::Method>>
DefaultMethodsConfig() {
  // Build set of classes, but use internal names, to make the code cleaner.
  std::map<std::string, std::vector<Config::Method>> classes;

  [&classes]() {
      classes["java/lang/Object"] = ToMethods({
        Allow("equals")
            .signature("(Ljava/lang/Object;)Z")
            .applies_to_derived_classes(),
        Allow("getClass")
            .signature("()Ljava/lang/Class;")
            .applies_to_derived_classes(),
        Allow("hashCode")
            .signature("()I")
            .applies_to_derived_classes(),
        Allow("clone")
            .signature("()Ljava/lang/Object;")
            .applies_to_derived_classes()
            .pre_call(ObjectClonePre)
            .returns_temporary_object(),
        Allow("toString")
            .signature("()Ljava/lang/String;")
            .applies_to_derived_classes(),
        Block("wait")
            .signature("()V")
            .applies_to_derived_classes(),
        Block("wait")
            .signature("(J)V")
            .applies_to_derived_classes(),
        Block("wait")
            .signature("(JI)V")
            .applies_to_derived_classes(),
        Block("notify")
            .signature("()V")
            .applies_to_derived_classes(),
        Block("notifyAll")
            .signature("()V")
            .applies_to_derived_classes()
      });
  }();

  [&classes]() {
      classes["java/lang/Class"] = ToMethods({
        Block("forName"),
        Block("getClassLoader"),
        Block("getClassLoader0"),
        Block("newInstance"),
        Block("setSigners"),
        Allow("getCanonicalName"),
        Allow("getComponentType"),
        Allow("getDeclaringClass"),
        Allow("getEnclosingClass"),
        Allow("getEnumConstants"),
        Allow("getGenericInterfaces"),
        Allow("getGenericSuperclass"),
        Allow("getInterfaces"),
        Allow("getModifiers"),
        Allow("getName"),
        Allow("getPackage"),
        Allow("getSigners"),
        Allow("getSimpleBinaryName"),
        Allow("getSimpleName"),
        Allow("getSuperclass"),
        Allow("getTypeParameters"),
        Allow("isAnnotation"),
        Allow("isAnonymousClass"),
        Allow("isArray"),
        Allow("isAssignableFrom"),
        Allow("isEnum"),
        Allow("isInstance"),
        Allow("isInterface"),
        Allow("isLocalClass"),
        Allow("isLocalOrAnonymousClass"),
        Allow("isMemberClass"),
        Allow("isPrimitive"),
        Allow("isSynthetic"),
        Allow("toString")
      });
  }();

  [&classes]() {
      classes["java/lang/Math"] = ToMethods({ AllowAll() });
  }();

  [&classes]() {
      classes["java/lang/StrictMath"] = ToMethods({ AllowAll() });
  }();

  [&classes]() {
      classes["java/math/BigDecimal"] = ToMethods({
        Allow("toString")
      });
  }();

  [&classes]() {
      classes["java/math/BigInteger"] = ToMethods({
        Allow("toString")
      });
  }();

  [&classes]() {
      classes["java/util/Date"] = ToMethods({
        Allow("after"),
        Allow("before"),
        Allow("clone"),
        Allow("compareTo"),
        Allow("equals"),
        Allow("getTime"),
        Allow("hashCode"),
        Allow("toString"),
      });
  }();

  [&classes]() {
      // TODO: add "pre_call" to string constructors that copy array.
      classes["java/lang/String"] = ToMethods({
        Allow("format").pre_call(StringFormatPre),
        Interpret("copyValueOf"),  // unsafe if the string is too long.
        Interpret("getBytes"),  // unsafe if the string is too long.
        Interpret("getChars"),  // unsafe unless destination is temporary.
        Interpret("toCharArray"),  // unsafe if the string is too long.
        Interpret("valueOf")
            .signature("(Ljava/lang/Object;)Ljava/lang/String;"),
        AllowAll()
      });
  }();

  [&classes]() {
    classes["java/lang/StringBuilder"] = ToMethods({
      Interpret("append")
          .signature("(Ljava/lang/Object;)Ljava/lang/StringBuilder;"),
      Allow("append")
          .require_temporary_object(),
    });
  }();

  [&classes]() {
      classes["java/lang/System"] = ToMethods({
        Allow("arraycopy").pre_call(SystemArraycopyPre),
        Allow("getenv"),
        Allow("getProperties"),
        Allow("getProperty"),
      });
  }();

  [&classes]() {
      // TODO: augment with interpreter call stack (which JDK
      // implementation of "Throwable.fillInStackTrace" is not aware of).
      classes["java/lang/Throwable"] = ToMethods({
        Allow("fillInStackTrace")
            .signature("()Ljava/lang/Throwable;")
            .require_temporary_object()
            .applies_to_derived_classes()
      });
  }();

  [&classes]() {
    const std::string wrapper_types[] = {"Boolean",   "Byte",    "Short",
                                         "Character", "Integer", "Long",
                                         "Float",     "Double"};

    for (const std::string& wrapper_type : wrapper_types) {
      classes["java/lang/" + wrapper_type] = ToMethods({AllowAll()});
    }
  }();

  [&classes]() {
    const std::string collection_classes[] = {
        "com/google/common/collect/ImmutableMapEntrySet$RegularEntrySet",
        "com/google/common/collect/RegularImmutableList",
        "com/google/common/collect/RegularImmutableMap",
        "com/google/common/collect/RegularImmutableMultiset",
        "com/google/common/collect/RegularImmutableSet",
        "com/google/common/collect/RegularImmutableSortedSet",
        "com/google/common/collect/SingletonImmutableList",
        "com/google/common/collect/SingletonImmutableSet",
        "java/util/ArrayDeque",
        "java/util/ArrayList",
        "java/util/Arrays$ArrayList",
        "java/util/Collections$EmptyList",
        "java/util/Collections$EmptyMap",
        "java/util/Collections$EmptySet",
        "java/util/Collections$UnmodifiableMap",
        "java/util/Collections$UnmodifiableMap$UnmodifiableEntrySet",
        "java/util/concurrent/ArrayBlockingQueue",
        "java/util/concurrent/ConcurrentHashMap",
        "java/util/concurrent/ConcurrentHashMap$EntrySet",
        "java/util/concurrent/ConcurrentHashMap$EntrySetView",
        "java/util/concurrent/ConcurrentLinkedDeque",
        "java/util/concurrent/ConcurrentLinkedQueue",
        "java/util/concurrent/ConcurrentSkipListSet",
        "java/util/concurrent/ConcurrentSkipListMap",
        "java/util/concurrent/ConcurrentSkipListMap$EntrySet",
        "java/util/concurrent/ConcurrentSkipListMap$KeySet",
        "java/util/concurrent/CopyOnWriteArrayList",
        "java/util/concurrent/CopyOnWriteArraySet",
        "java/util/concurrent/LinkedBlockingDeque",
        "java/util/concurrent/LinkedBlockingQueue",
        "java/util/EnumMap",
        "java/util/EnumSet",
        "java/util/HashMap",
        "java/util/HashMap$EntrySet",
        "java/util/HashSet",
        "java/util/Hashtable",
        "java/util/IdentityHashMap",
        "java/util/LinkedHashMap",
        "java/util/LinkedHashMap$LinkedEntrySet",
        "java/util/LinkedHashSet",
        "java/util/LinkedList",
        "java/util/PriorityQueue",
        "java/util/Properties",
        "java/util/Stack",
        "java/util/TreeMap",
        "java/util/TreeMap$EntrySet",
        "java/util/TreeSet",
        "java/util/Vector",
        "java/util/WeakHashMap",
        "java/util/WeakHashMap$EntrySet",
    };

    for (const std::string& collection_class : collection_classes) {
      classes[collection_class] =
          ToMethods({Allow("entrySet"), Allow("get"), Allow("isEmpty"),
                     Allow("iterator").returns_temporary_object(),
                     Allow("keySet"), Allow("size")});
    }
  }();

  [&classes]() {
    const std::string iterator_classes[] = {
        "com/google/common/collect/ImmutableMultiset$1",
        "java/util/AbstractList$Itr",
        "java/util/ArrayDeque$DeqIterator",
        "java/util/ArrayList$Itr",
        "java/util/Collections$EmptyIterator",
        "java/util/Collections$UnmodifiableMap$UnmodifiableEntrySet$1",
        "java/util/HashMap$EntryIterator",
        "java/util/HashMap$KeyIterator",
        "java/util/Hashtable$Enumerator",
        "java/util/LinkedHashMap$EntryIterator",
        "java/util/LinkedHashMap$KeyIterator",
        "java/util/LinkedHashMap$LinkedEntryIterator",
        "java/util/LinkedHashMap$LinkedKeyIterator",
        "java/util/LinkedList$ListItr",
        "java/util/PriorityQueue$Itr",
        "java/util/TreeMap$EntryIterator",
        "java/util/TreeMap$KeyIterator",
        "java/util/WeakHashMap$EntryIterator",
        "java/util/Vector$Itr",
        "java/util/concurrent/ArrayBlockingQueue$Itr",
        "java/util/concurrent/ConcurrentHashMap$EntryIterator",
        "java/util/concurrent/ConcurrentLinkedDeque$Itr",
        "java/util/concurrent/ConcurrentLinkedQueue$Itr",
        "java/util/concurrent/ConcurrentSkipListMap$EntryIterator",
        "java/util/concurrent/ConcurrentSkipListMap$KeyIterator",
        "java/util/concurrent/CopyOnWriteArrayList$COWIterator",
        "java/util/concurrent/LinkedBlockingDeque$Itr",
        "java/util/concurrent/LinkedBlockingQueue$Itr",
    };

    for (const std::string& iterator_class : iterator_classes) {
      classes[iterator_class] =
          ToMethods({Allow("hasNext").signature("()Z"),
                     Allow("next").signature("()Ljava/lang/Object;")});
    }
  }();

  [&classes]() {
    const std::string iterator_classes[] = {
        "com/google/common/collect/AbstractIndexedListIterator",
    };

    for (const std::string& iterator_class : iterator_classes) {
      classes[iterator_class] = ToMethods({
        Allow("hasNext")
            .signature("()Z")
            .applies_to_derived_classes(),
        Allow("next")
            .signature("()Ljava/lang/Object;")
            .applies_to_derived_classes()
      });
    }
  }();

  [&classes]() {
    const std::string map_entry_classes[] = {
        "com/google/common/collect/ImmutableEntry",
        "com/google/common/collect/ImmutableMapEntry",
        "com/google/common/collect/"
        "ImmutableMapEntry$NonTerminalImmutableMapEntry",
        "java/util/AbstractMap$SimpleImmutableEntry",
        "java/util/"
        "Collections$UnmodifiableMap$UnmodifiableEntrySet$UnmodifiableEntry",
        "java/util/HashMap$Entry",  // Java 7.
        "java/util/HashMap$Node",   // Java 8.
        "java/util/Hashtable$Entry",
        "java/util/LinkedHashMap$Entry",
        "java/util/TreeMap$Entry",
        "java/util/WeakHashMap$Entry",
        "java/util/concurrent/ConcurrentHashMap$WriteThroughEntry",
        "java/util/concurrent/ConcurrentHashMap$MapEntry"};

    for (const std::string& map_entry_class : map_entry_classes) {
      classes[map_entry_class] =
          ToMethods({Allow("getKey"), Allow("getValue")});
    }
  }();

  [&classes]() {
    classes["java/lang/Thread"] = ToMethods({ Allow("currentThread") });

    // The get() method is not whitelisted for derived methods as the first call
    // to get() runs initialValue(), and if the user overrides initialValue()
    // then they would be able to run arbitrary code inside expressions.
    classes["java/lang/ThreadLocal"] = ToMethods({
        Allow("get").signature("()Ljava/lang/Object;")
    });
  }();

  //
  // Additional configuration provided through flags.
  //

  for (const std::string& item :
       SplitString(absl::GetFlag(FLAGS_extra_allowed_methods))) {
    const auto method = SplitMethod(item);
    VLOG(1) << "Adding block rule for class " << method.first
            << ", method " << method.second;
    classes[method.first].push_back(Allow(method.second).build());
  }

  for (const std::string& item :
       SplitString(absl::GetFlag(FLAGS_extra_blocked_methods))) {
    const auto method = SplitMethod(item);
    VLOG(1) << "Adding allow rule for class " << method.first
            << ", method " << method.second;
    classes[method.first].push_back(Block(method.second).build());
  }

  for (const std::string& cls :
       SplitString(absl::GetFlag(FLAGS_extra_whitelisted_classes))) {
    VLOG(1) << "Adding allow-all rule for class " << cls;
    classes[cls].push_back(AllowAll().build());
  }

  return classes;
}

std::unique_ptr<Config> DefaultConfig() {
  Config::Builder builder;

  if (absl::GetFlag(FLAGS_enable_safe_caller)) {
    Config::MethodCallQuota expression_method_call_quota;
    expression_method_call_quota.max_classes_load =
        absl::GetFlag(FLAGS_expression_max_classes_load_quota);
    expression_method_call_quota.max_interpreter_instructions =
        absl::GetFlag(FLAGS_expression_max_interpreter_instructions_quota);

    Config::MethodCallQuota pretty_printers_method_call_quota;
    pretty_printers_method_call_quota.max_classes_load =
        absl::GetFlag(FLAGS_pretty_printers_max_classes_load_quota);
    pretty_printers_method_call_quota.max_interpreter_instructions =
        absl::GetFlag(FLAGS_pretty_printers_max_interpreter_instructions_quota);

    Config::MethodCallQuota dynamic_log_method_call_quota;
    dynamic_log_method_call_quota.max_classes_load =
        absl::GetFlag(FLAGS_dynamic_log_max_classes_load_quota);
    dynamic_log_method_call_quota.max_interpreter_instructions =
        absl::GetFlag(FLAGS_dynamic_log_max_interpreter_instructions_quota);

    builder.SetDefaultMethodRule(InterpretAll().build());
    builder.SetQuota(
        Config::EXPRESSION_EVALUATION,
        expression_method_call_quota);
    builder.SetQuota(
        Config::PRETTY_PRINTERS,
        pretty_printers_method_call_quota);
    builder.SetQuota(
        Config::DYNAMIC_LOG,
        dynamic_log_method_call_quota);
  } else {
    builder.SetDefaultMethodRule(BlockAll().build());
  }

  for (auto it : DefaultMethodsConfig()) {
    builder.SetClassConfig("L" + it.first + ";", std::move(it.second));
  }

  return builder.Build();
}

}  // namespace cdbg
}  // namespace devtools

