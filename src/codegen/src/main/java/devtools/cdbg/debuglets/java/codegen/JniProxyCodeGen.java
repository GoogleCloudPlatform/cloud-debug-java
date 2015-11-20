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

package devtools.cdbg.debuglets.java.codegen;

import static java.nio.charset.StandardCharsets.UTF_8;

import com.google.common.base.CaseFormat;
import com.google.common.collect.ImmutableMap;
import com.google.common.io.Files;
import com.google.gson.Gson;

import freemarker.template.Configuration;
import freemarker.template.DefaultObjectWrapper;
import freemarker.template.Template;
import freemarker.template.TemplateException;

import org.objectweb.asm.Type;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Utility to generate C++ code to access Java classes through JNI.
 */
public class JniProxyCodeGen {
  /**
   * Wrapper class to expose argument type or return type to code generator.
   */
  public static class ProxyType {
    private final Class<?> cls;

    private static final ImmutableMap<Class<?>, String> PRIMITIVE_TYPES;

    static {
      ImmutableMap.Builder<Class<?>, String> builder = ImmutableMap.builder();
      builder.put(void.class, "void");
      builder.put(boolean.class, "jboolean");
      builder.put(byte.class, "jbyte");
      builder.put(char.class, "jchar");
      builder.put(short.class, "jshort");
      builder.put(int.class, "jint");
      builder.put(long.class, "jlong");
      builder.put(float.class, "jfloat");
      builder.put(double.class, "jdouble");

      PRIMITIVE_TYPES = builder.build();
    }

    public ProxyType(Class<?> cls) {
      this.cls = cls;
    }

    public String getJniType() {
      String primitiveNativeType = PRIMITIVE_TYPES.get(cls);
      if (primitiveNativeType != null) {
        return primitiveNativeType;
      }

      return "jobject";
    }

    public String getNativeReturnType() {
      if (cls == void.class) {
        // We can't instantiate ExceptionOr<void>, so we use nullptr_t instead.
        return "::devtools::cdbg::ExceptionOr<std::nullptr_t>";
      }

      String primitiveNativeType = PRIMITIVE_TYPES.get(cls);
      if (primitiveNativeType != null) {
        return String.format("::devtools::cdbg::ExceptionOr<%s>", primitiveNativeType);
      }

      if ((cls == String.class) || (cls == byte[].class)) {
        return "::devtools::cdbg::ExceptionOr<string>";
      }

      return "::devtools::cdbg::ExceptionOr< ::devtools::cdbg::JniLocalRef >";
    }

    public String getNativeArgumentType() {
      String primitiveNativeType = PRIMITIVE_TYPES.get(cls);
      if (primitiveNativeType != null) {
        return primitiveNativeType;
      }

      if ((cls == String.class) || (cls == byte[].class)) {
        return "const string&";
      }

      return "jobject";
    }

    public boolean isVoid() {
      return cls == void.class;
    }

    /**
     * Gets the type name used to format the JNI call method.
     */
    public String getJniCallMethodTypeName() {
      if (PRIMITIVE_TYPES.containsKey(cls)) {
        return CaseFormat.LOWER_CAMEL.to(CaseFormat.UPPER_CAMEL, cls.getSimpleName());
      }

      return "Object";
    }

    public String getReturnValueConversion(String rc) {
      return String.format(getReturnValueConversionFormat(), rc);
    }

    String getArgumentConversionFormat() {
      if (cls == String.class) {
        return "::devtools::cdbg::JniToJavaString(%s).get()";
      }

      if (cls == byte[].class) {
        return "::devtools::cdbg::JniToByteArray(%s).get()";
      }

      return "%s";
    }

    String getReturnValueConversionFormat() {
      if (cls == void.class) {
        return "nullptr";
      }

      if (PRIMITIVE_TYPES.containsKey(cls)) {
        return "%s";
      }

      if (cls == String.class) {
        return "::devtools::cdbg::JniToNativeString(%s)";
      }

      if (cls == byte[].class) {
        return "::devtools::cdbg::JniToNativeBlob(%s)";
      }

      return "::devtools::cdbg::JniLocalRef(%s)";
    }
  }

  /**
   * Wrapper class to expose method argument to code generator.
   */
  public static class ProxyArgument {
    private final String name;
    private final ProxyType type;

    public ProxyArgument(String name, ProxyType type) {
      this.name = name;
      this.type = type;
    }

    public String getName() {
      return name;
    }

    public ProxyType getType() {
      return type;
    }

    public String getArgumentConversion() {
      return String.format(type.getArgumentConversionFormat(), name);
    }
  }

