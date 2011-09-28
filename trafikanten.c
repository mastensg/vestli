#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#include <json/json.h>

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

    sleep(1);

    CURL *curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, fill_buffer);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)buf);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, HTTP_USERAGENT);
    curl_easy_perform(curl_handle);
    curl_easy_cleanup(curl_handle);

    return 0;
}

static void
json_print_type(JSON *j) {
    enum json_type type = json_object_get_type(j);
    printf("type: ");

    switch (type) {
    case json_type_null:
        printf("json_type_null\n");
        break;
    case json_type_boolean:
        printf("json_type_boolean\n");
        break;
    case json_type_double:
        printf("json_type_double\n");
        break;
    case json_type_int:
        printf("json_type_int\n");
        break;
    case json_type_object:
        printf("json_type_object\n");
        break;
    case json_type_array:
        printf("json_type_array\n");
        break;
    case json_type_string:
        printf("json_type_string\n");
        break;
    }
}

static void
json_get_string(char *dst, JSON *srcobj, char *key) {
    JSON *srcent = json_object_object_get(srcobj, key);
    char const *srcstr = json_object_get_string(srcent);

    strcpy(dst, srcstr);

    json_object_put(srcent);
}

static void
json_get_int(int *dst, JSON *srcobj, char *key) {
    JSON *srcent = json_object_object_get(srcobj, key);

    *dst = json_object_get_int(srcent);

    json_object_put(srcent);
}

static int
json_get_time(time_t *dst, JSON *srcobj, char *key) {
    JSON *srcent = json_object_object_get(srcobj, key);
    char const *timestr = json_object_get_string(srcent);

    long long int t;
    int z;
    sscanf(timestr, "/Date(%lld+%04d)/", &t, &z);
    *dst = t / 1000;

    json_object_put(srcent);

    return 0;
}

int
trafikanten_get_departures(departure *deps, const size_t maxdeps, const char *id) {
    char url[256];
    sprintf(url, "http://api-test.trafikanten.no/RealTime/GetRealTimeData/%s", id);

    http_buffer buf;
    http_get(&buf, url);

    JSON *j = json_tokener_parse(buf.data);

    int i;
    int n = json_object_array_length(j);
    for(i = 0; i < n && i < maxdeps; ++i) {
        JSON *jdep = json_object_array_get_idx(j, i);

        json_get_string(deps[i].destination, jdep, "DestinationName");
        json_get_int(&deps[i].direction, jdep, "DirectionRef");
        json_get_string(deps[i].line, jdep, "LineRef");
        json_get_time(&deps[i].arrival, jdep, "ExpectedArrivalTime");

        json_object_put(jdep);
    }

    return i;
}
