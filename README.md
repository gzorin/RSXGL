# RSXGL - The RSX Graphics Library

This library implements parts of the OpenGL 3.1 core profile specification for
the PlayStation 3's RSX GPU. It's suitable for use in programs that
have exclusive access to the RSX, such as GameOS software (and is likely
unsuitable for implementing a multitasking desktop, as the library
doesn't arbitrate access to the RSX).

Please see the STATUS file for up-to-date information about the
current capabilities of this library.

## Installing

RSXGL uses the GNU autotools for its build system and is distributed
with a configure script. It requires the following projects:

* [ps3toolchain](http://github.com/ps3dev/ps3toolchain)
* [PSL1GHT](http://github.com/ps3dev/PSL1GHT)

RSXGL incorporates parts of the Mesa project, primarily to provide
runtime compilation of GLSL programs. Suitable versions of Mesa and
libdrm are included with RSXGL. python 2.6 with the libxml2 module is
required by Mesa's build process (specifically for building GLSL's
builtin functions). To tell the build system to use a particular python
executable, other than the default, set the PYTHON environment variable:

```
export PYTHON=/path/to/python-2.6
```

The RSXGL library depends upon a toolchain that can generate binaries for the
PS3's PPU, and also upon parts of the PSL1GHT SDK. The sample programs also
require a few ported libraries, such as libpng, which are provided by
the ps3toolchain project. ps3toolchain recommends setting two
environment variables to locate these dependencies:

```
export PS3DEV=/usr/local/ps3dev
export PSL1GHT=$PS3DEV
```

RSXGL's configure script will use the above environment variables if
they're set;  if they aren't set, by default the script uses the above
settings.

RSXGL comes with an `autogen.sh` script that should be used to generate 
and run the project's `configure` script. From the top-level source directory:

```
./autogen.sh
```

You can just generate the configure script, without actually configuring
the build (useful if you want to build in a directory separate from the source):

```
NOCONFIGURE=1 ./autogen.sh
```

```
./configure
make
make install
```

The build system creates libraries intended to be run on the PS3; it
also creates some utilities (such as a shading program assembler
derived from PSL1GHT's cgcomp) that are meant to run on the build
system. By default, these products are installed under $PS3DEV/ppu
and $PS3DEV, respectively. You can direct the build system to place
them elsewhere:

```
./configure --with-ppu-prefix=/path/to/rsxgl --prefix=/path/to/ps3dev
```

If the ported libraries, such as libpng and zlib, have been installed
someplace other than ${PS3DEV}/portlibs/ppu, you can set an
environment variable to find them:

```
./configure ppu_portlibs_PKG_CONFIG_PATH=/path/to/portlibs/lib/pkgconfig
```

Pass the "--help" option to configure to see many other build system options.

## Sample programs

Currently two sample programs are built:

* src/samples/rsxgltest - A very simple test program whose contents and
behavior will vary. This program is mainly used to try out various
features of the library as they are developed.

* src/samples/rsxglgears - A port of an old chestnut, the "glgears"
program that uses OpenGL to render some spinning gears. This port is
based upon a version included in the Mesa library, which was itself a
port to OpenGL ES 2 after being handed down throughout the ages.

Sample programs are packaged into NPDRM .pkg files, but those packages
remain in their build locations; they don't get moved anywhere
relative to RSXGL's install path by "make install".

The sample can print debugging information over TCP, in the manner of
PSL1GHT's network/debugtest sample. You can pass the the IP address of
your host system to RSXGL's configure:

```
./configure RSXGL_CONFIG_samples_host_port=192.168.1.1 RSXGL_CONFIG_samples_port=9100
```

Before starting the application on the PS3, use this command to
receive debugging output:

```
nc -l 9100
```

If you don't want to build the samples at all:

```
./configure --disable-samples
```

More complex samples are available in [a separate project](http://github.com/gzorin/rsxgl-samples).
