# Configuration details for the gitlab CI runners

## Build

OS: FreeBSD 12-CURRENT (will probably work with RELEASE as well)
FreeBSD HEAD svn repository in ~/data/fbsdhead. Expect it to be trashed.
Packages:
* git

## Test
OS: Any
Packages:
* git(?)
* uncrustify

## Rlease
OS: Any
Packages:
* git(?)
* doxygen
* tree