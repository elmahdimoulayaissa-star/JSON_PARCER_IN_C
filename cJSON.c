#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include <math.h>


static int Object(char* file_str,int* pos ,cJSON* head); 
static int white_space_sanitizer(char* file_str,int* pos);
static bool is_white_space(char curr);
static int string_reader(char* file_str,int* pos ,cJSON* parent);
static int number_reader(char* file_str,int* pos, cJSON* parent);
static bool is_digit(char curr);
static int array_reader(char curr);

int cjson(FILE* file){
    if (file ==NULL){
        printf("file doens't exist ");
        return -1;
    }

    fseek(file,0, SEEK_END);
    long len = ftell(file);
    fseek(file,0, SEEK_END);

    char* file_str = malloc(len + 1);
    fread(file_str, 0, len, file);
    file_str[len] = '\0';

    cJSON head = {.prev = NULL};
    char curr = *file_str;
    int* pos;
    *pos = 0;


    while(curr != '/0'){
        if (curr == '{'){

        }


    }

    return  0;
}

static bool is_white_space(char curr){
    if ( curr == ' ' || curr == '\n' || curr == '\t' || curr == '\r' ){
        return true;
    }
    return false;
}

static int white_space_sanitizer(char* file_str,int* pos){
    char curr  = file_str[*pos];
    while (is_white_space(curr)){
        *pos += 1;
        curr = file_str[*pos];
    }
    return 0;
}


static int string_reader(char* file_str,int* pos ,cJSON* parent){
    *pos+=1;
    char curr = file_str[*pos];
    char* buffer = malloc(100*sizeof(char));
    int i = 0;
    if (!buffer){
        //TODO 
        return -1;
    }

    while (curr != '"'){
        
        if(curr == '\0'){
            //TODO
            free(buffer);
            return -1;
        }
        if (curr == '\\'){
            curr = file_str[ ++ (*pos)];
            switch (curr) {
                case '"': buffer[++i] = '"';break;
                case '\\': buffer[++i] = '\\' ; break;
                case '/': buffer[++i] = '/'; break;
                case 'b': buffer[++i] = '\b';break;
                case 'f': buffer[++i] = '\f';break;
                case 'n': buffer[++i] = '\n';break;
                case 'r': buffer[++i] = '\r';break;
                case 't': buffer[++i] = '\t';break;
                case 'u': //TODO \uXXXX
                          break;
                default:
                        free(buffer);
                        //TODO
                        return -1;
            }
        }else {
            buffer[i++] = curr;
            (*pos)++;
            curr = file_str[*pos];
        }


    }
    buffer[++i] = '\0';
    (*pos)++;
    
    parent->type = STRING; 
    parent->string = strdup(buffer); 

    free(buffer);
    return 0;
}


static bool is_digit(char curr){
    return curr >= '0' && curr <= '9';
}

static int number_reader(char* file_str,int* pos, cJSON* parent){
    *pos += 1;
    char curr = file_str[*pos];
    bool positive = true;
    bool positive_expo = true;
    uint8_t * number = calloc(MAX_INT_SIZE, sizeof(uint8_t));
    uint8_t* precision = calloc(MAX_FLOAT_PRES, sizeof(uint8_t));
    uint8_t* expo = calloc(MAX_EXPO, sizeof(uint8_t));
    if (!number || !precision || !expo) {
        free(number);free(precision);free(expo);
        //TODO
        return -1;
    }   

    int top_num = -1;
    int top_pes = -1;
    int top_expo = -1;


    if(curr == '-'){
        positive = false;
        *pos += 1 ;
        curr = file_str[*pos];
    }
    if (is_digit(curr) && curr != '0'){
        while (is_digit(curr)) {
            uint8_t digit = (uint8_t)curr - (uint8_t)'0';
            if (++top_num >= MAX_INT_SIZE) {
                free(number);free(precision);free(expo);
                //TODO
                return -1;
            }
            number[top_num] = digit;
            curr = file_str[++(*pos)];
        }
    }else if (curr == '0') {
        curr = file_str[++(*pos)];
        if (is_digit(curr)) {
            //TODO
            free(number);free(precision);free(expo);
            return -1;
        }
    }else {
        free(number);free(precision);free(expo);
        //TODO
        return -1;
    }
    //FRACTION
    if (curr == '.') {
        curr = file_str[++(*pos)];
        if (!is_digit(curr)){
            //TODO
            free(number);free(precision);free(expo);
            return -1;
        }
        while ( is_digit(curr)){
            uint8_t digit = (uint8_t)curr - (uint8_t)'0';
            precision[++top_pes] = digit;
            curr = file_str[++(*pos)];
        }
    }else if (curr == 'e' || curr == 'E' || is_white_space(curr)) {}
    else{
        free(number);free(precision);free(expo);
        //TODO EROR
        return -1;
    }
    // EXPO

    if (curr == 'e' || curr == 'E') {
        curr = file_str[++(*pos)];

        if (curr == '-') {
            positive_expo = false;
            curr = file_str[++(*pos)];
        }else if(curr == '+'){
            curr = file_str[++(*pos)];
        }
        if (!is_digit(curr)){
            //TODO
            free(number);free(precision);free(expo);
            return -1;
        }

        while ( is_digit(curr)){
            uint8_t digit = (uint8_t)curr - (uint8_t)'0';
            expo[++top_expo] = digit;
            curr = file_str[++(*pos)];
        }

    }else if(is_white_space(curr)){}
    else{
        free(number);free(precision);free(expo);
        //TODO
        return -1;
    }


    // Fuse the number
    double res = 0;
    
    for(int i = top_num ; i>=0;i-- ){
        res = (res * 10) + number[i];
    }
    
    double frac = 0.1; 
    for(int i = 0; i <= top_pes ;i++){
        res += precision[i] * frac;
        frac *= 0.1;
    }
    long e = 0;

    for (int i = top_expo;i>=0;i--){
        e = (e * 10) + expo[i];
    }
    e = (positive_expo)? e : (-1*e);

    res = res * pow(10,e);

    res = (positive) ? res : (-1 * res);

    // JSON linking

    free(number);free(precision);free(expo);
    if(isinf(res)){
        //TODO
        return -1;
    } 
    parent->type = NUMBER;
    parent->valuedouble = res;
    return 0;

}
