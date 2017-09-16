import os
uwsgi_os = os.uname()[0]

NAME = 'tcl'

CFLAGS = []
LDFLAGS = []

if uwsgi_os == 'Linux':
    LIBS = ['-ltcl8.6']
else:
    LIBS = ['-ltcl86']
GCC_LIST = ['tcl']
