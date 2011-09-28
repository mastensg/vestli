#include <ctype.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"

static char *
json_decode_string (const char **input)
{
  const char *c, *end;
  unsigned int ch;
  char *result, *o;

  c = *input;

  if (*c != '"')
    return 0;

  end = ++c;

  while (*end && *end != '"')
    {
      if (*end == '\\' && *(end + 1))
        end += 2;
      else
        ++end;
    }

  result = malloc (end - c + 1);

  if (!result)
    return 0;

  o = result;

  while (c != end)
    {
      switch (*c)
        {
        case '\\':

          ++c;

          switch (*c++)
            {
            case '0': ch = strtol (c, (char **) &c, 8); break;
            case '"': ch = '"'; break;
            case '/': ch = '/'; break;
            case 'a': ch = '\a'; break;
            case 'b': ch = '\b'; break;
            case 't': ch = '\t'; break;
            case 'n': ch = '\n'; break;
            case 'v': ch = '\v'; break;
            case 'f': ch = '\f'; break;
            case 'r': ch = '\r'; break;
            case 'u': ch = strtol (c, (char **) &c, 16); break;
            case '\\': ch = '\\'; break;
            default:
              break;
            }

          if (ch < 0x80)
            *o++ = (ch);
          else if (ch < 0x800)
            {
              *o++ = (0xc0 | (ch >> 6));
              *o++ = (0x80 | (ch & 0x3f));
            }
          else if (ch < 0x10000)
            {
              *o++ = (0xe0 | (ch >> 12));
              *o++ = (0x80 | ((ch >> 6) & 0x3f));
              *o++ = (0x80 | (ch & 0x3f));
            }
          else if (ch < 0x200000)
            {
              *o++ = (0xf0 | (ch >> 18));
              *o++ = (0x80 | ((ch >> 12) & 0x3f));
              *o++ = (0x80 | ((ch >> 6) & 0x3f));
              *o++ = (0x80 | (ch & 0x3f));
            }
          else if (ch < 0x4000000)
            {
              *o++ = (0xf8 | (ch >> 24));
              *o++ = (0x80 | ((ch >> 18) & 0x3f));
              *o++ = (0x80 | ((ch >> 12) & 0x3f));
              *o++ = (0x80 | ((ch >> 6) & 0x3f));
              *o++ = (0x80 | (ch & 0x3f));
            }
          else
            {
              *o++ = (0xfc | (ch >> 30));
              *o++ = (0x80 | ((ch >> 24) & 0x3f));
              *o++ = (0x80 | ((ch >> 18) & 0x3f));
              *o++ = (0x80 | ((ch >> 12) & 0x3f));
              *o++ = (0x80 | ((ch >> 6) & 0x3f));
              *o++ = (0x80 | (ch & 0x3f));
            }

          break;

        default:

          *o++ = *c++;
        }
    }

  if (*c == '"')
    ++c;

  *o = 0;

  *input = c;

  return result;
}

