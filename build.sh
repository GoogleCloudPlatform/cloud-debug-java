#!/bin/bash -e
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
# This script builds the Java Cloud Debugger agent from source code. The
# debugger is currently only supported on Linux.
#
# The build script assumes some dependencies.
# To install those on Debian, run this command:
# sudo apt-get -y -q --no-install-recommends install \
#     curl gcc build-essential libssl-dev unzip openjdk-8-jdk \
#     cmake python3 maven
#
# The Java Cloud Debugger agent uses glog, gflags and jsoncpp libraries.
# This script downloads and builds them first. Then it runs make to build
# the C++ and Java portions of the codebase.
#
# Home page of gflags: https://github.com/gflags/gflags
# Home page of glog: https://github.com/google/glog
# Home page of jsoncpp: http://sourceforge.net/projects/libjson/files/?source=navbar
#

GFLAGS_URL=https://github.com/gflags/gflags/archive/v2.2.2.tar.gz
GLOG_URL=https://github.com/google/glog/archive/v0.4.0.tar.gz
JSONCPP_URL=https://github.com/open-source-parsers/jsoncpp/archive/refs/tags/1.7.6.tar.gz


ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") >/dev/null; /bin/pwd -P)

# Parallelize the build over N threads where N is the number of cores * 1.5.
PARALLEL_BUILD_OPTION="-j $(($(nproc 2> /dev/null || echo 4)*3/2))"

# Clean up previously generated files
pushd "${ROOT}"/third_party
rm -rf gflags* glog* jsoncpp* install
rm -rf antlr/build
rm -f antlr/lib/cpp/src/*.o
popd

pushd "${ROOT}"/src
rm -f agent/*.class
rm -f agent/*.jar
rm -f agent/*.o
rm -f agent/*.so
rm -f agent/internals_class_loader_static_defs.inl
rm -f agent/version.txt
rm -rf agent/antlrgen
rm -rf agent/service-account-auth/target
rm -rf agent/internals/target
rm -rf agent/internals-class-loader/target
rm -f codegen/*.cc
rm -f codegen/*.h
rm -rf codegen/target
popd

rm -rf "${ROOT}"/dist

if [[ -n "${INSTALL_DEPS}" ]]; then
apt-get update
apt-get -y -q --no-install-recommends install \
    curl gcc build-essential libssl-dev unzip openjdk-8-jdk \
    cmake python3 maven
fi

# Clean up any previous build files.
rm -rf "${ROOT}"/third_party/gflags* \
       "${ROOT}"/third_party/glog* \
       "${ROOT}"/third_party/*jsoncpp* \
       "${ROOT}"/third_party/install

# Build and install gflags to third_party/.
pushd "${ROOT}"/third_party
curl -Lk "${GFLAGS_URL}" -o gflags.tar.gz
tar xzf gflags.tar.gz
cd gflags-*
mkdir build
cd build
cmake -DCMAKE_CXX_FLAGS=-fpic \
      -DGFLAGS_NAMESPACE=google \
      -DCMAKE_INSTALL_PREFIX:PATH="${ROOT}"/third_party/install \
      ..
make ${PARALLEL_BUILD_OPTION}
make install
popd

# Build and install glog to third_party/.
pushd "${ROOT}"/third_party
curl -L "${GLOG_URL}" -o glog.tar.gz
tar xzf glog.tar.gz
cd glog-*
mkdir build
cd build
cmake -DCMAKE_CXX_FLAGS=-fpic \
      -DCMAKE_PREFIX_PATH="${ROOT}"/third_party/install \
      -DCMAKE_INSTALL_PREFIX:PATH="${ROOT}"/third_party/install \
      ..
make ${PARALLEL_BUILD_OPTION}
make install
popd

# Build and install jsoncpp to third_party/.
pushd "${ROOT}"/third_party
curl -L ${JSONCPP_URL} -o jsoncpp.tar.gz
tar xzf jsoncpp.tar.gz
cd *jsoncpp-*
mkdir build
cd build
cmake -DCMAKE_CXX_FLAGS=-fpic \
      -DCMAKE_INSTALL_PREFIX:PATH="${ROOT}"/third_party/install \
      ..
make ${PARALLEL_BUILD_OPTION}
make install
popd

# Build the debugger agent.
pushd "${ROOT}"/src/agent
mkdir -p ${ROOT}/dist
make ${PARALLEL_BUILD_OPTION} \
     BUILD_TARGET_PATH="${ROOT}/dist" \
     THIRD_PARTY_LIB_PATH="${ROOT}"/third_party/install/lib \
     THIRD_PARTY_INCLUDE_PATH="${ROOT}"/third_party/install/include
popd

echo "Build artifacts are in the dist directory"
