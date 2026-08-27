#ifndef CJSON_H
#define CJSON_H

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#define MAX_STRING 200
#define MAX_INT_SIZE 20
#define MAX_FLOAT_PRES 17
#define MAX_EXPO 3



enum JSONC_t{
     STRING,
     NUMBER,
     BOOLEAN,  
     NULL_e,
     ARRAY, 
     OBJECT,
};

typedef struct JSONC
{
    struct JSONC *next;
    struct JSONC *prev;
    struct JSONC *child;
    enum JSONC_t type;
    char *valuestring;
    double valuedouble;
    char *string;
} JSONC;    

typedef enum {  
    JSON_OK ,
    JSON_ERR_IO,
    JSON_ERR_ALLOC,
    JSON_ERR_END_OF_FILE,
    JSON_ERR_UNEXPECTED_CHAR,
    JSON_ERR_INVALID_ESCAPE,
    JSON_ERR_INVALID_NUMBER,
    JSON_ERR_INVALID_NUMBER_PRES,   
    JSON_ERR_INVALID_NUMBER_INF,
    JSON_ERR_EXPECTED_COLON,
    JSON_ERR_EXPECTED_COMMA_OR_END,
    JSON_ERR_INVALID_LITERRAL,
    
}JSON_Errorcode;

typedef struct JSON_Error {
    JSON_Errorcode code;
    bool JSON_OK;
    int pos ;
    int line ;
    char* near;
}JSON_Error;

JSONC* json_in_c(FILE* file);
void json_c_delete(JSONC* head);
void print_JSONC(JSONC *head);

#endif
