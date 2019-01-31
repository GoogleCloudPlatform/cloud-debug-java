#!/bin/bash
#
# Copyright 2015 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

#
# Formats Java "-agentpath:..." command line option to enable Java Cloud
# Debugger in AppEngine VM runtime environment. This script has to be colocated
# with the rest of the Java Cloud Debugger binaries.
#
# The script assumes that AppEngine specific environment variables are set.
#
# Usage example:
#     java $( /opt/cdbg/format-env-appengine-vm.sh ) -cp ...
#

if [[ -n "${CDBG_DISABLE}" ]]; then
  >&2 echo "CDBG_DISABLE is set, Java Cloud Debugger will not be loaded"
  exit
fi

CDBG_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

ARGS="-agentpath:${CDBG_ROOT}/cdbg_java_agent.so="
ARGS+="--log_dir=/var/log/app_engine"
ARGS+=",--alsologtostderr=true"

# When using Jetty/Tomcat images, the debugger should also read the
# WEB-INF/classes and WEB-INF/lib directories. When using the OpenJDK
# image (which deploys a JAR), this is not necessary.
if [[ -n "${RUNTIME_DIR}" && -z "${CDBG_APP_WEB_INF_DIR}" ]]; then
  CDBG_APP_WEB_INF_DIR="${RUNTIME_DIR}/webapps/root/WEB-INF"
fi
if [[ -n "${CDBG_APP_WEB_INF_DIR}" ]]; then
  ARGS+=",--cdbg_extra_class_path=${CDBG_APP_WEB_INF_DIR}/classes:${CDBG_APP_WEB_INF_DIR}/lib"
fi

echo "${ARGS}"
