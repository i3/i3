#!/bin/sh
# © 2012 Han Boetes <han@mijncomputer.nl> (see also: LICENSE)
 
YEAR=`date "+%Y"`
weblog=$(mktemp)
zcat $(find /var/log/lighttpd/build.i3wm.org -type f -name "access.log.*.gz" | sort | tail -5) > $weblog
# this will match the latest logfile, which is not yet gzipped
find /var/log/lighttpd/build.i3wm.org/log$YEAR -type f \! -name "access.log.*.gz" -exec cat '{}' \; >> $weblog
cat /var/log/lighttpd/build.i3wm.org/access.log >> $weblog
gitlog=$(mktemp)
 
# create a git output logfile. Only keep the first 6 chars of the release hash
git log -150 --pretty='        %h %s' next > $gitlog

awk '/i3-wm_.*\.deb/ {print $7}' $weblog|awk -F'/' '{print $NF}'|awk -F'_' '{print $2 }'|awk -F'-' '{print $NF}' |cut -c 2-8|sort |uniq -c | while read line; do
    set -- $line
    # $1 is the number of downloads, $2 is the release md5sum
    sed -i "/$2/s|^        |$(printf '%3i' $1) d/l |" $gitlog
done

cat $gitlog
rm $gitlog
rm $weblog