static struct json_value *
json_decode_value (const char **input)
{
  struct json_value *result = 0;
  const char *c;

  c = *input;

  while (*c && isspace (*c))
    ++c;

  if (!*c)
    return 0;

  result = calloc (1, sizeof (*result));

  if (!result)
    return 0;

  if (isdigit (*c) || *c == '-')
    {
      result->type = json_number;
      result->v.number = strtod (c, (char **) &c);
    }
  else if (*c == '"')
    {
      result->type = json_string;
      result->v.string = json_decode_string (&c);
    }
  else if (*c == '[')
    {
      struct json_value *previous = 0;
      struct json_value *current = 0;

      ++c;

      result->type = json_array;

      for (;;)
        {
          while (*c && isspace (*c))
            ++c;

          if (previous)
            {
              if (*c != ',')
                break;

              ++c;
            }

          if (!*c || *c == ']')
            break;

          if (0 == (current = json_decode_value (&c)))
            break;

          if (previous)
            previous->next = current;
          else
            result->v.array = current;

          previous = current;
        }

      if (*c == ']')
        ++c;
    }
  else if (*c == '{')
    {
      struct json_node *previous = 0;
      struct json_node *current = 0;

      ++c;

      result->type = json_object;

      for (;;)
        {
          char *name;
          struct json_value *value;

          while (*c && isspace (*c))
            ++c;

          if (previous)
            {
              if (*c != ',')
                break;

              ++c;
            }

          while (*c && isspace (*c))
            ++c;

          if (*c != '"')
            break;

          if (0 == (name = json_decode_string (&c)))
            break;

          while (*c && isspace (*c))
            ++c;

          if (*c != ':')
            break;

          ++c;

          if (0 == (value = json_decode_value (&c)))
            break;

          if (0 == (current = calloc (1, sizeof (*current))))
            break;

          current->name = name;
          current->value = value;

          if (previous)
            previous->next = current;
          else
            result->v.object = current;

          previous = current;
        }

      if (*c == '}')
        ++c;
    }
  else if (!strncmp (c, "true", 4))
    {
      result->type = json_boolean;
      result->v.boolean = 1;
      c += 4;
    }
  else if (!strncmp (c, "false", 5))
    {
      result->type = json_boolean;
      result->v.boolean = 0;
      c += 5;
    }
  else if (!strncmp (c, "null", 4))
    {
      result->type = json_null;
      c += 4;
    }
  else
    {
      free (result);
      result = 0;
    }

  *input = c;

  return result;
}

struct json_value *
json_decode (const char *string)
{
  return json_decode_value (&string);
}

void
json_free (struct json_value *v)
{
  struct json_value *v_current, *v_next;
  struct json_node *n_current, *n_next;

  if (!v)
    return;

  switch (v->type)
    {
    case json_string:

      free (v->v.string);

      break;

    case json_array:

      for (v_current = v->v.array; v_current; v_current = v_next)
        {
          v_next = v_current->next;

          json_free (v_current);
        }

      break;

    case json_object:

      for (n_current = v->v.object; n_current; n_current = n_next)
        {
          n_next = n_current->next;

          json_free (n_current->value);
          free (n_current->name);
          free (n_current);
        }

      break;

    default:

      ;
    }

  free (v);
}

static int
json_print_string (const char *string)
{
  putchar ('"');

  while (*string)
    {
      switch (*string)
        {
        case '\b': putchar ('\\'); putchar ('b'); break;
        case '\f': putchar ('\\'); putchar ('f'); break;
        case '\n': putchar ('\\'); putchar ('n'); break;
        case '\r': putchar ('\\'); putchar ('r'); break;
        case '\t': putchar ('\\'); putchar ('t'); break;
        case '"':  putchar ('\\'); putchar ('"'); break;
        case '\\': putchar ('\\'); putchar ('\\'); break;

        default:

          if (iscntrl (*string))
            printf ("\\x%x", *string);
          else
            putchar (*string);

          break;
        }

      ++string;
    }

  putchar ('"');

  return 0;
}

int
json_print (const struct json_value *v)
{
  const struct json_node *n;

  if (!v)
    return printf ("<null>");

  switch (v->type)
    {
    case json_number:

      return printf ("%g", v->v.number);

    case json_string:

      return json_print_string (v->v.string);

    case json_boolean:

      return printf (v->v.boolean ? "true" : "false");

    case json_array:

      putchar ('[');

      for (v = v->v.array; v; v = v->next)
        {
          if (-1 == json_print (v))
            return -1;

          if (v->next)
            putchar (',');
        }

      putchar (']');

      return 0;

    case json_object:

      putchar ('{');

      for (n = v->v.object; n; n = n->next)
        {
          if (-1 == json_print_string (n->name))
            return -1;

          putchar (':');

          if (-1 == json_print (n->value))
            return -1;

          if (n->next)
            putchar (',');
        }

      putchar ('}');

      return 0;

    default:

      return printf ("null");
    }
}

#ifdef TEST
int
main (int argc, char **argv)
{
  struct json_value * j;

  j = json_decode ("{\"error\":{\"code\":-257,\"result\":null,\"message\":\"Hash is too large: want=0000000001beb000000000000000000000000000000000000000000000000000 has=0bb15d2115e38086e3b6c568c861ee45eaa733fa5ee94ace6f6105267518d8d7\n\"},\"id\":1}");
  json_print (j);
}
#endif
