#!/bin/sh
# vim:ts=4:sw=4:expandtab
# © 2012 Michael Stapelberg, Public Domain

# This script is a trivial shell script to send your own output to i3bar while
# using the JSON protocol.
#
# It is ugly and that is inherent to using JSON with shell scripts. You
# _really_ should not do that. See i3status or i3status’s contrib/ directory
# for examples of how to handle the output in higher-level languages.
#
# This example is purely for illustration of the protocol. DO NOT USE IT IN THE
# REAL WORLD.

# Send the header so that i3bar knows we want to use JSON:
echo '{ "version": 1 }'

# Begin the endless array.
echo '['

# We send an empty first array of blocks to make the loop simpler:
echo '[]'

# Now send blocks with information forever:
while :;
do
	echo ",[{\"name\":\"time\",\"full_text\":\"$(date)\"}]"
	sleep 1
done
