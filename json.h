#ifndef JSON_H_
#define JSON_H_

struct json_node;

enum json_value_type
{
  json_invalid_type = 0,
  json_number,
  json_string,
  json_boolean,
  json_array,
  json_object,
  json_null
};

struct json_value
{
  enum json_value_type type;

  union
    {
      double number;
      char *string;
      int boolean;
      struct json_value *array;
      struct json_node *object;
    } v;

  struct json_value *next;
};

struct json_node
{
  char *name;
  struct json_value *value;
  struct json_node *next;
};

struct json_value *
json_decode (const char *string);

void
json_free (struct json_value *v);

int
json_print (const struct json_value *v);

#endif /* !JSON_H_ */
