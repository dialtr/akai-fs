# Tasks

## Pending
* Create FUSE skeleton.

* Identify globals and also statics. Anything that that could store global
  state that would prevent the functions from being threadsafe. These will
  have to be addressed.

* Work on run.sh; it should be able to download an iso from somewhere so that
  the potentially large iso file doesn't have to be in the repo. An idea would
  be to have a bootstrap script that would fetch an iso from some site that
  hosts it so that it could remain outside the repo. (It's a 500mb file!)

* Pipe the output from run.sh to a file and then grep to see if it reported
  format errors when reading the iso. That is the only way to tell if the
  utility is working properly.

## Done

* Write new README.md
* Get original code building under CMake.
* Restructure akaituil so that its source files are in their own directories.
