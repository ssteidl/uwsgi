#include <assert.h>
#include <tcl8.6/tcl.h>
#include <uwsgi.h>

extern struct uwsgi_server uwsgi;

struct uwsgi_tcl
{
    int initialized;
    Tcl_Interp* interp;
    char* tcl_script;
};

struct uwsgi_tcl utcl;

static int uwsgi_tcl_init(){
    uwsgi_log("Initializing tcl plugin\n");

    if(utcl.initialized) {

        goto already_initialized;
    }

    utcl.initialized = 1;

already_initialized:
    return 1;
}

static int uwsgi_tclrespond(ClientData clientData, Tcl_Interp *interp,
                             int objc, struct Tcl_Obj *const *objv) {
    int tcl_error = TCL_OK;

    if(objc != 4)
    {
        //TODO: Content type should be a list of headers.
        Tcl_WrongNumArgs(interp, objc, objv, "Exactly 3 arguments required: <response code> <content-type> <payload>");
        return TCL_ERROR;
    }

    int status = 0;
    int status_number_len = 0;
    char* status_number_str = Tcl_GetStringFromObj(objv[1], &status_number_len);
    tcl_error = Tcl_GetInt(interp, status_number_str, &status);
    if(tcl_error)
    {
        Tcl_Obj* args[] = {Tcl_GetObjResult(interp)};
        Tcl_Obj* result_msg = Tcl_Format(interp, "Response code is not an integer: %s\n", 1, args);
        if(result_msg == NULL)
        {
            uwsgi_log("Internal error: [%s line %d]\n", __FILE__, __LINE__);
            exit(1);
        }

        Tcl_SetObjResult(interp, result_msg);
        return TCL_ERROR;
    }

    struct wsgi_request* wsgi_req = current_wsgi_req();
    assert(wsgi_req);
    int error = uwsgi_response_prepare_headers(wsgi_req, status_number_str, status_number_len);
    assert(!error);

    int content_type_len = 0;
    char* content_type = Tcl_GetStringFromObj(objv[2], &content_type_len);

    error = uwsgi_response_add_content_type(wsgi_req, content_type, content_type_len);
    assert(!error);

    int payload_len = 0;
    char* payload = Tcl_GetStringFromObj(objv[3], &payload_len);

    error = uwsgi_response_add_content_length(wsgi_req, payload_len);
    assert(!error);

    error = uwsgi_response_write_body_do(wsgi_req, payload, payload_len);
    assert(!error);

    return TCL_OK;
}

static int uwsgi_tclrequest_body(ClientData clientData, Tcl_Interp *interp,
                                 int objc, struct Tcl_Obj *const *objv) {

    if(objc != 2)
    {
        Tcl_WrongNumArgs(interp, objc, objv, "Expected exactly 1 argument: <read_size | -1>");
        return TCL_ERROR;
    }

    int read_size = 0;
    int ret = Tcl_GetIntFromObj(interp, objv[1], &read_size);
    if(ret == TCL_ERROR)
    {
        return TCL_ERROR;
    }

    struct wsgi_request* wsgi_req = current_wsgi_req();
    ssize_t actual = 0;
    char* body = uwsgi_request_body_read(wsgi_req, read_size, &actual);
    if(body == NULL)
    {
        Tcl_SetResult(interp, "Error reading request body", NULL);
        return TCL_ERROR;
    }

    Tcl_Obj* body_obj = Tcl_NewByteArrayObj((unsigned char*)body, actual);
    Tcl_SetObjResult(interp, body_obj);
    return TCL_OK;
}

static void uwsgi_tcl_app() {

    if(utcl.tcl_script == NULL)
    {
        uwsgi_log("No tcl script provided: [%s line %d]\n", __FILE__, __LINE__);
        exit(1);
    }
}

static void uwsgi_tcl_after_fork()
{
    //Initializes some tcl subsystems and calculates path to executable
    Tcl_FindExecutable(NULL);
    utcl.interp = Tcl_CreateInterp();
    if(utcl.interp == NULL) {

        uwsgi_log("Failed to create tcl interpreter during intialization: [%s line %d]\n", __FILE__, __LINE__);
        exit(1);
    }

    int tcl_error = Tcl_Init(utcl.interp);
    if(tcl_error) {

        uwsgi_log("Error initializing tcl interpreter: [%s line %d]\n", __FILE__, __LINE__);
        exit(1);
    }

    tcl_error = Tcl_EvalFile(utcl.interp, utcl.tcl_script);
    if(tcl_error) {

        uwsgi_log("Error evaluating Tcl Script: '%s' [%s line %d]\n",
              Tcl_GetStringResult(utcl.interp), __FILE__, __LINE__);
        exit(1);
    }


    Tcl_Command respond_cmd_token = Tcl_CreateObjCommand(utcl.interp, "uwsgi::respond", uwsgi_tclrespond, NULL, NULL);
    Tcl_Command req_body = Tcl_CreateObjCommand(utcl.interp, "uwsgi::request_body", uwsgi_tclrequest_body, NULL, NULL);

    //TODO: Store the token somewhere.
    (void)respond_cmd_token;
    (void)req_body;
}

