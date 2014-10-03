#!scons

import os

os.environ['PKG_CONFIG_PATH']="/usr/opt/pidgin/lib/pkgconfig"

env = Environment(CCFLAGS="-g")
env["ENV"]["PKG_CONFIG_PATH"] = os.environ.get("PKG_CONFIG_PATH")

env.ParseConfig('pkg-config glib-2.0 purple --cflags --libs')

objects = [env.SharedObject(a) for a in (
        "main.c",
        "pat.c")]

lib = env.SharedLibrary("purple-nickserv",objects,SHLIBPREFIX="")

userdir = env.Dir(os.path.expanduser('~') + '/.purple/plugins/')
install = env.Install(userdir,lib)
env.Alias('install',install)
