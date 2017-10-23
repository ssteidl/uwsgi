#ifndef TCL_LOOP_H
#define TCL_LOOP_H
#include <uwsgi.h>

void uwsgi_opt_setup_tcl_loop(char *opt, char *value, void *null);

void tcl_loop_run(struct uwsgi_server* uwsgi);

#endif // TCL_LOOP_H
