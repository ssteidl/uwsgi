
proc / {uwsgi} {

    set payload {{"id": 71, "value": "uwsgi is awesome"}}
    append payload "\n"
    uwsgi::respond {201} {text/json} $payload
}
