# Java Cloud Debugger

Google [Cloud Debugger](https://cloud.google.com/tools/cloud-debugger/) for
Java.

## Overview

The Cloud Debugger lets you inspect the state of a Java application at any code
location without stopping or slowing it down. The debugger makes it easier to
view the application state without adding logging statements.

You can use the Cloud Debugger on both production and staging instances of your
application. The debugger adds less than 10ms to the request latency only when
the application state is captured. In most cases, this is not noticeable by
users. The Cloud Debugger gives a read-only experience. Application variables
can't be changed through the debugger.

The Cloud Debugger attaches to all instances of the application. The call stack
and the variables come from the first instance to take the snapshot.

The Java Cloud Debugger is only supported on Linux at the moment. It was tested
on Debian Linux, but it should work on other distributions as well.

The Cloud Debugger consists of 3 primary components:

1.  The debugger agent (requires Java 7 and above).
2.  Cloud Debugger backend that stores the list of snapshots for each
    application. You can explore the API using the
    [APIs Explorer](https://developers.google.com/apis-explorer/#p/clouddebugger/v2/).
3.  User interface for the debugger implemented using the Cloud Debugger API.
    Currently the only option for Java is the
    [Google Developers Console](https://console.developers.google.com). The
    UI requires that the source code is submitted to
    [Google Cloud Repo](https://cloud.google.com/tools/repo/cloud-repositories/).
    More options (including browsing local source files) are coming soon.

This document only focuses on the Java debugger agent. Please see the
this [page](https://cloud.google.com/tools/cloud-debugger/debugging) for
explanation how to debug an application with the Cloud Debugger.

## Options for Getting Help

1.  StackOverflow: http://stackoverflow.com/questions/tagged/google-cloud-debugger
2.  Google Group: cdbg-feedback@google.com

## Installation

The easiest way to install the debugger agent is to download the pre-built
package from the Internet:

```shell
mkdir /opt/cdbg
wget -qO- https://storage.googleapis.com/cloud-debugger/compute-java/debian-wheezy/cdbg_java_agent_gce.tar.gz | \
    tar xvz -C /opt/cdbg
```

Alternatively you can build the debugger agent from source code:

```shell
git clone https://github.com/GoogleCloudPlatform/cloud-debug-java.git
cd cloud-debug-java
chmod +x build.sh
./build.sh
```

Note that the build script assumes some dependencies. To install these
dependencies on Debian, run this command:

```
sudo apt-get -y -q --no-install-recommends install \
    curl gcc build-essential libssl-dev unzip openjdk-7-jdk \
    cmake python maven
```

## Setup

The Java Cloud Debugger agent is a
[JVMTI](http://docs.oracle.com/javase/7/docs/technotes/guides/jvmti/)
agent that needs to be enabled when JVM starts. The agent is enabled by
using `-agentpath`
[option](http://docs.oracle.com/javase/8/docs/platform/jvmti/jvmti.html#starting)
with Java launcher. Most debugger options are configured through system
properties.

For example:

<pre>
java <b>-agentpath:/opt/cdbg/cdbg_java_agent.so</b> -jar ~/myapp.jar
</pre>

By default the debugger agent assumes that it runs on Google Compute Engine and
uses local [metadata service](https://cloud.google.com/compute/docs/metadata) to
obtain the credentials. You can still use the Java Cloud Debugger outside of
Google Compute Engine or on a virtual machine that does not allow API access to
all Google Cloud services. This would require setting up a
[service account](#service-account).

### Web Servers

Java web servers usually start through a bootstrap process, and each web server
has its own way of customizing Java options.

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

### Naming and Versioning

Developers can run multiple applications and versions at the same time within
the same project. You should tag each app version with the Cloud Debugger to
uniquely identify it in the Cloud Debugger UI.

To tag the application and it's version, please add these system properties:

<pre>
-Dcom.google.cdbg.module=<i>mymodule</i>
-Dcom.google.cdbg.version=<i>myversion</i>
</pre>

Use 'module' to name your application (or service).
Use 'version' to name the app version (e.g. build id).
The UI will display the running version as 'module - version'.

### Logging

By default the Java Cloud Debugger write its logs to `cdbg_java_agent.INFO` file
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

Service account authentication lets you run the debugger agent on any Linux
machine, including outside of [Google Cloud Platform](https://cloud.google.com).
The debugger agent authenticates against the backend with the service account
created in [Google Developers Console](https://console.developers.google.com).
If your application runs on Google Compute Engine,
[metadata service authentication](#setup) is an easier option.

The first step for this setup is to create the service account in JSON format.
Please see
[OAuth](https://cloud.google.com/storage/docs/authentication?hl=en#generating-a-private-key)
page for detailed instructions. If you don't have a Google Cloud Platform
project, you can create one for free on
[Google Developers Console](https://console.developers.google.com).

Once you have the service account, please note the service account e-mail,
[project ID and project number](https://developers.google.com/console/help/new/#projectnumber).
Then copy the .json file to all the machines that run your application.

You will need to install the debugger agent that supports the service account.
The URL is: https://storage.googleapis.com/cloud-debugger/compute-java/debian-wheezy/cdbg_java_agent_service_account.tar.gz.


To enable the service account authentication add these arguments to Java
launcher command (same way as with `-agentpath`):

<pre>
-Dcom.google.cdbg.auth.serviceaccount.enable=<i>true</i>
-Dcom.google.cdbg.auth.serviceaccount.projectid=<i>myprojectid</i>
-Dcom.google.cdbg.auth.serviceaccount.projectnumber=<i>123456789</i>
-Dcom.google.cdbg.auth.serviceaccount.email=<i>email@something.com</i>
-Dcom.google.cdbg.auth.serviceaccount.jsonfile=<i>/opt/cdbg/svc.json</i>
</pre>

You can set the `GOOGLE_APPLICATION_CREDENTIALS` environment variable
to the JSON file instead of setting the `auth.serviceaccount.jsonfile` argument.
