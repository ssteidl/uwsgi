#include "tcl_loop.h"
#include <tcl8.6/tcl.h>

void uwsgi_opt_setup_tcl_loop(char *opt, char *value, void *null) {

    // set async mode
    uwsgi_opt_set_int(opt, value, &uwsgi.async);
    if (uwsgi.socket_timeout < 30) {
            uwsgi.socket_timeout = 30;
    }
    // set loop engine
    uwsgi.loop = "tcl_loop";

    utcl.tcl_loop = 1;
}

void tcl_loop_run(struct uwsgi_server* uwsgi)
{
    if (!uwsgi->async_waiting_fd_table)
        uwsgi->async_waiting_fd_table = uwsgi_calloc(sizeof(struct wsgi_request *) * uwsgi->max_fd);
    if (!uwsgi->async_proto_fd_table)
        uwsgi->async_proto_fd_table = uwsgi_calloc(sizeof(struct wsgi_request *) * uwsgi->max_fd);

    uwsgi->schedule_to_req = async_schedule_to_req;

    //TODO: Tcl Integration

//    uwsgi->wait_read_hook

    /* Possible schedule to main implementation:
     *
     * - tcl_loop_run Creates a new tcl command TCL_NRCreateCommand, makes a coroutine of that command and then jumps into it with Tcl_NREvalObj.
     * - Loop run creates an event object.
     * - ...
     */
}
