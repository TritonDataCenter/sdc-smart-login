#!/bin/bash

if [[ $DO_PUBLISH == 1 ]]; then
  pfexec mkdir -p $PUBLISH_LOCATION
  pfexec cp smartlogin*.tgz $PUBLISH_LOCATION/$PKG
else
  echo scp
fi
