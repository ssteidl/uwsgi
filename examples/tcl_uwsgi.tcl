proc application {environ} {

    set response_code {200}
    set payload {}

    set method [dict get $environ REQUEST_METHOD]

    switch $method {

	GET {
	    set payload $environ
	}
	POST {
	    set payload [uwsgi::request_body -1]
	    append payload " -- [string length $payload]"
	}
	default {
	    set response_code {405}
	}
    }

    uwsgi::respond $response_code {text/plain} $payload
}
