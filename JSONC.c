#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include "JSONC.h"
#include <math.h>
#include <time.h>


enum STRING_t{
    NAME,
    VALUE,
};

static JSON_Error err = {.JSON_OK=true};

static const char* error_message(JSON_Errorcode code);
static const int locate(char* file_str,int* pos,int* line,char*near);
static const int report_error (char* file_str,int* pos,JSON_Errorcode code);


static int object_reader(char* file_str,int* pos ,JSONC* current_node); 
static int array_reader(char* filr_str, int* pos,JSONC* current_node);
static int white_space_sanitizer(char* file_str,int* pos);
static bool is_white_space(char curr);
static int string_reader(char* file_str,int* pos ,JSONC* current_node,enum STRING_t type);
static int number_reader(char* file_str,int* pos, JSONC* current_node);
static bool is_digit(char curr);
static int value_reader(char* file_str ,int* pos , JSONC* current_node);
static char utf8_encoder(char* file_str,int* pos, JSONC* current_node);
static void print_JSONC_rec(JSONC* head,int depth);

void print_JSONC(JSONC *head){
    print_JSONC_rec(head, 0);
}

static void print_JSONC_rec(JSONC* head,int depth){
    for(int i=0; i<depth;i++)printf(" ");
    if(head->string != NULL && head->string != 0){
        printf("\"%s\"\t:\t",head->string);
    }
    switch (head->type) {
        case STRING:
            printf("%s\t","STRING");
            printf("%s",head->valuestring);
            break;
        case NUMBER:
            printf("%s\t","NUMBER");
            printf("%e",head->valuedouble);
            break;
        case BOOLEAN:
            printf("%s\t","BOOLIEN");
            printf("%s",head->valuestring);
            break;
        case NULL_e:
            printf("%s\t","NULL");
            printf("%s","NULL");
            break;
        case OBJECT :
            printf("%s\t","OBJECT");
            if (head->child != NULL) {
                printf("\n");
                print_JSONC_rec(head->child, depth+1);
            }
            break;
        case ARRAY:
            printf("%s\t","ARRAY");
            if (head->child != NULL) {
                printf("\n");
                print_JSONC_rec(head->child, depth+1);
            }
            break;
    }
    printf("\n");
    if (head->next !=NULL) {
        print_JSONC_rec(head->next, depth);
    }
}
void json_c_delete(JSONC* head){
    if (head == NULL )return;
    if(head->child != NULL)json_c_delete(head->child);
    if(head->next !=NULL)json_c_delete(head->next);
    free(head);
    return;
}

JSONC* json_in_c(FILE* file){
    if (file ==NULL){
        report_error(NULL,NULL, JSON_ERR_IO);
        return NULL;
    }

    fseek(file,0, SEEK_END);
    long len = ftell(file);
    fseek(file,0, SEEK_SET);

    char* file_str = malloc(len + 1);
    fread(file_str,1 ,len, file);
    file_str[len] = '\0';

    JSONC* head = calloc(1, sizeof(*head));
    if (head == NULL){
        report_error(NULL,NULL, JSON_ERR_ALLOC);
        free(file_str);
        return NULL;
    } 
    if (file_str == NULL) {
        report_error(NULL,NULL, JSON_ERR_ALLOC);
        free(head);
        return NULL;
    }

    int pos = 0;
    white_space_sanitizer(file_str, &pos);

    char curr = *(file_str + pos);
    value_reader(file_str, &pos, head);
    free(file_str);
    if(err.JSON_OK)return head;
    else return NULL;
}

static const int report_error (char* file_str,int* pos,JSON_Errorcode code){
    err.JSON_OK = false;
    if (file_str == NULL || pos == NULL){
        fprintf(stderr, "JSON parce error %s ", error_message(code));
//        fprintf(stdout, "JSON parce error %s ", error_message(code));
        return 0;
    }

    int line;char near[16];
    locate(file_str, pos,&line,near);
    err.pos=*pos;
    err.line=line;
    err.code = code;
    err.near = near ;

    fprintf(stderr, "JSON parce error at line %d , at position %d \n near : %s \n with error %s", line,*pos,near,error_message(code));
    fprintf(stdout, "JSON parce error at line %d , at position %d \n near : %s \n with error %s", line,*pos,near,error_message(code));
    return -1;
}

