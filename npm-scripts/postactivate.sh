#!/bin/bash

DIR=`dirname $0`

export BASEDIR=$npm_config_agent_root
export MODULES=$npm_config_root
export ETC_DIR=$npm_config_etc
export SMF_DIR=$npm_config_smfdir
export VERSION=$npm_package_version
export CFG_FILE=$ETC_DIR/smartlogin.cfg

source /lib/sdc/config.sh
load_sdc_config

if [ ! -f $CFG_FILE ];
then
    echo "capi-ip=$CONFIG_capi_admin_ip" > $CFG_FILE
    echo "capi-login=$CONFIG_capi_http_admin_user" >> $CFG_FILE
    echo "capi-pw=$CONFIG_capi_http_admin_pw" >> $CFG_FILE
    # Set a default TTL of 10m for 1k entries
    echo "capi-cache-size=1000" >> $CFG_FILE
    echo "capi-cache-age=600" >> $CFG_FILE
fi

subfile () {
  IN=$1
  OUT=$2
  sed -e "s#@@BASEDIR@@#$BASEDIR#g" \
      -e "s/@@VERSION@@/$VERSION/g" \
      -e "s#@@MODULES@@#$MODULES#g" \
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
