#define HTTP_MAX_BUFFER_SIZE 65536
#define HTTP_USERAGENT "libtrafikanten/0.1"

typedef struct json_object JSON;

typedef struct {
    char data[HTTP_MAX_BUFFER_SIZE];
    size_t size;
} http_buffer;

typedef struct {
    char line[8];
    int direction;
    char destination[64];
    time_t arrival;
} departure;

int trafikanten_get_departures(departure *deps, size_t maxdeps, char *id);
