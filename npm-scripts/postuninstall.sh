#!/bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

#
# Copyright 2015 Joyent, Inc.
#

if [[ "${SDC_AGENT_SKIP_LIFECYCLE:-no}" = "yes" ]]; then
    printf 'Running during package build; skipping lifecycle script.\n' >&2
    exit 0
fi

export SMFDIR=$npm_config_smfdir

if svcs smartlogin; then
  svcadm disable -s smartlogin
  svccfg delete smartlogin
fi

rm -f "$SMFDIR/smartlogin.xml"
