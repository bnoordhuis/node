#
# Regular cron jobs for the node package
#
0 4	* * *	root	[ -x /usr/bin/node_maintenance ] && /usr/bin/node_maintenance
