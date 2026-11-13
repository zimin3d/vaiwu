/* Host-check stub mirroring the vanilla cJSON API subset we use. */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
typedef struct cJSON {
    struct cJSON* next; struct cJSON* prev; struct cJSON* child;
    int type; char* valuestring; int valueint; double valuedouble; char* string;
} cJSON;
cJSON* cJSON_CreateObject(void);
cJSON* cJSON_AddStringToObject(cJSON* object, const char* name, const char* string);
cJSON* cJSON_AddNumberToObject(cJSON* object, const char* name, double number);
cJSON* cJSON_AddArrayToObject(cJSON* object, const char* name);
void cJSON_AddItemToArray(cJSON* array, cJSON* item);
char* cJSON_PrintUnformatted(const cJSON* item);
void cJSON_Delete(cJSON* item);
void cJSON_free(void* object);
cJSON* cJSON_Parse(const char* value);
const cJSON* cJSON_GetObjectItemCaseSensitive(const cJSON* object, const char* string);
int cJSON_IsObject(const cJSON* item);
int cJSON_IsArray(const cJSON* item);
int cJSON_IsNumber(const cJSON* item);
int cJSON_IsString(const cJSON* item);
int cJSON_GetArraySize(const cJSON* array);
cJSON* cJSON_GetArrayItem(const cJSON* array, int index);
#ifdef __cplusplus
}
#endif
