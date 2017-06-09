#! /bin/sh

# @file ralf.init.d
#
# Copyright (C) Metaswitch Networks 2017
# If license terms are provided to you in a COPYING file in the root directory
# of the source code repository by which you are accessing this code, then
# the license outlined in that COPYING file applies to your use.
# Otherwise no rights are granted except for those provided to you by
# Metaswitch Networks in a separate written agreement.

### BEGIN INIT INFO
# Provides:          ralf
# Required-Start:    $remote_fs $syslog clearwater-infrastructure
# Required-Stop:     $remote_fs $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Clearwater Ralf
# Description:       Clearwater CTF Ralf
### END INIT INFO

# Author: Mike Evans <mike.evans@metaswitch.com>
#
# Please remove the "Author" lines above and replace them
# with your own name if you copy and modify this script.

# Do NOT "set -e"

# PATH should only include /usr/* if it runs after the mountnfs.sh script
PATH=/sbin:/usr/sbin:/bin:/usr/bin
DESC="Clearwater CTF"
NAME=ralf
EXECNAME=ralf
PIDFILE=/var/run/$NAME/$NAME.pid
DAEMON=/usr/share/clearwater/bin/ralf
HOME=/etc/clearwater
log_directory=/var/log/$NAME

# Exit if the package is not installed
[ -x "$DAEMON" ] || exit 0

# Read configuration variable file if it is present
#[ -r /etc/default/$NAME ] && . /etc/default/$NAME

# Load the VERBOSE setting and other rcS variables
. /lib/init/vars.sh

# Define LSB log_* functions.
# Depend on lsb-base (>= 3.2-14) to ensure that this file is present
# and status_of_proc is working.
. /lib/lsb/init-functions

#
# Function to set up environment
#
setup_environment()
{
        export LD_LIBRARY_PATH=/usr/share/clearwater/ralf/lib
        ulimit -Hn 1000000
        ulimit -Sn 1000000
        ulimit -c unlimited
        # enable gdb to dump a parent ralf process's stack
        echo 0 > /proc/sys/kernel/yama/ptrace_scope
}

#
# Function to pull in settings prior to starting the daemon
#
get_settings()
{
        # Set up defaults and then pull in the settings for this node.
        sas_server=0.0.0.0
        signaling_dns_server=127.0.0.1
        num_http_threads=$(($(grep processor /proc/cpuinfo | wc -l) * 50))
        log_level=2
        . /etc/clearwater/config

        # Work out which features are enabled.
        if [ -d /etc/clearwater/features.d ]
        then
          for file in $(find /etc/clearwater/features.d -type f)
          do
            [ -r $file ] && . $file
          done
        fi
}

