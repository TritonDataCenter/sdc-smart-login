#!/bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

#
# Copyright (c) 2014, Joyent, Inc.
#

DIR=`dirname $0`

export PREFIX=$npm_config_prefix 
export ETC_DIR=$npm_config_etc
export SMF_DIR=$npm_config_smfdir
export VERSION=$npm_package_version
export CFG_FILE=$ETC_DIR/smartlogin.cfg

source /lib/sdc/config.sh
load_sdc_config

if [ ! -f $CFG_FILE ];
then
    echo "capi-url=$CONFIG_capi_client_url" > $CFG_FILE
    echo "capi-login=$CONFIG_capi_http_admin_user" >> $CFG_FILE
    echo "capi-pw=$CONFIG_capi_http_admin_pw" >> $CFG_FILE
    echo "capi-connect-timeout=1" >> $CFG_FILE
    echo "capi-timeout=3" >> $CFG_FILE
    echo "capi-retry-attempts=3" >> $CFG_FILE
    echo "capi-retry-sleep=1" >> $CFG_FILE
    echo "capi-recheck-denies=yes" >> $CFG_FILE
    echo "capi-cache-size=1000" >> $CFG_FILE
    echo "capi-cache-age=600" >> $CFG_FILE
fi

subfile () {
  IN=$1
  OUT=$2
  sed -e "s#@@PREFIX@@#$PREFIX#g" \
      -e "s/@@VERSION@@/$VERSION/g" \
      -e "s#@@ETC_DIR@@#$ETC_DIR#g" \
      -e "s#@@SMFDIR@@#$SMFDIR#g"   \
      $IN > $OUT
}

subfile "$DIR/../etc/smartlogin.xml.in" "$SMF_DIR/smartlogin.xml"

svccfg import $SMF_DIR/smartlogin.xml

SL_STATUS=`svcs -H smartlogin | awk '{ print $1 }'`

echo "SmartLogin status was $SL_STATUS"

# Gracefully restart the agent if it is online.
if [ "$SL_STATUS" = 'online' ]; then
  svcadm restart smartlogin
else
  svcadm enable smartlogin
fi
