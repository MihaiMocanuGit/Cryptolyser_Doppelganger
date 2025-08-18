#!/bin/bash
# unofficial bash strict mode
set -euo pipefail
IFS=$'\n\t'

project="Cryptolyser_Doppelganger"

# OpenSSL hardware acceleration flags
x86_no_hw_acc="OPENSSL_ia32cap=~0x200000200000000"
arm_no_hw_acc="OPENSSL_armcap=0X1B"
#OPENSSL_armcap=0x1B: This forces OpenSSL to disable ARMv8 crypto extensions, effectively disabling hardware acceleration on ARM (including AES instructions on a Raspberry Pi 4).
#OPENSSL_ia32cap="~0x200000200000000": Disables AES-NI on x86/x64 systems

#From OpenSSL crypto/arm_arch.h:
# # define ARMV7_NEON      (1<<0)
# # define ARMV7_TICK      (1<<1)
# # define ARMV8_AES       (1<<2)
# # define ARMV8_SHA1      (1<<3)
# # define ARMV8_SHA256    (1<<4)
# # define ARMV8_PMULL     (1<<5)
#
# We want:
# 5 4 3 2 1 0
# 0 1 1 0 1 1 -> 1B

build_type="release"
build_only=false
job_count="12"

Help()
{
   echo "Builds and runs the project."
   echo
   echo "Syntax: run.sh [-h|b|B|j] [ARGS]"
   echo "Options:"
   echo "    -h     Prints this help page."
   echo "    -b     Sets the build type (debug/release/relwithdebinfo/minsizerel)."
   echo "           DEFAULT: release"
   echo "    -B     Build only, do not run program."
   echo "    -j     Sets the thread job count."
   echo "           DEFAULT: 12"
   echo "ARGS: run program with given arguments"
}

while getopts ":hb:Bj:" option; do
   case $option in
      h) # help
         Help
         exit;;
      b) # build type
         build_type=$OPTARG;;
      B) # build only
         build_only=true;;
      j) # job count
         job_count=$OPTARG;;
     \?) # Invalid option
         echo "Error: Invalid option"
         exit;;
   esac
done
shift $(expr $OPTIND - 1 ) # jump over all parsed arguments

if ! [[ "$build_type" =~ ^(debug|relwithdebinfo|minsizerel)$ ]]
then
    build_type="release"
fi

echo "build_type = $build_type"

build_dir="$(dirname $0)/build_$build_type"
echo "build_dir = $build_dir"

echo "Setting build options"
time (cmake  -DCMAKE_BUILD_TYPE="$build_type" -S . -B "$build_dir" \
    && echo "Building $build_type" && cmake --build "$build_dir" -j "$job_count") &&

if [ "$build_only" = false ]
then
    echo "Setting cpu2 to performance mode. Requires SUDO"
    sudo sh -c 'echo performance > /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor'

    echo $x86_no_hw_acc
    echo $arm_no_hw_acc
    echo "Running on cpu2, with decreased niceness. Requires SUDO"
    echo "Program launching: $build_dir/$project" "$@"
    sudo nice -n -20 env $x86_no_hw_acc env $arm_no_hw_acc taskset 0x4 "$build_dir/$project" "$@"
fi