#
# Function to get the arguments to pass to the process
#
get_daemon_args()
{
        # Get the settings
        get_settings

        # Set the destination realm correctly
        if [ ! -z $billing_realm ]
        then
          billing_realm_arg="--billing-realm=$billing_realm"
        elif [ ! -z $home_domain ]
        then
          billing_realm_arg="--billing-realm=$home_domain"
        fi

        [ "$sas_use_signaling_interface" != "Y" ] || sas_signaling_if_arg="--sas-use-signaling-interface"

        [ -z "$target_latency_us" ] || target_latency_us_arg="--target-latency-us=$target_latency_us"
        [ -z "$max_tokens" ] || max_tokens_arg="--max-tokens=$max_tokens"
        [ -z "$init_token_rate" ] || init_token_rate_arg="--init-token-rate=$init_token_rate"
        [ -z "$min_token_rate" ] || min_token_rate_arg="--min-token-rate=$min_token_rate"
        [ -z "$exception_max_ttl" ] || exception_max_ttl_arg="--exception-max-ttl=$exception_max_ttl"
        [ -z "$cdf_identity" ] || billing_peer_arg="--billing-peer=$cdf_identity"
        [ -z $signaling_namespace ] || namespace_prefix="ip netns exec $signaling_namespace"
        [ -z "$local_site_name" ] || local_site_name_arg="--local-site-name=$local_site_name"
        [ -z "$chronos_hostname" ] || chronos_hostname_arg="--chronos-hostname=$chronos_hostname"
        [ -z "$ralf_chronos_callback_uri" ] || ralf_chronos_callback_uri_arg="--ralf-chronos-callback-uri=$ralf_chronos_callback_uri"
        [ -z "$ralf_hostname" ] || ralf_hostname_arg="--ralf-hostname=$ralf_hostname"
        [ -z "$http_acr_logging" ] || http_acr_logging_arg="--http-acr-logging"

        DAEMON_ARGS="--localhost=$local_ip
                     $local_site_name_arg
                     --http=$local_ip
                     --http-threads=$num_http_threads
                     --session-stores=$ralf_session_store
                     --access-log=$log_directory
                     --dns-servers=$signaling_dns_server
                     --log-file=$log_directory
                     --log-level=$log_level
                     $chronos_hostname_arg
                     $ralf_chronos_callback_uri_arg
                     $ralf_hostname_arg
                     $http_acr_logging_arg
                     $billing_realm_arg
                     $billing_peer_arg
                     $target_latency_us_arg
                     $max_tokens_arg
                     $init_token_rate_arg
                     $min_token_rate_arg
                     $exception_max_ttl_arg
                     $sas_signaling_if_arg
                     --sas=$sas_server,$NAME@$public_hostname"

        [ "$http_blacklist_duration" = "" ]     || DAEMON_ARGS="$DAEMON_ARGS --http-blacklist-duration=$http_blacklist_duration"
        [ "$diameter_blacklist_duration" = "" ] || DAEMON_ARGS="$DAEMON_ARGS --diameter-blacklist-duration=$diameter_blacklist_duration"
        [ "$astaire_blacklist_duration" = "" ]  || DAEMON_ARGS="$DAEMON_ARGS --astaire-blacklist-duration=$astaire_blacklist_duration"
        [ "$dns_timeout" = "" ]                 || DAEMON_ARGS="$DAEMON_ARGS --dns-timeout=$dns_timeout"
}

#
# Function that starts the daemon/service
#
do_start()
{
        # Return
        #   0 if daemon has been started
        #   1 if daemon was already running
        #   2 if daemon could not be started

        # Allow us to write to the pidfile directory
        install -m 755 -o $NAME -g root -d /var/run/$NAME && chown -R $NAME /var/run/$NAME

        start-stop-daemon --start --quiet --pidfile $PIDFILE --exec $DAEMON --test > /dev/null \
                || return 1

        # daemon is not running, so attempt to start it.
        setup_environment
        get_daemon_args
        $namespace_prefix start-stop-daemon --start --quiet --pidfile $PIDFILE --exec $DAEMON --chuid $NAME --chdir $HOME -- $DAEMON_ARGS --daemon --pidfile=$PIDFILE \
                || return 2
        # Add code here, if necessary, that waits for the process to be ready
        # to handle requests from services started subsequently which depend
        # on this one.  As a last resort, sleep for some time.
}

#
# Function that stops the daemon/service
#
do_stop()
{
        # Return
        #   0 if daemon has been stopped
        #   1 if daemon was already stopped
        #   2 if daemon could not be stopped
        #   other if a failure occurred
        start-stop-daemon --stop --quiet --retry=TERM/30/KILL/5 --pidfile $PIDFILE --name $EXECNAME
        RETVAL="$?"
        return "$RETVAL"
}

#
# Function that runs the daemon/service in the foreground
#
do_run()
{
        # Allow us to write to the pidfile directory
        install -m 755 -o $NAME -g root -d /var/run/$NAME && chown -R $NAME /var/run/$NAME

        setup_environment
        get_daemon_args
        $namespace_prefix start-stop-daemon --start --quiet --pidfile $PIDFILE --exec $DAEMON --chuid $NAME --chdir $HOME -- $DAEMON_ARGS --pidfile=$PIDFILE \
                || return 2
}