static const int locate(char* file_str,int* pos,int* line,char* near){
    *line = 1;
    for(int i = 0 ; i<=*pos && file_str[i];i++){
        if (file_str[i] == '\n')(*line)++;
    }

    int start = ((*pos) -14 >= 0 ) ? (*pos) - 14 : 0;
    int n = (start != 0) ? 15 : 15 + ((*pos)-15);
    memcpy(near,file_str, n);
    near[n]='\0';

    return 0;
}


static const char* error_message(JSON_Errorcode code){
    switch (code) {
        case JSON_ERR_IO:return "could not read file ";
        case JSON_ERR_ALLOC:return "could not allocate memory";
        case JSON_ERR_END_OF_FILE:return "file already ended";
        case JSON_ERR_UNEXPECTED_CHAR:return "inexpected character";
        case JSON_ERR_INVALID_ESCAPE:return "invalid escape sequence";
        case JSON_ERR_INVALID_NUMBER:return "invalid number";
        case JSON_ERR_INVALID_NUMBER_PRES:return "invalid number float presission too big";
        case JSON_ERR_EXPECTED_COLON:return "expected ':";
        case JSON_ERR_EXPECTED_COMMA_OR_END:return "expected ',' or closing brakets";
        case JSON_ERR_INVALID_LITERRAL:return "invalid litteral (true/false/null)";
        case JSON_ERR_INVALID_NUMBER_INF:return "number too big";
        default: return "unkwon errors";
    }
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


static int string_reader(char* file_str,int* pos ,JSONC* current_node,enum STRING_t type ){
    char curr = file_str[*pos];
    int curr_size = 100;
    char* buffer = calloc(curr_size,sizeof(char));
    int i = -1;
    if (!buffer){
        return report_error(file_str, pos, JSON_ERR_ALLOC); 
    }

    while (curr != '"'){
        
        if(curr == '\0'){
            free(buffer);
            return report_error(file_str, pos, JSON_ERR_END_OF_FILE); 
        }
        if (curr == '\\'){
            curr = file_str[ ++ (*pos)];
            switch (curr) {
                case '"': buffer[++i] = '"';(*pos)++;break;
                case '\\': buffer[++i] = '\\' ;(*pos)++; break;
                case '/': buffer[++i] = '/';(*pos)++; break;
                case 'b': buffer[++i] = '\b';(*pos)++;break;
                case 'f': buffer[++i] = '\f';(*pos)++;break;
                case 'n': buffer[++i] = '\n';(*pos)++;break;
                case 'r': buffer[++i] = '\r';(*pos)++;break;
                case 't': buffer[++i] = '\t';(*pos)++;break;
                case 'u': //TODO \uXXXX
                        int temp = (*pos) + 5;
                        while(*pos <= temp ){
                            (*pos)++;
                            curr = file_str[*pos];
                            if (curr == '\0')return report_error(file_str, pos, JSON_ERR_END_OF_FILE);
                        }
                        break;
                default:
                        free(buffer);
                        return report_error(file_str,pos, JSON_ERR_UNEXPECTED_CHAR);
            }
            curr = file_str[(*pos)]; 
        }else {
            i++;
            buffer[i] = curr;
            (*pos)++;
            curr = file_str[*pos];
        }
        if (i>= curr_size-10){
            buffer = realloc(buffer, curr_size + 100);
            if(!buffer){
                return report_error(file_str, pos, JSON_ERR_ALLOC);
            }
        }


    }
    (*pos)++;
    
    if(type == NAME ){
        current_node->string = strdup(buffer);
    }else {
        current_node->type = STRING; 
        current_node->valuestring = strdup(buffer); 
    }

    free(buffer);
    return 0;
}


static bool is_digit(char curr){
    return curr >= '0' && curr <= '9';
}

static int number_reader(char* file_str,int* pos, JSONC* current_node){

    char curr = file_str[*pos];
    bool positive = true;
    bool positive_expo = true;
    uint8_t * number = calloc(MAX_INT_SIZE, sizeof(uint8_t));
    uint8_t* precision = calloc(MAX_FLOAT_PRES, sizeof(uint8_t));
    uint8_t* expo = calloc(MAX_EXPO, sizeof(uint8_t));
    if (!number || !precision || !expo) {
        free(number);free(precision);free(expo);
        return report_error(file_str, pos, JSON_ERR_ALLOC);
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
                return report_error(file_str,pos, JSON_ERR_INVALID_NUMBER);
            }
            number[top_num] = digit;
            curr = file_str[++(*pos)];
        }
    }else if (curr == '0') {
        curr = file_str[++(*pos)];
        if (is_digit(curr)) {
            free(number);free(precision);free(expo);
            return report_error(file_str,pos, JSON_ERR_INVALID_NUMBER);
        }
    }else {
        free(number);free(precision);free(expo);
        return report_error(file_str,pos, JSON_ERR_INVALID_NUMBER);
    }

    //FRACTION
    if (curr == '.') {
        curr = file_str[++(*pos)];
        if (!is_digit(curr)){
            free(number);free(precision);free(expo);
            return report_error(file_str, pos,JSON_ERR_INVALID_NUMBER);
        }
        while ( is_digit(curr)){
            uint8_t digit = (uint8_t)curr - (uint8_t)'0';
            precision[++top_pes] = digit;
            curr = file_str[++(*pos)];
        }
    }else if (curr == 'e' || curr == 'E' || is_white_space(curr)) {}
    else{
        goto number_fusion_and_allocation; 
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
            free(number);free(precision);free(expo);
            return report_error(file_str,pos,JSON_ERR_INVALID_NUMBER);
        }

        while ( is_digit(curr)){
            uint8_t digit = (uint8_t)curr - (uint8_t)'0';
            expo[++top_expo] = digit;
            curr = file_str[++(*pos)];
        }
    }
    goto number_fusion_and_allocation;