static void add_to_environ_dict(Tcl_Obj* environ,
                                char* key, size_t key_len,
                                char* value, size_t val_len) {
    int error = Tcl_DictObjPut(utcl.interp, environ,
                               Tcl_NewStringObj(key, key_len),
                               Tcl_NewStringObj(value, val_len));

    if(error)
    {
        uwsgi_log("Unable to put key '%s' with value '%s' into tcl environ dict", key, value);
        exit(1);
    }

}

static int uwsgi_tcl_request(struct wsgi_request *wsgi_req) {

    int error = uwsgi_parse_vars(wsgi_req);
    assert(!error);

    uwsgi_log("found script: %s, %d\n", utcl.tcl_script);
    char* buffer[wsgi_req->len+1];
    buffer[wsgi_req->len] = 0;

    wsgi_req->app_id = uwsgi_get_app_id(wsgi_req, wsgi_req->appid, wsgi_req->appid_len, 251);

    //Create the environ variables
    Tcl_Obj* app = Tcl_NewStringObj("application", 11);
    Tcl_Obj* environ = Tcl_NewDictObj();
    assert(environ);

    add_to_environ_dict(environ,
                        "REQUEST_METHOD", 14,
                        wsgi_req->method, wsgi_req->method_len);

    //TODO: DOCUMENT_ROOT.  Probably do it similar to cgi
    int i;
    for(i=0;i<wsgi_req->var_cnt;i+=2) {
        add_to_environ_dict(environ, wsgi_req->hvec[i].iov_base, wsgi_req->hvec[i].iov_len, wsgi_req->hvec[i+1].iov_base, wsgi_req->hvec[i+1].iov_len);
    }

    add_to_environ_dict(environ,
                        "SCRIPT_FILENAME", 15,
                        wsgi_req->file, wsgi_req->file_len);

    add_to_environ_dict(environ,
                        "QUERY_STRING", 12,
                        wsgi_req->query_string, wsgi_req->query_string_len);

    add_to_environ_dict(environ,
                        "CONTENT_TYPE", 12,
                        wsgi_req->content_type, wsgi_req->content_type_len);

    memcpy(buffer, wsgi_req->buffer, wsgi_req->len);
    uwsgi_log("BODY: %s\n", buffer);

    Tcl_Obj* cmd[] = {app, environ};
    Tcl_Obj* cmd_obj = Tcl_NewListObj(2, cmd);
    int tcl_error = Tcl_EvalObj(utcl.interp, cmd_obj);
    if(tcl_error)
    {
        Tcl_Obj* option_dict = Tcl_GetReturnOptions(utcl.interp, tcl_error);
        char* dict_string = Tcl_GetStringFromObj(option_dict, NULL);
//        Tcl_LogCommandInfo(utcl.interp, utcl.tcl_script, "application", -1);
        uwsgi_log("tcl error: %s\n %s\n", Tcl_GetStringResult(utcl.interp), dict_string);
        return UWSGI_OK;
    }

    return UWSGI_OK;
}


static void uwsgi_tcl_after_request(struct wsgi_request *wsgi_req) {

    log_request(wsgi_req);
}

struct uwsgi_option uwsgi_tcl_options[] = {

    {"tcl", required_argument, 0, "load a tcl app", uwsgi_opt_set_str, &utcl.tcl_script, 0},
    {0, 0, 0, 0, 0, 0, 0}
};
struct uwsgi_plugin tcl_plugin = {

        .name = "tcl",
        .modifier1 = 251,
        .init = uwsgi_tcl_init,
        .post_fork = uwsgi_tcl_after_fork,
        .options = uwsgi_tcl_options,
        .init_apps = uwsgi_tcl_app,
        .request = uwsgi_tcl_request,
        .after_request = uwsgi_tcl_after_request,

};
