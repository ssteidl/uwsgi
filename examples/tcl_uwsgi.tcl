proc application {environ start_response} {

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

    set headers [dict create {Content-Type} {text/plain} {X-JOE} {JACK}]
    $start_response $response_code $headers
    return $payload
}
