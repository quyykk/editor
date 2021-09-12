#!/bin/bash
set -e

# Sets the application build number in Editor-Info.plist to the first argument. It should be an integer.
# Example:
# $ ./utils/set_plist_build.sh 1337

perl -0777 -p -i -e "s/(CFBundleVersion.*?)\>\d+/\$1>$1/igs" XCode/Editor-Info.plist
rm -rf XCode/Editor-Info.plist.bak
