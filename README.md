# Java Cloud Debugger Agent

Google [Cloud Debugger](https://cloud.google.com/debugger/) for Java.

## Overview

Cloud Debugger (also known as Stackdriver Debugger) lets you inspect the state
of a running cloud application, at any code location, without stopping or
slowing it down. It is not your traditional process debugger but rather an
always on, whole app debugger taking snapshots from any instance of the app.

Cloud Debugger is safe for use with production apps or during development.
The Java debugger agent adds less than 10ms to the request latency when a
debug snapshot is captured. In most cases, this is not noticeable to users.
Furthermore, the Java debugger agent does not allow modification of application
state in any way, and has close to zero impact on the app instances.

Cloud Debugger attaches to all instances of the app providing the ability to
take debug snapshots and add logpoints. A snapshot captures the call-stack and
variables from any one instance that executes the snapshot location. A logpoint
writes a formatted message to the application log whenever any instance of the
app executes the logpoint location.

The Java debugger agent is only supported on Linux at the moment. It was tested
on Debian Linux, but it should work on other distributions as well.

Cloud Debugger consists of 3 primary components:

1.  The Java debugger agent (requires Java 7 and above).
2.  Cloud Debugger service storing and managing snapshots/logpoints. Explore the
    API's using
    [APIs Explorer](https://cloud.google.com/debugger/api/reference/rest/).
3.  User interface, including a command line interface
    [`gcloud debug`](https://cloud.google.com/sdk/gcloud/reference/debug/) and a
    Web interface on
    [Google Cloud Console](https://console.cloud.google.com/debug/). See the
    [online help](https://cloud.google.com/debugger/docs/using/snapshots) on how
    to use Google Cloud Console Debug page.

## Getting Help

1.  StackOverflow: http://stackoverflow.com/questions/tagged/google-cloud-debugger
2.  Send email to: [Cloud Debugger Feedback](mailto:cdbg-feedback@google.com)
3.  Send Feedback from Google Cloud Console

## Installation

The easiest way to install the Java debugger agent for
[Google Cloud Platform](https://cloud.google.com) is to download the pre-built
package from the Internet. (the package is updated periodically):

```shell
mkdir /opt/cdbg
wget -qO- https://storage.googleapis.com/cloud-debugger/compute-java/debian-wheezy/cdbg_java_agent_gce.tar.gz | \
    tar xvz -C /opt/cdbg
```

Alternatively you can build the Java debugger agent from source code:

```shell
git clone https://github.com/GoogleCloudPlatform/cloud-debug-java.git
cd cloud-debug-java
chmod +x build.sh
./build.sh
ls cdbg_java_agent_gce.tar.gz
```

Note that the build script assumes some dependencies. To install these
dependencies, run this command:


**On Debian 8:**

```shell
sudo apt-get -y -q --no-install-recommends install \
    curl gcc build-essential libssl-dev unzip openjdk-7-jdk \
    cmake python maven
```

**On Debian 9:**

```shell
sudo apt-get -y -q --no-install-recommends install \
    curl gcc build-essential libssl-dev unzip openjdk-8-jdk \
    cmake python maven
```

### Alpine Linux

The Java agent is not regularly tested on Alpine Linux, and support will be on a
best effort basis. The [Dockerfile](alpine/Dockerfile) shows how to build a
minimal image with the agent installed.

## Setup

The Java debugger agent is a
[JVMTI](http://docs.oracle.com/javase/7/docs/technotes/guides/jvmti/)
agent that needs to be enabled when JVM starts with the `-agentpath`
[option](http://docs.oracle.com/javase/8/docs/platform/jvmti/jvmti.html#starting)
of the Java launcher. Most of the debugger options are configured through system
properties.

For example:

<pre>
java <b>-agentpath:/opt/cdbg/cdbg_java_agent.so</b> -jar ~/myapp.jar
</pre>

By default the Java debugger agent assumes that it runs on Google Cloud Platform
and obtain the credentials from the local
[metadata service](https://cloud.google.com/compute/docs/metadata). To use the
Java debugger agent outside Google Cloud Platform  requires setting up a
[service account](#service-account).

You can customize the behavior of the agent by passing arguments to it.
Multiple arguments can be passed by separating them using commas without
spaces, as follows:

<pre>
java -agentpath:/opt/cdbg/cdbg_java_agent.so<b>=--arg1=val1,--arg2=val2</b> -jar ~/myapp.jar
</pre>


### Application Servers

Java application servers usually start through a bootstrap process, and each
application server has its own way of customizing Java options.

#### Tomcat

Add this line to `/etc/default/tomcat7` or `/etc/default/tomcat8`:

```shell
JAVA_OPTS="${JAVA_OPTS} -agentpath:/opt/cdbg/cdbg_java_agent.so"
```

If you run Tomcat in a Docker container, add this line to `Dockerfile` instead:

```
ENV JAVA_OPTS -agentpath:/opt/cdbg/cdbg_java_agent.so
```

#### Jetty

Add `cdbg.ini` file to `/var/lib/jetty/start.d`:

```ini
--exec
-agentpath:/opt/cdbg/cdbg_java_agent.so
```

#### Extra Classpath

The Java debugger agent needs to be able to find the application classes when
it's running in an application server like Tomcat or Jetty. By default, it
looks for the exploded root war directory. In other words, if you deployed a
`ROOT.war` in Tomcat, the agent can find it without additional configuration.

However, if you deployed your WAR file with a different name (e.g.,
`myapp.war`), or that the exploded WAR directory is not under the default
exploded root war directory (e.g., your exploded war is under
`/opt/tomcat/webapps/myapp`), then you must
let the agent know the full path to your application's classes using the
`cdbg_extra_class_path` parameter.

```none
-agentpath:/opt/cdbg/cdbg_java_agent.so=--cdbg_extra_class_path=/opt/tomcat/webapps/myapp/WEB-INF/classes
```

You can specify multiple paths by using a `:` (colon) as the path delimiter.

```none
-agentpath:/opt/cdbg/cdbg_java_agent.so=--cdbg_extra_class_path=/opt/tomcat/webapps/myapp/WEB-INF/classes:/another/path/with/classes
```

### Naming and Versioning

Developers can run multiple applications and versions at the same time within
the same Google Cloud Platform project. You should tag each app version with
the Cloud Debugger to uniquely identify it in the Cloud Debugger user interface.

To tag the application and it's version, please add these system properties:

<pre>
-Dcom.google.cdbg.module=<i>my-app-name</i>
-Dcom.google.cdbg.version=<i>my-app-version</i>
</pre>

Use `module` to name your application (or service).
Use `version` to name the app version (e.g. build version).
The UI will display the running version as `module - version`.

### Logging

By default the Java debugger aget writes its logs to `cdbg_java_agent.INFO` file
in the default logging directory. It is possible to change the log directory
as following:

```
-agentpath:/opt/cdbg/cdbg_java_agent.so=--log_dir=/my/log/dir
```

Alternatively you can make the Java Cloud Debugger log to *stderr*:

```
-agentpath:/opt/cdbg/cdbg_java_agent.so=--logtostderr=1
```

### Service Account

To use the Java debugger agent on machines <i>not</i> hosted by Google Cloud
Platform, the agent must use a Google Cloud Platform service account credentials
to authenticate with the Cloud Debugger Service.

Use the Google Cloud Console Service Accounts
[page](https://console.cloud.google.com/iam-admin/serviceaccounts/project) to
create a credentials file for an existing or new service account. The
service account must have at least the `Stackdriver Debugger Agent` role.
If you don't have a Google Cloud Platform project, you can create one for free
on [Google Cloud Console](https://console.cloud.google.com).

Once you have the service account JSON file, deploy it alongside the Java
debugger agent.

Using the service account option requires the Java debugger agent version that
supports it. Either download the pre-packaged agent from
https://storage.googleapis.com/cloud-debugger/compute-java/debian-wheezy/cdbg_java_agent_service_account.tar.gz
or the locally built `cdbg_java_agent_service_account.tar.gz`

To use the service account credentials add these system properties:
<pre>
-Dcom.google.cdbg.auth.serviceaccount.enable=<i>true</i>
-Dcom.google.cdbg.auth.serviceaccount.jsonfile=<i>/opt/cdbg/gcp-svc.json</i>
</pre>

Alternatively, you can set the `GOOGLE_APPLICATION_CREDENTIALS` environment
variable to the JSON file path instead of adding the
`auth.serviceaccount.jsonfile` system property.

### Breakpoint Canary

This feature protects large jobs from any potential bug in the Debugger agent
which can take the entire job down when a snapshot or a logpoint is applied.

When enabled, new snapshots and logpoints are rolled out to a subset of the
application's instances (roughly 10% of the instances) first. The subset is
called the canary set, and this canary set is decided upon every time a new
snapshot or logpoint is created. This means that the canary set might be
different for each individual snapshot/logpoint. The verification takes around
40 seconds to finish. Once the verification on the canary set is finished, the
snapshot or logpoint is applied to the remaining tasks. However, sometimes a
snapshot hits before it is rolled out to the entire job, saving the need to
apply it to all.

Note that this feature can be enabled/disabled on the application at
registration time or on the snapshot/logpoint when being created. Currently, the
later part (individual snapshot/logpoint when being created) is not surfaced
through any tools yet.

Currently this feature is not enabled by default. To enable this feature, set
the following property:

<pre>
-Dcom.google.cdbg.breakpoints.enable_canary=true
</pre>

### Other JVM Languages

#### Scala

Debugging Scala applications is supported; however, expressions and conditions
conditions must be written using the Java programming language syntax.

#### Kotlin

Debugging Kotlin applications is supported; however, expressions and conditions
conditions must be written using the Java programming language syntax.

Many Kotlin-specific features can be used in conditions and expressions with
simple workarounds:

```Kotlin
// Main.kt
private fun getGreeting() {
  return "Hello world!"
}
class Main {
  companion object {
    fun welcome() {
      return getGreeting()
    }
  }
}
```

Package-level functions can be accessed by qualifying them with the name of the
file and a `Kt` suffix. For instance, the `getGreeting` function above can be
used in an expression as `MainKt.getGreeting()`

Companion object methods can be accessed by qualifying them with the `Companion`
keyword. For instance, the `welcome` function above can be used in an expression
as `Main.Companion.welcome()`
