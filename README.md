<!--
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
-->

<!--
    Copyright (c) 2014, Joyent, Inc.
-->

# sdc-smart-login

This repository is part of the Joyent SmartDataCenter project (SDC).  For 
contribution guidelines, issues, and general documentation, visit the main
[SDC](http://github.com/joyent/sdc) project page.

## Overview

Smartlogin is the set of components that enable SSHd on customer zones to
resolve public keys in CAPI.

The agent is installed along with other agents, the plugin is included
as part of dataset packaging.

A few key tidbites:

- The agent assumes the OS has libzdoor and libsmartsshd laid down
- The SMF service makes zones a dependent of this package

## Internals

The agent runs as a standalone daemon (note, it doesn't actually fork, it
just lets SMF do all that), and is essentially a proxy over CAPI.  It maintains
a configurable in-memory O(1) LRU cache (cache is hand rolled) for CAPI calls,
and has a "refresh" TTL (i.e., if an entry is expired we attempt to replace it,
but we don't actually evict it).  The only other interesting bit is the fact
that we have to maintain our own zone_monitor to account for zones being
provisioned/de-provisioned on the box (libzdoor monitors an existing zdoor
for reboots, but doesn't take any action in new/destroyed zones).

The package gets built into the agents shar with everything else.
