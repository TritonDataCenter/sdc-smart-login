#!/bin/bash

export SMFDIR=$npm_config_smfdir

if svcs smartlogin; then
  svcadm disable -s smartlogin
  svccfg delete smartlogin
fi

rm -f "$SMFDIR/smartlogin.xml"