number_fusion_and_allocation :
    // Fuse the number
    double res = 0;
    
    for(int i = 0 ; i<=top_num;i++ ){
        res = (res * 10) + number[i];
    }
    if (top_pes > MAX_FLOAT_PRES){return report_error(file_str,pos, JSON_ERR_INVALID_NUMBER_PRES);} 
    double frac = 0.1; 
    for(int i = 0; i <= top_pes ;i++){
        res += precision[i] * frac;
        frac *= 0.1;
    }
    long e = 0;

    for (int i = 0;i<= top_expo;i++){
        e = (e * 10) + expo[i];
    }
    e = (positive_expo)? e : (-1*e);

    res = res * pow(10,e);

    res = (positive) ? res : (-1 * res);

    // JSON linking

    free(number);free(precision);free(expo);
    if(isinf(res)){
        return report_error(file_str,pos, JSON_ERR_INVALID_NUMBER_INF);
    } 
    current_node->type = NUMBER;
    current_node->valuedouble = res;
    return 0;

}


static int value_reader(char* file_str ,int* pos , JSONC* current_node){
    white_space_sanitizer(file_str,pos); 
    char curr = file_str[*pos];
    switch (curr) {
        case '"':
            (*pos)++;
            string_reader(file_str, pos, current_node,VALUE);
            break;
        case '0':case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8':case '9':case '-':
            number_reader(file_str, pos, current_node);
            break;
        case '{':{
            (*pos)++;
            current_node->type = OBJECT;
            if(file_str[(*pos)] == '}'){
                (*pos)++;break;
            }else {
                JSONC* child_o = calloc(1,sizeof(*child_o));
                current_node->child = child_o;
                object_reader(file_str, pos, child_o);
            }
            break;
        }
        case '[':{
            (*pos)++;
            current_node->type = ARRAY;
            if(file_str[(*pos)] == ']'){
                (*pos)++;break;
            }else {
                JSONC* child_a = calloc(1,sizeof(*child_a));
                current_node->child = child_a;
                array_reader(file_str, pos, child_a);
            }
            break;
        }
        case 'n':
            if(strncmp(file_str + *pos, "null",4) == 0){
               current_node->type =  NULL_e;
               current_node->valuedouble = 0;
               current_node->valuestring = NULL;
               *pos += 4;
//               if (file_str[*pos] == '\0')// goto null_terminator;
               break;
            }
        case 't':
            if(strncmp(file_str + *pos, "true",4) == 0){
               current_node->type =  BOOLEAN;
               current_node->valuedouble = 1;
               current_node->valuestring = "true";
               *pos += 4;
//               if (file_str[*pos] == '\0')// goto null_terminator;
               break;
            }
        case 'f':
            if(strncmp(file_str + *pos, "false",5) == 0){
               current_node->type =  BOOLEAN;
               current_node->valuedouble = 0;
               current_node->valuestring = "false";
               *pos += 5;
//               if (file_str[*pos] == '\0')// goto null_terminator;
               break;
            }
        default:
            return report_error(file_str,pos, JSON_ERR_INVALID_LITERRAL);

    }
    return 0;
}




