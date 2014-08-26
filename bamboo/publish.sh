#!/bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

#
# Copyright (c) 2014, Joyent, Inc.
#

if [[ $DO_PUBLISH == 1 ]]; then
  pfexec mkdir -p $PUBLISH_LOCATION
  pfexec cp smart-login.tgz $PUBLISH_LOCATION/$PKG
else
  echo scp
fi
