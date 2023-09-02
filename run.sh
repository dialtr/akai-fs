#!/bin/bash

ISO_PATH="./iso/akai-sample-cd.iso"

BIN_PATH="./cmake-build-debug/akaiutil"

# A simple smoke test to ensure that akaiutil is still working after refactors.

if [ ! -f "${ISO_PATH}" ]; then
  echo "Error: Expecting '${ISO_PATH}' ISO file to exist."
	exit 1
fi

if [ ! -f "${BIN_PATH}" ]; then
	echo "Error: Debug target '${BIN_PATH}' not found."
	echo "Did you forget to build the project?"
	exit 1
fi

# Run akaiutil, specifying the disc to open.
# TODO(tdial): Enhance akaiutil to run smoke tests and exit with 0 or 1
echo -e "quit\n" | "${BIN_PATH}" -f "${ISO_PATH}"

# Did we exit with zero status?
# TODO(tdial): Zero status really means nothing. Should work on task to
# make the check meaningful (outlined in TASKS.md)
#EXIT_CODE="$?"
#if [ "${EXIT_CODE}" -ne "0" ]; then
#	echo "run.sh: test failed"
#else
#	echo "run.sh: test passed"
#fi

