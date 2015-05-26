#include "value.h"

value_t* value_new_null()
{
	value_t* val = malloc(sizeof(*val));
	val->type = VALUE_NULL;
	return val;
}

value_t* value_new_int(long number)
{
	value_t* val = value_new_null();
	val->type = VALUE_INT;
	val->v.i = number;
	return val;
}

value_t* value_new_float(double number)
{
	value_t* val = value_new_null();
	val->type = VALUE_FLOAT;
	val->v.f = number;
	return val;
}

value_t* value_new_string(const char* string)
{
	value_t* val = value_new_null();
	val->type = VALUE_STRING;
	val->v.str = strdup(string);
	return val;
}

void value_free(value_t* value)
{
	if(value->type == VALUE_STRING)
	{
		free(value->v.str);
	}
	free(value);
}
