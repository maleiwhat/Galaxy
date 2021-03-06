#!/bin/bash
## ========================================================================== ##
## Copyright (c) 2014-2018 The University of Texas at Austin.                 ##
## All rights reserved.                                                       ##
##                                                                            ##
## Licensed under the Apache License, Version 2.0 (the "License");            ##
## you may not use this file except in compliance with the License.           ##
## A copy of the License is included with this software in the file LICENSE.  ##
## If your copy does not contain the License, you may obtain a copy of the    ##
## License at:                                                                ##
##                                                                            ##
##     https://www.apache.org/licenses/LICENSE-2.0                            ##
##                                                                            ##
## Unless required by applicable law or agreed to in writing, software        ##
## distributed under the License is distributed on an "AS IS" BASIS, WITHOUT  ##
## WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.           ##
## See the License for the specific language governing permissions and        ##
## limitations under the License.                                             ##
##                                                                            ##
## ========================================================================== ##

if [ "x$1" != "x" ]; then
	echo "usage: prep-third-party.sh"
	echo "This script will init and update submodules, download ispc, and apply patches to the third-party libraries."
	exit 1
fi

function fail
{
	echo "ERROR: $1"
	exit 1
}

GXY_ROOT=$PWD
if [ ! -d ${GXY_ROOT}/third-party ]; then
	fail "Please run this script from the root directory of the Galaxy repository."
fi

echo "initializing submodules..."
git submodule --quiet init
if [ $? != 0 ]; then
	fail "submodule init returned error code $?"
fi

echo "updating submodules..."
git submodule --quiet update
if [ $? != 0 ]; then
	fail "submodule update returned error code $?"
fi

echo "checking ispc..."
cd ${GXY_ROOT}/third-party/ispc
./get-ispc.sh
if [ $? != 0 ]; then
	fail
fi

echo "applying patches..."
PATCH_DIR=${GXY_ROOT}/third-party/patches
cd ${PATCH_DIR}
for patch in *.patch ; do
	PATCH_TARGET=`echo ${patch} | sed -e 's/\.patch//'`
	if [ ! -d ${GXY_ROOT}/third-party/${PATCH_TARGET} ]; then
		fail "could not find ${PATCH_TARGET} at ${GXY_ROOT}/third-party/${PATCH_TARGET}"
	fi
	echo "  patching ${PATCH_TARGET} ..."
	cd ${GXY_ROOT}/third-party/${PATCH_TARGET}
	git apply ${PATCH_DIR}/${patch}
	if [ $? != 0 ]; then
		fail "patch application for ${PATCH_TARGET} returned error code $?"
	fi
done

echo "done!"
cd ${GXY_ROOT}
exit 0






