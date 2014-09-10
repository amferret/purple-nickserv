#!scons

import os

#os.environ['PKG_CONFIG_PATH']="/usr/local/lib/pkgconfig"

env = Environment(CCFLAGS="-g")
env["ENV"]["PKG_CONFIG_PATH"] = os.environ.get("PKG_CONFIG_PATH")

env.ParseConfig('pkg-config glib-2.0 purple-3 --cflags --libs')

objects = list()
objects.append(env.SharedObject("main.c"))

lib = env.SharedLibrary("purple-nickserv",objects,SHLIBPREFIX="")

userdir = env.Dir(os.path.expanduser('~') + '/.purple/plugins/')
install = env.Install(userdir,lib)
env.Alias('install',install)