#
# Function that aborts the daemon/service
#
# This is very similar to do_stop except it sends SIGABRT to dump a core file
# and waits longer for it to complete.
#
do_abort()
{
        # Return
        #   0 if daemon has been stopped
        #   1 if daemon was already stopped
        #   2 if daemon could not be stopped
        #   other if a failure occurred
        start-stop-daemon --stop --quiet --retry=ABRT/60/KILL/5 --pidfile $PIDFILE --name $EXECNAME
        RETVAL="$?"
        # If the abort failed, it may be because the PID in PIDFILE doesn't match the right process
        # In this window condition, we may not recover, so remove the PIDFILE to get it running
        if [ $RETVAL != 0 ]; then
          rm -f $PIDFILE
        fi
        return "$RETVAL"
}

#
# Function that sends a SIGHUP to the daemon/service
#
do_reload() {
        #
        # If the daemon can reload its configuration without
        # restarting (for example, when it is sent a SIGHUP),
        # then implement that here.
        #
        start-stop-daemon --stop --signal 1 --quiet --pidfile $PIDFILE --name $EXECNAME
        return 0
}

# There should only be at most one ralf process, and it should be the one in /var/run/ralf.pid.
# Sanity check this, and kill and log any leaked ones.
if [ -f $PIDFILE ] ; then
  leaked_pids=$(pgrep -f "^$DAEMON" | grep -v $(cat $PIDFILE))
else
  leaked_pids=$(pgrep -f "^$DAEMON")
fi
if [ -n "$leaked_pids" ] ; then
  for pid in $leaked_pids ; do
    logger -p daemon.error -t $NAME Found leaked ralf $pid \(correct is $(cat $PIDFILE)\) - killing $pid
    kill -9 $pid
  done
fi

case "$1" in
  start)
        [ "$VERBOSE" != no ] && log_daemon_msg "Starting $DESC" "$NAME"
        do_start
        case "$?" in
                0|1) [ "$VERBOSE" != no ] && log_end_msg 0 ;;
                2) [ "$VERBOSE" != no ] && log_end_msg 1 ;;
        esac
        ;;
  stop)
        [ "$VERBOSE" != no ] && log_daemon_msg "Stopping $DESC" "$NAME"
        do_stop
        case "$?" in
                0|1) [ "$VERBOSE" != no ] && log_end_msg 0 ;;
                2) [ "$VERBOSE" != no ] && log_end_msg 1 ;;
        esac
        ;;
  run)
        [ "$VERBOSE" != no ] && log_daemon_msg "Running $DESC" "$NAME"
        do_run
        case "$?" in
                0|1) [ "$VERBOSE" != no ] && log_end_msg 0 ;;
                2) [ "$VERBOSE" != no ] && log_end_msg 1 ;;
        esac
        ;;
  status)
        status_of_proc "$DAEMON" "$NAME" && exit 0 || exit $?
        ;;
  reload|force-reload)
        #
        # If do_reload() is not implemented then leave this commented out
        # and leave 'force-reload' as an alias for 'restart'.
        #
        do_reload
        ;;
  restart)
        #
        # If the "reload" option is implemented then remove the
        # 'force-reload' alias
        #
        log_daemon_msg "Restarting $DESC" "$NAME"
        do_stop
        case "$?" in
          0|1)
                do_start
                case "$?" in
                        0) log_end_msg 0 ;;
                        1) log_end_msg 1 ;; # Old process is still running
                        *) log_end_msg 1 ;; # Failed to start
                esac
                ;;
          *)
                # Failed to stop
                log_end_msg 1
                ;;
        esac
        ;;
  abort)
        log_daemon_msg "Aborting $DESC" "$NAME"
        do_abort
        ;;
  abort-restart)
        log_daemon_msg "Abort-Restarting $DESC" "$NAME"
        do_abort
        case "$?" in
          0|1)
                do_start
                case "$?" in
                        0) log_end_msg 0 ;;
                        1) log_end_msg 1 ;; # Old process is still running
                        *) log_end_msg 1 ;; # Failed to start
                esac
                ;;
          *)
                # Failed to stop
                log_end_msg 1
                ;;
        esac
        ;;
  *)
        echo "Usage: $SCRIPTNAME {start|stop|run|status|reload|force-reload|restart|abort|abort-restart}" >&2
        exit 3
        ;;
esac

:
