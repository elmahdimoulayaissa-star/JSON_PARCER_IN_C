#ifndef CJSON_H
#define CJSON_H

#define MAX_STRING 200
#define MAX_INT_SIZE 20
#define MAX_FLOAT_PRES 17
#define MAX_EXPO 3


enum cJSON_t{
     STRING,
     NUMBER,
     ARRAY,
     OBJECT,
};

typedef struct cJSON
{
    struct cJSON *next;
    struct cJSON *prev;
    struct cJSON *child;
    enum cJSON_t type;
    char *valuestring;
    double valuedouble;
    char *string;
} cJSON;


#endif
