# akai-fs

## About

The **akai-fs** project aims to provide a FUSE filesystem to allow users to
mount disks using proprietary Akai sampler formats. It builds upon an
existing open source utility called **akaiutils** originally written by
Michael Indlekofer.

The original **akaiutils** program is written in C and was intended as an
interactive, command-line program. Since **akai-fs** will be a FUSE daemon
and will be written in (mostly) modern C++, some significant refactoring of
the code will take place. The original snapshot of **akaiutils** is
provided in the *archive/* directory for reference, and I will make an attempt
to document the link between the new implementation and the original work
where practical.

## Project Structure

While still in the alpha stage, my plan is to refactor relevant parts of
**akaiutils** into a reusable library and then wrap it with a FUSE daemon.
This approach will ultimately (hopefully) allow others to reuse the
library for interesting projects.

## Building

This project is being developed with CMake.  Build instructions for
supported platforms will follow when the project is further long.

## Status

The **akai-fs** prokect is currently in ALPHA.

## Acknowlegements

As mentioned above, **akai-fs** stands on the shoulders of some incredible
reverse engineering work by Michael Indlekofer, the author of **akaiutils**.

## Contacts
Tom Dial <thomas.r.dial@gmail.com>

