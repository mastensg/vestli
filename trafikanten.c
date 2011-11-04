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
    sprintf(url, "http://api-test.trafikanten.no/RealTime/GetRealTimeData/%s", station->id);

    http_buffer buf;
    http_get(&buf, url);

    struct json_value *j = json_decode(buf.data);

    if (!j)
        return 0;

    int i = 0;
    for(struct json_value *n = j->v.array; n && i < maxdeps; n = n->next, ++i) {
        for(struct json_node *m = n->v.object; m; m = m->next) {
            if(!strcmp(m->name, "DestinationName"))
              strcpy(deps[i].destination, m->value->v.string);
            else if(!strcmp(m->name, "DirectionRef"))
              deps[i].direction = strtol(m->value->v.string, 0, 0);
            else if(!strcmp(m->name, "LineRef"))
              strcpy(deps[i].line, m->value->v.string);
            else if(!strcmp(m->name, "ExpectedArrivalTime")) {
                long long int t;
                sscanf(m->value->v.string, "/Date(%lld+%*04d)/", &t);
                deps[i].arrival = t / 1000;
            }

            deps[i].station = station;
        }
    }

    json_free(j);

    return i;
}
