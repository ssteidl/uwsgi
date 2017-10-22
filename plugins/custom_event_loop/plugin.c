#include <uwsgi.h>
#include <sys/event.h>
extern struct uwsgi_server uwsgi;

struct uwsgi_cel {
    int kq_fd;
} ucel;


#define free_req_queue uwsgi.async_queue_unused_ptr++; uwsgi.async_queue_unused[uwsgi.async_queue_unused_ptr] = wsgi_req

static void process_read_event(struct wsgi_request* wsgi_req)
{
    uwsgi_log("Processing read event\n");

    int status = wsgi_req->socket->proto(wsgi_req);

    uwsgi_log("Status: %d\n", status);
    if(status > 0)
    {
        return;
    }

    if(status == 0)
    {
        uwsgi_log("Calling request\n");
        uwsgi.async_proto_fd_table[wsgi_req->fd] = NULL;

        uwsgi_log("addr: %ul", uwsgi.schedule_to_req);
        uwsgi.schedule_to_req();
        return;
    }
}

static void process_write_event(struct wsgi_request* wsgi_req)
{
    uwsgi_log("Processing write event\n");


}

static void cel_loop() {


    if (!uwsgi.async_waiting_fd_table)
            uwsgi.async_waiting_fd_table = uwsgi_calloc(sizeof(struct wsgi_request *) * uwsgi.max_fd);
    if (!uwsgi.async_proto_fd_table)
            uwsgi.async_proto_fd_table = uwsgi_calloc(sizeof(struct wsgi_request *) * uwsgi.max_fd);

    uwsgi_log("Max fds: %d\n", uwsgi.max_fd);


    uwsgi.schedule_to_req = async_schedule_to_req;

    struct uwsgi_socket *uwsgi_sock = uwsgi.sockets;
    while(uwsgi_sock) {

        struct kevent event;
        EV_SET(&event, uwsgi_sock->fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
        int events = kevent(ucel.kq_fd, &event, 1, NULL, 0, NULL);
        if(events == -1)
        {
            uwsgi_log("Adding accept read filter on %d: %s\n", uwsgi_sock->fd, strerror(errno));
            exit(1);
        }

        uwsgi_sock = uwsgi_sock->next;
    }

    for(;;)
    {
        struct kevent event;
        int event_count = kevent(ucel.kq_fd, NULL, 0, &event, 1, NULL);
        if(event_count == -1)
        {
            uwsgi_log("kevent: %s\n", strerror(errno));
            exit(1);
        }

        if(event.udata != NULL)
        {
            process_read_event(event.udata);
            continue;
        }

        struct wsgi_request* wsgi_req = find_first_available_wsgi_req();

        if(wsgi_req == NULL)
        {
            uwsgi_async_queue_is_full(uwsgi_now());
            uwsgi_log("Queue is full\n");
            exit(1);
        }

        uwsgi.wsgi_req = wsgi_req;
        int fd = (int)event.ident;
        // TODO better to move it to a function api ...
        struct uwsgi_socket *uwsgi_sock = uwsgi.sockets;
        while(uwsgi_sock) {
                if (uwsgi_sock->fd == fd) break;
                uwsgi_sock = uwsgi_sock->next;
        }

        if(!uwsgi_sock)
        {
            free_req_queue;
            uwsgi_log("Could not find fd!!\n");
            exit(1);
        }

        uwsgi_log("Setting up request structure\n");
        wsgi_req_setup(wsgi_req, wsgi_req->async_id, uwsgi_sock);

        // mark core as used
        uwsgi.workers[uwsgi.mywid].cores[wsgi_req->async_id].in_request = 1;

        if (wsgi_req_simple_accept(wsgi_req, uwsgi_sock->fd)) {
            // in case of errors (or thundering herd, just reset it)
            uwsgi.workers[uwsgi.mywid].cores[wsgi_req->async_id].in_request = 0;
            free_req_queue;
            uwsgi_log("Error accepting connection!!\n");
            exit(1);
        }


        wsgi_req->start_of_request = uwsgi_micros();
        wsgi_req->start_of_request_in_sec = wsgi_req->start_of_request/1000000;

        uwsgi.async_proto_fd_table[wsgi_req->fd] = wsgi_req;

        struct kevent req_event;
        EV_SET(&req_event, wsgi_req->fd, EVFILT_READ, EV_ADD, 0, 0, wsgi_req);
        kevent(ucel.kq_fd, &req_event, 1, NULL, 0, NULL);
    }
}

static void uwsgi_opt_setup_cel(char *opt, char *value, void *null) {

        // set async mode
        uwsgi_opt_set_int(opt, value, &uwsgi.async);
        if (uwsgi.socket_timeout < 30) {
                uwsgi.socket_timeout = 30;
        }
        // set loop engine
        uwsgi.loop = "cel";
}

static void cel_init() {

    uwsgi_register_loop( (char *) "cel", cel_loop);
}

static struct uwsgi_option cel_options[] = {
        {"cel", required_argument, 0, "a shortcut enabling cel loop engine with the specified number of async cores and optimal parameters", uwsgi_opt_setup_cel, NULL, UWSGI_OPT_THREADS},
        {0, 0, 0, 0, 0, 0, 0},

};

void cel_post_fork (void)
{

    ucel.kq_fd = kqueue();
    if(ucel.kq_fd == -1)
    {
        uwsgi_log("Kqueue: %s\n", strerror(errno));
        exit(1);
    }

    uwsgi_log("created kqueue fd: %d\n", ucel.kq_fd);
}

struct uwsgi_plugin custom_event_loop_plugin = {

        .name = "cel",
        .options = cel_options,
        .on_load = cel_init,
        .post_fork = cel_post_fork
};

