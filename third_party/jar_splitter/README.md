# Jar Splitter

The App Engine Standard Java 8 environment limits deployed files to a maximum
size of 32M. The java agent contains a jar file of size ~52M. As a result the
agent build uses this jar splitter tool to ensure the provided agent can be
successfully deployed.

## The Code

The following files were copied from
https://github.com/GoogleCloudPlatform/appengine-java-standard, commit ID
6b724ba9e2a181ba55e07a9daf28105dc11be16d:

* [JarSplitter.java][jar-splitter]
* [JarMaker.java][jar-maker]

The file `JarSplitterMain.java` was added to wrap and invoke the functionality
from [JarSplitter.java][jar-splitter].

## Why not rely on jar-splitting from staging tools

The configuration supports a `enable-jar-splitting` option (see
[appengine-web.xml][staging-elements]). However this only applies to jar files
found under `WEB-INF/lib`. Relying on this would limit where users can place the
agent in their deployments. In addition, placing the agent somewhere under
`WEB-INF/lib` is dangerous and should be avoided, as it will then be searched by
the jvm when loading classes as part of running the main application. This can
lead to conflicts etc, the agent's jar files must be kept separate from the
running application.

[jar-splitter]: https://github.com/GoogleCloudPlatform/appengine-java-standard/blob/6b724ba9e2a181ba55e07a9daf28105dc11be16d/api_dev/src/main/java/com/google/appengine/tools/util/JarSplitter.java
[jar-maker]: https://github.com/GoogleCloudPlatform/appengine-java-standard/blob/6b724ba9e2a181ba55e07a9daf28105dc11be16d/api_dev/src/main/java/com/google/appengine/tools/util/JarMaker.java
[staging-elements]: https://cloud.google.com/appengine/docs/legacy/standard/java/config/appref#staging_elements
