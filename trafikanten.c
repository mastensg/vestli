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
json_print_type(enum json_type type) {
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

int
trafikanten_get_departures(departure *deps, size_t maxdeps, char *id) {
    //char url[256];
    //sprintf(url, "http://api-test.trafikanten.no/RealTime/GetRealTimeData/%s", id);
    char *url = "http://www.ping.uio.no/~mastensg/oslos.json";

    http_buffer buf;
    http_get(&buf, url);

    struct json_object *j = json_tokener_parse(buf.data);

    int n = json_object_array_length(j);
    for(int i = 0; i < n; ++i) {
        struct json_object *jdep = json_object_array_get_idx(j, i);

        struct json_object *jdest = json_object_object_get(jdep, "DestinationName");
        struct json_object *jdeststr = json_object_get_string(jdest);

        struct json_object *jline = json_object_object_get(jdep, "LineRef");
        int jlineint = json_object_get_int(jline);

        printf("%d %s\n", jlineint, jdeststr);

        json_object_put(jdest);
        json_object_put(jdeststr);
        json_object_put(jline);
        json_object_put(jdep);
    }

    return 0;
}
