AM_CFLAGS = -Wall -Wextra -pedantic -std=c99 -g

bin_PROGRAMS = vestli

vestli_LDFLAGS = -lSDL -lSDL_ttf -lcurl

vestli_SOURCES = vestli.c trafikanten.h trafikanten.c json.h json.c
vestli_CPPFLAGS = -DPROGRAM_NAME="\"vestli\""
