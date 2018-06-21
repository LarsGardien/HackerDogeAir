#define _XOPEN_SOURCE 700 //must be defined for strptime from time.h
#include <stdbool.h>
#include <limits.h>
#include <mustache.h>

#include <kore/kore.h>
#include <kore/http.h>
#include <kore/pgsql.h>

#include "shared/shared_time.h"
#include "shared/shared_error.h"
#include "pages/flight/flight_search/flight_search_render.h"
#include "model/user.h"
#include "assets.h"
#include "time.h"

#define FLIGHT_SEARCH_ERROR_ARRIVALDATE_VALIDATOR_INVALID 401

int     flight_search(struct http_request *);
int     flight_search_parseparams(struct http_request *, FlightSearchParams *search_params);

void    flight_search_error_handler(struct http_request *, int, FlightSearchContext *);

int
flight_search(struct http_request *req)
{
    int err = 0;
    time_t zero_epoch = 0;
    struct tm *zero_arrivaldate = localtime(&zero_epoch);
    FlightSearchContext context = {
        .partial_context = {.session_id = 0},
        .params = {.arrivaldate = *zero_arrivaldate}
    };
    SLIST_INIT(&context.flightlist);
    if(req->method == HTTP_METHOD_GET)
    {
        if((err = flight_search_render(&context)))
        {
            flight_search_error_handler(req, err, &context);
        }
        http_response_header(req, "content-type", "text/html");
        http_response(req, HTTP_STATUS_OK,
            context.partial_context.dst_context->string,
            strlen(context.partial_context.dst_context->string));
        flight_search_render_clean(&context);
        return (KORE_RESULT_OK);
    }
    else if(req->method != HTTP_METHOD_POST)
    {
        return (KORE_RESULT_ERROR);
    }

    if((err = flight_search_parseparams(req, &context.params)) != (SHARED_OK))
    {
        flight_search_error_handler(req, err, &context);
        return (KORE_RESULT_OK);
    }

    //TODO: DataAccess layer search using context.params
    char *departure0 = "</td><h1>Hello there</h1><td>";
    char *arrival0 = "UitjeHol";

    FlightSearchListNode flight_node0 = {
        .flight = {
            .arrival_datetime = {0, 15, 13, 18, 12, 2018-1900, 0, 0},
            .departure_datetime = {0, 10, 13, 18, 12, 2018-1900, 0, 0},
            .arrival_location = arrival0,
            .departure_location = departure0
        }
    };
    SLIST_INSERT_HEAD(&context.flightlist, &flight_node0, flights);

    char *departure1 = "SchipUitJeHol";
    char *arrival1 = "InjeHol";

    FlightSearchListNode flight_node1 = {
        .flight = {
            .arrival_datetime = {0, 15, 13, 18, 12, 2018-1900, 0, 0},
            .departure_datetime = {0, 10, 13, 18, 12, 2018-1900, 0, 0},
            .arrival_location = arrival1,
            .departure_location = departure1
        }
    };
    SLIST_INSERT_HEAD(&context.flightlist, &flight_node1, flights);

    if((err = flight_search_render(&context)) != (SHARED_OK))
    {
        flight_search_error_handler(req, err, &context);  
        return (KORE_RESULT_OK); 
    }

    http_response_header(req, "content-type", "text/html");
    http_response(req, HTTP_STATUS_OK,
        context.partial_context.dst_context->string,
        strlen(context.partial_context.dst_context->string));
    flight_search_render_clean(&context);
    return (KORE_RESULT_OK);
}

int flight_search_parseparams(struct http_request *req, FlightSearchParams *search_params)
{
    http_populate_post(req);
    int err = 0;
    char *date = NULL;
    if(!http_argument_get_string(req, "arrivaldate", &date))
    {
        return (FLIGHT_SEARCH_ERROR_ARRIVALDATE_VALIDATOR_INVALID);
    }

    if((err = shared_time_string_to_tm(date, &search_params->arrivaldate, "%d-%m-%Y")) 
        != (SHARED_OK))
    {
        kore_log(LOG_ERR, "flight_search_parseparams: time conversion error %d", err);
        return (FLIGHT_SEARCH_ERROR_ARRIVALDATE_VALIDATOR_INVALID);
    }

    return (SHARED_OK);
}

void
flight_search_error_handler(struct http_request *req, int errcode, FlightSearchContext *context)
{
    bool handled = true;
    int err = 0;
    switch (errcode)
    {
        case (FLIGHT_SEARCH_ERROR_ARRIVALDATE_VALIDATOR_INVALID):
                context->error_message = 
                "Please enter a correct arrival date. (e.g. 28-01-2018)";
            break;

        default:
            handled = false;
    }

    if(!handled)
    {
        shared_error_handler(req, errcode, "/flight/search");
    }
    else
    {
        if((err = flight_search_render(context)) != (SHARED_OK))
        {
            flight_search_error_handler(req, err, context);
        }

        http_response_header(req, "content-type", "text/html");
        http_response(req, HTTP_STATUS_OK, 
            context->partial_context.dst_context->string, 
            strlen(context->partial_context.dst_context->string));

        flight_search_render_clean(context);
    }
}
