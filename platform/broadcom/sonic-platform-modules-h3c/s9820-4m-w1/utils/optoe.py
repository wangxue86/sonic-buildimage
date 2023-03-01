#!/usr/bin/env python
#
# Copyright (C) 2017 Inventec, Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

"""
Usage: %(scriptName)s [options] command object

options:
    -h | --help     : this help message
    -d | --debug    : run with debug mode
    -f | --force    : ignore error during installation or clean 
command:
    install         : install drivers and generate related sysfs nodes
    clean           : uninstall drivers and remove related sysfs nodes
"""

import os
import commands
import sys, getopt
import logging
import re
import time
from collections import namedtuple

DEBUG = False
args = []
FORCE = 0
i2c_prefix = '/sys/class/i2c-adapter/i2c-0/'


if DEBUG == True:
    print sys.argv[0]
    print 'ARGV: ', sys.argv[1:]


def main():
    global DEBUG
    global args
    global FORCE


    if len(sys.argv)<2:
        show_help()

    options, args = getopt.getopt(sys.argv[1:], 'hdf', ['help',
                                                       'debug',
                                                       'force',
                                                          ])
    if DEBUG == True:
        print options
        print args
        print len(sys.argv)

    for opt, arg in options:
        if opt in ('-h', '--help'):
            show_help()
        elif opt in ('-d', '--debug'):
            DEBUG = True
            logging.basicConfig(level=logging.INFO)
        elif opt in ('-f', '--force'):
            FORCE = 1
        else:
            logging.info('no option')
    for arg in args:
        if arg == 'install':
            install()
        elif arg == 'clean':
            uninstall()
        else:
            show_help()

    return 0

def show_help():
    print __doc__ % {'scriptName' : sys.argv[0].split("/")[-1]}
    sys.exit(0)

def show_log(txt):
    if DEBUG == True:
        print "[OPTOE]"+txt
    return

def exec_cmd(cmd, show):
    logging.info('Run :'+cmd)
    status, output = commands.getstatusoutput(cmd)
    show_log (cmd +" with result:" + str(status))
    show_log ("      output:"+output)
    if status:
        logging.info('Failed :'+cmd)
        if show:
            print('Failed :'+cmd)
    return  status, output

def system_install():
    global FORCE

#
# It replaces the xcvr.ko driver with the optoe driver
#
    #optoe map to i2c-bus
    for i in range(1,129):
        status, output =exec_cmd("echo optoe1 "+str(hex(i))+" > "+i2c_prefix+"new_device", 1)
        if status:
            print output
            if FORCE == 0:
                return status
    return

def system_uninstall():
    global FORCE
#
# It replaces the xcvr.ko driver with the optoe driver
#
    #optoe map to i2c-bus
    for i in range(1,129):
        status, output =exec_cmd("echo "+str(hex(i))+" > "+i2c_prefix+"delete_device", 1)
        if status:
            print output
            if FORCE == 0:
                return status
    return


def system_ready():
    if not device_found():
        return False
    return True

def install():
    if not device_found():
        print "Installing...."
        status = system_install()
        if status:
            if FORCE == 0:
                return status
    else:
        print "Optoe i2c devices already exist! Exiting...."
    return

def uninstall():
    if not device_found():
        print "No device to uninstall! Exiting...."
    else:
        print "Uninstalling devices...."
        status = system_uninstall()
        if status:
            if FORCE == 0:
                return status
    return

def device_found():
#
# Wangdongwen: we asume that once a device node 0-0001 is found, then all nodes is there
#
    ret, log = exec_cmd("ls "+i2c_prefix+"0-00*", 0)
    return not(ret)

if __name__ == "__main__":
    main()