  /**
   * Wrapper class to expose constructors to code generator.
   */
  public static class ProxyConstructor {
    /**
     * Generates {@code NewObjectOf} method that allocates a subclass of this class.
     *
     * TODO(vlif): remove this option.
     */
    private final boolean subclassConstructor;

    /**
     * Proxy-enabled Java class constructor.
     */
    private final Constructor<?> constructor;

    /**
     * Index of this constructor for name disambiguation.
     */
    private final int id;

    public ProxyConstructor(boolean subclassConstructor, Constructor<?> constructor, int id) {
      this.subclassConstructor = subclassConstructor;
      this.constructor = constructor;
      this.id = id;
    }

    /**
     * Returns true if the generated method allocates a subclass of this class and not
     * necesserily instance of this class.
     */
    public boolean isSubclassConstructor() {
      return subclassConstructor;
    }


    /**
     * Gets the index of this constructor for name disambiguation.
     */
    public int getId() {
      return id;
    }

    /**
     * Gets the name of the generated method that allocates a new object.
     */
    public String getMethodName() {
      return subclassConstructor ? "NewObjectOf" : "NewObject";
    }

    /**
     * Gets the constructor declaration string.
     */
    public String getDescription() {
      return constructor.toString();
    }

    /**
     * Gets the JNI signature of the method.
     */
    public String getSignature() {
      return Type.getConstructorDescriptor(constructor);
    }

    /**
     * Gets the list of method arguments.
     */
    public List<ProxyArgument> getArguments() {
      List<ProxyArgument> arguments = new ArrayList<>();

      if (subclassConstructor) {
        arguments.add(new ProxyArgument("cls", new ProxyType(Class.class)));
      }

      arguments.addAll(getConstructorArguments());

      return arguments;
    }

    /**
     * Gets the list of Java constructor arguments.
     */
    public List<ProxyArgument> getConstructorArguments() {
      List<ProxyArgument> arguments = new ArrayList<>();
      for (Class<?> cls : constructor.getParameterTypes()) {
        arguments.add(
            new ProxyArgument(String.format("arg%d", arguments.size()), new ProxyType(cls)));
      }

      return arguments;
    }

    /**
     * Checks whether a method has at least one argument.
     */
    public boolean getHasArguments() {
      return constructor.getParameterTypes().length > 0;
    }
  }

  /**
   * Wrapper class to expose method to code generator.
   */
  public static class ProxyMethod {
    /**
     * Proxy-enabled Java method.
     */
    private final Method method;

    /**
     * Index of this method to disambiguate overloaded methods.
     */
    private final int id;

    public ProxyMethod(Method method, int id) {
      this.method = method;
      this.id = id;
    }

    /**
     * Gets the index of this method to disambiguate overloaded methods.
     */
    public int getId() {
      return id;
    }

    /**
     * Gets method name (might not be unique due to method overloading).
     */
    public String getName() {
      return method.getName();
    }

    /**
     * Gets the method declaration string.
     */
    public String getDescription() {
      return method.toString();
    }

    /**
     * Gets the JNI signature of the method.
     */
    public String getSignature() {
      return Type.getMethodDescriptor(method);
    }

    /**
     * Checks method modifiers for "static".
     */
    public boolean isStatic() {
      return Modifier.isStatic(method.getModifiers());
    }

    /**
     * Gets the return type for the method.
     */
    public ProxyType getReturnType() {
      return new ProxyType(method.getReturnType());
    }

    /**
     * Gets the list of method arguments.
     */
    public List<ProxyArgument> getArguments() {
      List<ProxyArgument> arguments = new ArrayList<>();
      for (Class<?> cls : method.getParameterTypes()) {
        arguments.add(
            new ProxyArgument(String.format("arg%d", arguments.size()), new ProxyType(cls)));
      }

      return arguments;
    }

    /**
     * Checks whether a method has at least one argument.
     */
    public boolean getHasArguments() {
      return method.getParameterTypes().length > 0;
    }
  }

  /**
   * Directory for the generated files.
   */
  private final File path;

  /**
   * Class for which the proxy is generated.
   */
  private final Class<?> cls;

  /**
   * Configuration of the code generator for this Java class.
   */
  private final Config.ProxyClass classConfig;

  /**
   * Normalized name of the class used to format the name of the generated file.
   */
  private final String fileNameClass;

  /**
   * List of constructors to proxy.
   */
  private final ArrayList<ProxyConstructor> constructors;

  /**
   * List of methods to proxy.
   */
  private final ArrayList<ProxyMethod> methods;

  /**
   * Code generator configuration.
   */
  private final Configuration codeGenConfig;

  public JniProxyCodeGen(File path, Class<?> cls, Config.ProxyClass config) {
    this.path = path;
    this.cls = cls;
    this.classConfig = config;

    fileNameClass = cls.getName()
        .replace('.', '_')
        .replace('$', '_')
        .replaceFirst("^java_lang_", "")
        .replaceFirst("^java_io_", "")
        .replaceFirst("^java_util_logging_", "jul_")
        .replaceFirst("^com_google_api_client_util_", "api_client_")
        .replaceFirst("^com_google_devtools_cdbg_debuglets_java_", "")
        .toLowerCase();

    Constructor<?>[] clsConstructors = cls.getDeclaredConstructors();
    Method[] clsMethods = cls.getDeclaredMethods();

    // Sort list of methods to ensure consistent generated code (Class returns elements
    // in no particular order).
    Comparator<Object> methodComparator = new Comparator<Object>() {
      @Override
      public int compare(Object o1, Object o2) {
        return o1.toString().compareTo(o2.toString());
      }
    };

    Arrays.sort(clsConstructors, methodComparator);
    Arrays.sort(clsMethods, methodComparator);

    constructors = new ArrayList<>();
    methods = new ArrayList<>();
    for (Config.ProxyMethod methodConfig : config.getMethods()) {
      boolean found = false;
      if (methodConfig.getMethodName().equals("<init>")) {
        for (Constructor<?> constructor : clsConstructors) {
          if ((methodConfig.getMethodSignature() != null)
              && !Type.getConstructorDescriptor(constructor).equals(
                    methodConfig.getMethodSignature())) {
            continue;
          }

          constructors.add(new ProxyConstructor(methodConfig.isSubclassConstructor(),
              constructor, constructors.size()));
          found = true;
        }
      } else {
        for (Method method : clsMethods) {
          if (!method.getName().equals(methodConfig.getMethodName())) {
            continue;
          }

          if ((methodConfig.getMethodSignature() != null)
              && !Type.getMethodDescriptor(method).equals(methodConfig.getMethodSignature())) {
            continue;
          }

          methods.add(new ProxyMethod(method, methods.size()));
          found = true;
        }
      }

      if (!found) {
        throw new RuntimeException(
            String.format("Method %s (signature: %s) not found in class %s",
                methodConfig.getMethodName(),
                (methodConfig.getMethodSignature() != null)
                    ? methodConfig.getMethodSignature()
                    : "any",
                cls.getName()));
      }
    }

    codeGenConfig = new Configuration(Configuration.VERSION_2_3_22);
    codeGenConfig.setClassForTemplateLoading(getClass(), "");
    codeGenConfig.setObjectWrapper(new DefaultObjectWrapper(Configuration.VERSION_2_3_22));
  }

  public void generate() throws IOException, TemplateException {
    generateFile("JniProxy.h.tpl", new File(path, "jni_proxy_" + fileNameClass + ".h"));
    generateFile("JniProxy.cc.tpl", new File(path, "jni_proxy_" + fileNameClass + ".cc"));
    generateFile("Mock.h.tpl", new File(path, "mock_" + fileNameClass + ".h"));
  }

  private void generateFile(String templateName, File destinationFile)
      throws IOException, TemplateException {
    Template template = codeGenConfig.getTemplate(templateName);

    FileOutputStream fileOutputStream = new FileOutputStream(destinationFile);
    Writer writer = new OutputStreamWriter(fileOutputStream);

    Map<String, Object> data = new HashMap<String, Object>();
    data.put("fileName", destinationFile.getName());
    data.put("fileNameWithoutExtension", Files.getNameWithoutExtension(destinationFile.getName()));
    data.put("fileNameClass", fileNameClass);
    data.put("namespaces", classConfig.getNativeNamespace().split("::"));
    data.put("guardFileName", destinationFile.getName().replace('.', '_').toUpperCase());
    data.put("cls", cls);
    data.put("constructors", constructors);
    data.put("methods", methods);
    data.put("classSignature", cls.getName().replace('.', '/'));
    data.put("normalizedClassName", cls.getSimpleName().replace('$', '_'));

data.put("includeDirectory", "");

    template.process(data, writer);

    writer.flush();
    writer.close();
  }

  public static void main(String[] args) throws Exception {
    // Load code generator configuration.
    String configJson = Files.asCharSource(new File(args[0]), UTF_8).read();
    Gson gson = new Gson();
    Config config = gson.fromJson(configJson, Config.class);

    // Generate proxy classes.
    for (Config.ProxyClass javaClass : config.getClasses()) {
      JniProxyCodeGen gen = new JniProxyCodeGen(
          new File(args[1]),
          Class.forName(javaClass.getClassName()),
          javaClass);
      gen.generate();
    }
  }

}
