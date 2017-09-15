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

static void uwsgi_tcl_app() {

    if(utcl.tcl_script == NULL)
    {
        uwsgi_log("No tcl script provided: [%s line %d]\n", __FILE__, __LINE__);
        exit(1);
    }

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
    }


    Tcl_Command respond_cmd_token = Tcl_CreateObjCommand(utcl.interp, "uwsgi::respond", uwsgi_tclrespond, NULL, NULL);
    //TODO: Store the token somewhere.
    (void)respond_cmd_token;
}

static int uwsgi_tcl_request(struct wsgi_request *wsgi_req) {

    uwsgi_log("found script: %s\n", utcl.tcl_script);
    uwsgi_parse_vars(wsgi_req);
    wsgi_req->app_id = uwsgi_get_app_id(wsgi_req, wsgi_req->appid, wsgi_req->appid_len, 251);

    int tcl_error = Tcl_Eval(utcl.interp, "/ shane");
    if(tcl_error)
    {
        uwsgi_log("tcl error: %s\n", Tcl_GetStringResult(utcl.interp));
        return UWSGI_OK;
    }

    Tcl_Obj* response = Tcl_GetObjResult(utcl.interp);
    int content_length = 0;
    char* response_string = Tcl_GetStringFromObj(response, &content_length);
    uwsgi_log("response: %s, %d\n", response_string, content_length);
    uwsgi_response_write_body_do(wsgi_req, response_string, content_length);

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
        .options = uwsgi_tcl_options,
        .init_apps = uwsgi_tcl_app,
        .request = uwsgi_tcl_request,
        .after_request = uwsgi_tcl_after_request,

};