static int array_reader(char* file_str, int* pos,JSONC* current_node){
    //curr is after '['
    white_space_sanitizer(file_str, pos);
    char curr = file_str[*pos];
    if (curr == '\0'){
        return report_error(file_str,pos, JSON_ERR_END_OF_FILE);
    }
    if(curr == ']'){
        (*pos)++; 
        return 0;
    }
    value_reader(file_str, pos, current_node);
    white_space_sanitizer(file_str, pos);
    
    
    curr = file_str[*pos];

    while(curr == ',' ){
        (*pos)++;
        white_space_sanitizer(file_str, pos);
        JSONC* next = calloc(1,sizeof(JSONC));
        current_node->next = next;
        next->prev =  current_node;

        value_reader(file_str, pos, next);
        white_space_sanitizer(file_str,pos);
        curr = file_str[*pos];
        current_node = next;
    }

    curr = file_str[*pos];
    if(curr != ']'){
        return report_error(file_str, pos, JSON_ERR_EXPECTED_COMMA_OR_END);;
    }
    (*pos)++;
    return 0;
}


static int object_reader(char* file_str,int* pos ,JSONC* current_node){
    white_space_sanitizer(file_str, pos);
    char curr = file_str[*pos];


    if(curr =='"'){
        (*pos)++;
        string_reader(file_str, pos, current_node, NAME);
        white_space_sanitizer(file_str, pos);
        curr = file_str[*pos];
        if(curr !=':') {
            return report_error(file_str,pos, JSON_ERR_EXPECTED_COLON);
        }
        (*pos)++;
        white_space_sanitizer(file_str, pos);
        value_reader(file_str,pos, current_node);
        white_space_sanitizer(file_str, pos);
    }else if (curr == '}') {current_node = NULL;}else {
        return report_error(file_str, pos,JSON_ERR_UNEXPECTED_CHAR);
    }

    curr = file_str[*pos];

    while(curr == ',' ){

        (*pos)++;
        white_space_sanitizer(file_str, pos);
        curr = file_str[*pos];

        if ( curr != '"'){
            return report_error(file_str, pos, JSON_ERR_UNEXPECTED_CHAR);
        }
        JSONC* next = calloc(1,sizeof(*next));
        current_node->next = next;
        next->prev = current_node;
        (*pos)++;

        string_reader(file_str, pos, next, NAME);
        white_space_sanitizer(file_str, pos);

        curr = file_str[*pos];
        if (curr != ':' ) {
            return report_error(file_str,pos, JSON_ERR_EXPECTED_COLON);
        }
        (*pos)++;

        white_space_sanitizer(file_str, pos);
        value_reader(file_str, pos, next);
        white_space_sanitizer(file_str,pos);        

        curr = file_str[*pos];
        current_node = next;
    }

    curr = file_str[*pos];
    if (curr != '}'){
        return JSON_ERR_EXPECTED_COMMA_OR_END;
    }
    (*pos)++;
    return 0;
}


static char utf8_encoder(char* file_str,int* pos, JSONC* current_node){
    char curr = file_str[*pos];
    
}

