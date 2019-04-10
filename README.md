zmakebas
--------

Description
-----------

zmakebas converts a Spectrum Basic program written as a text file into
an actual speccy Basic file (as a .TAP file, or optionally a raw
headerless file).

Using zmakebas rather than (say) writing the Basic in an emulator
means you can write using a nicer editor, and can use tools which work
on text files, etc. Also, with the `-l` option you can write without
line numbers, using labels in their place where necessary.

The program was originally intended to be used simply to make little
loader programs, so they wouldn't have to be sourceless binaries.
However, I went to a fair amount of effort to make sure it'd work for
bigger, more serious programs too, so you can also use it for that
kind of thing.


Installation
------------

While I think zmakebas should be fairly portable, I've only ever
compiled it on Linux and MS-DOS. Things should be ok for Linux as-is -
just do `make` then (as root) `make install`.


Miscellaneous
-------------

See the man page for details of how to use zmakebas.

There's a short demo file showing the use of escape sequences for UDGs
and block graphics characters etc. here, named `demo.bas`. You can use
`zmakebas demo.bas` to generate an `out.tap` file from it. A version
using labels is in `demolbl.bas`, which can be converted with
`zmakebas -l demolbl.bas`.


Contacting me
-------------

You can email me at russell.marks@ntlworld.com.


Share and enjoy! 

-Rus.
