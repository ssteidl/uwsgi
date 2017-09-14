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
    uwsgi_response_prepare_headers(wsgi_req, "200 OK", 6);
    uwsgi_response_add_header(wsgi_req, "Content-type", 12, "text/plain", 9);
    int content_length = 0;
    char* response_string = Tcl_GetStringFromObj(response, &content_length);
    uwsgi_log("response: %s, %d\n", response_string, content_length);
    uwsgi_response_write_body_do(wsgi_req, response_string, content_length);

    return UWSGI_OK;
}


static void uwsgi_tcl_after_request(struct wsgi_request *wsgi_req) {
    log_request(wsgi_req);
    uwsgi_log("i am the example plugin after request function\n");
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
