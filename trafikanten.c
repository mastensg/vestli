#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#include "json.h"
#include "trafikanten.h"

static size_t
fill_buffer(void *ptr, size_t size, size_t nmemb, void *data) {
    size_t realsize = nmemb * size;
    http_buffer *buf = (http_buffer *)data;

    if(buf->size + realsize > HTTP_MAX_BUFFER_SIZE) {
        warnx("fill_buffer: too much data: realsize=%zd", realsize);
        return 0;
    }

    memcpy(buf->data + buf->size, ptr, realsize);
    buf->size += realsize;
    buf->data[buf->size] = 0;

    return realsize;
}

static int
http_get(http_buffer *buf, char *url) {
    buf->size = 0;

    CURL *curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, fill_buffer);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)buf);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, HTTP_USERAGENT);
    curl_easy_perform(curl_handle);
    curl_easy_cleanup(curl_handle);

    return 0;
}

int
trafikanten_get_departures(departure *deps, const size_t maxdeps, const struct station *station) {
    char url[256];
    sprintf(url, "http://reisapi.ruter.no/StopVisit/GetDepartures/%s", station->id);

    http_buffer buf;
    http_get(&buf, url);

    struct json_value *j = json_decode(buf.data);

    if (!j)
        return 0;

    size_t i = 0;
    struct json_value *n;
    struct json_node *m, *o, *p;

    for (n = j->v.array; n && i < maxdeps; n = n->next, ++i)
    {
        puts("\n-----------\n\n");

        for(m = n->v.object; m; m = m->next)
        {
            if(strcmp(m->name, "MonitoredVehicleJourney"))
                continue;

            for (o = m->value->v.object; o; o = o->next)
            {
                if(!strcmp(o->name, "DestinationName"))
                {
                    strcpy(deps[i].destination, o->value->v.string);
                }
                else if(!strcmp(o->name, "DirectionRef"))
                {
                    puts(o->name);
                    puts(">>> hest");
                    printf("%p\n", o->value);
                    printf("%p\n", o->value->type);
                    puts(o->value->v.string);
                    deps[i].direction = strtol(o->value->v.string, 0, 0);
                    puts(">>> hest");
                }
                else if(!strcmp(o->name, "LineRef"))
                {
                    strcpy(deps[i].line, o->value->v.string);
                }
                else if(!strcmp(o->name, "MonitoredCall"))
                {
                    for (p = o->value->v.object; p; p = p->next)
                    {
                        if(strcmp(m->name, "ExpectedArrivalTime"))
                            continue;

                        long long int t;
                        puts(p->value->v.string);

                        sscanf(m->value->v.string, "/Date(%lld+%*04d)/", &t);

                        deps[i].arrival = t / 1000;
                    }
                }
            }

            deps[i].station = station;
        }
    }

    json_free(j);

    return i;
}
