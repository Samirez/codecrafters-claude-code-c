#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include "cjson/cJSON.h"

struct response_buf {
    char *data;
    size_t size;
};

static size_t curl_write_response(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total = size * nmemb;
    struct response_buf *buf = (struct response_buf *)userp;
    char *tmp = realloc(buf->data, buf->size + total + 1);
    if (!tmp) {
        return 0;
    }
    buf->data = tmp;
    memcpy(buf->data + buf->size, contents, total);
    buf->size += total;
    buf->data[buf->size] = '\0';
    return total;
}

int main(int argc, char *argv[]) {
    const char *prompt = NULL;

    if (getopt(argc, argv, "p:") == 'p') {
        prompt = optarg;
    }

    if (!prompt) {
        fprintf(stderr, "error: -p flag is required\n");
        return 1;
    }

    const char *api_key = getenv("OPENROUTER_API_KEY");
    const char *base_url = getenv("OPENROUTER_BASE_URL");

    if (!base_url || !*base_url) {
        base_url = "https://openrouter.ai/api/v1";
    }

    if (!api_key || !*api_key) {
        fprintf(stderr, "OPENROUTER_API_KEY is not set\n");
        return 1;
    }

    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "model", "anthropic/claude-haiku-4.5");
    cJSON *messages = cJSON_AddArrayToObject(req, "messages");
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "role", "user");
    cJSON_AddStringToObject(msg, "content", prompt);
    cJSON_AddItemToArray(messages, msg);

    // add tools to request
    cJSON *tools = cJSON_CreateArray();
    cJSON *tool = cJSON_CreateObject();
    cJSON_AddStringToObject(tool, "type", "function");
    
    cJSON *function = cJSON_CreateObject();
    cJSON_AddStringToObject(function, "name", "Read");
    cJSON_AddStringToObject(function, "description", "Read and return the contents of a file");

    cJSON *parameters = cJSON_CreateObject();
    cJSON_AddStringToObject(parameters, "type", "object");

    cJSON *properties = cJSON_CreateObject();
    cJSON *file_path = cJSON_CreateObject();
    cJSON_AddStringToObject(file_path, "type", "string");
    cJSON_AddStringToObject(file_path, "description", "The path to the file to read");
    cJSON_AddItemToObject(properties, "file_path", file_path);

    cJSON *required = cJSON_CreateStringArray((const char *[]){"file_path"}, 1);
    cJSON_AddItemToObject(parameters, "properties", properties);
    cJSON_AddItemToObject(parameters, "required", required);

    cJSON_AddItemToObject(function, "parameters", parameters);
    cJSON_AddItemToObject(tool, "function", function);
    cJSON_AddItemToArray(tools, tool);

    // add write tool to enable LLM to content to files if needed in the future
    cJSON *write_tool = cJSON_CreateObject();
    cJSON_AddStringToObject(write_tool, "type", "function");
    cJSON *write_function = cJSON_CreateObject();
    cJSON_AddStringToObject(write_function, "name", "Write");
    cJSON_AddStringToObject(write_function, "description", "Write content to a file");
    cJSON *write_parameters = cJSON_CreateObject();
    cJSON_AddStringToObject(write_parameters, "type", "object");
    cJSON *write_properties = cJSON_CreateObject();
    cJSON *write_file_path = cJSON_CreateObject();
    cJSON_AddStringToObject(write_file_path, "type", "string");
    cJSON_AddStringToObject(write_file_path, "description", "The path to the file to write to");
    cJSON *content = cJSON_CreateObject();
    cJSON_AddStringToObject(content, "type", "string");
    cJSON_AddStringToObject(content, "description", "The content to write to the file");
    cJSON_AddItemToObject(write_properties, "file_path", write_file_path);
    cJSON_AddItemToObject(write_properties, "content", content);
    cJSON *write_required = cJSON_CreateStringArray((const char *[]){"file_path", "content"}, 2);
    cJSON_AddItemToObject(write_parameters, "properties", write_properties);
    cJSON_AddItemToObject(write_parameters, "required", write_required);
    cJSON_AddItemToObject(write_function, "parameters", write_parameters);
    cJSON_AddItemToObject(write_tool, "function", write_function);
    cJSON_AddItemToArray(tools, write_tool);

    char *body = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    
    // create conversation history
    cJSON *conversation_history = cJSON_CreateObject();
    cJSON *history_messages = cJSON_AddArrayToObject(conversation_history, "messages");
    cJSON *user_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(user_msg, "role", "user");
    cJSON_AddStringToObject(user_msg, "content", prompt);
    cJSON_AddItemToArray(history_messages, user_msg);

    char url[512];
    snprintf(url, sizeof(url), "%s/chat/completions", base_url);

    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", api_key);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl = curl_easy_init();

    while (1) {
        cJSON *req = cJSON_CreateObject();
        cJSON_AddStringToObject(req, "model", "anthropic/claude-haiku-4.5");
        cJSON *req_messages = cJSON_AddArrayToObject(req, "messages");
        cJSON *msg_history = cJSON_GetObjectItem(conversation_history, "messages");

        if (msg_history && cJSON_IsArray(msg_history)) {
            cJSON *child = msg_history->child;
            while (child) {
                cJSON_AddItemReferenceToArray(req_messages, child);
                child = child->next;
            }
        }

        cJSON *tools_copy = cJSON_Duplicate(tools, 1);
        cJSON_AddItemToObject(req, "tools", tools_copy);
        char *body = cJSON_PrintUnformatted(req);
        struct response_buf resp = {NULL, 0};
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, auth_header);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_response);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        free(body);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            free(resp.data);
            cJSON_Delete(req);
            cJSON_Delete(conversation_history);
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            return 1;
        }

        cJSON *json = cJSON_Parse(resp.data);
        free(resp.data);

        if (!json) {
            fprintf(stderr, "Failed to parse response JSON\n");
            cJSON_Delete(req);
            cJSON_Delete(conversation_history);
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            return 1;
        }

        cJSON_Delete(req);

        // add response to conversation history
        cJSON *choices = cJSON_GetObjectItem(json, "choices");

        if (!cJSON_IsArray(choices) || cJSON_GetArraySize(choices) == 0) {
            fprintf(stderr, "no choices in response\n");
            cJSON_Delete(json);
            cJSON_Delete(conversation_history);
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            return 1;
        }

        cJSON *first = cJSON_GetArrayItem(choices, 0);
        cJSON *message = cJSON_GetObjectItem(first, "message");
        cJSON *tool_calls = cJSON_GetObjectItem(message, "tool_calls");

        cJSON *assistant_msg = cJSON_CreateObject();
        cJSON_AddStringToObject(assistant_msg, "role", "assistant");
        cJSON *content = cJSON_GetObjectItem(message, "content");

        if (content) {
            cJSON_AddStringToObject(assistant_msg, "content", cJSON_GetStringValue(content));
        }

        if (tool_calls) {
            cJSON *tool_calls_copy = cJSON_Duplicate(tool_calls, 1);
            cJSON_AddItemToObject(assistant_msg, "tool_calls", tool_calls_copy);
        }

        cJSON_AddItemToArray(history_messages, assistant_msg);

        if (cJSON_IsArray(tool_calls) && cJSON_GetArraySize(tool_calls) > 0) {
            int num_calls = cJSON_GetArraySize(tool_calls);
            for (int i = 0; i < num_calls; i++) {
                cJSON *tool_call = cJSON_GetArrayItem(tool_calls, i);
                cJSON *tool_call_function = cJSON_GetObjectItem(tool_call, "function");
                const char *function_name = cJSON_GetStringValue(
                    cJSON_GetObjectItem(tool_call_function, "name"));
                const char *args_str = cJSON_GetStringValue(
                    cJSON_GetObjectItem(tool_call_function, "arguments"));
                const char *tool_call_id =
                    cJSON_GetStringValue(cJSON_GetObjectItem(tool_call, "id"));

                if (function_name && strcmp(function_name, "Read") == 0 && args_str) {
                    cJSON *args = cJSON_Parse(args_str);
                    const char *file_path =
                        cJSON_GetStringValue(cJSON_GetObjectItem(args, "file_path"));

                    if (file_path) {
                        FILE *file = fopen(file_path, "rb");

                        if (!file) {
                            fprintf(stderr, "Read: cannot open file: %s\n", file_path);
                        } else {
                            fseek(file, 0, SEEK_END);
                            long fsize = ftell(file);
                            fseek(file, 0, SEEK_SET);
                            char *fbuf = malloc(fsize + 1);
                            fread(fbuf, 1, fsize, file);
                            fclose(file);
                            fbuf[fsize] = '\0';
                            cJSON *tool_response = cJSON_CreateObject();
                            cJSON_AddStringToObject(tool_response, "role", "tool");
                            cJSON_AddStringToObject(tool_response, "tool_call_id", tool_call_id);
                            cJSON_AddStringToObject(tool_response, "content", fbuf);
                            cJSON_AddItemToArray(history_messages, tool_response);
                            free(fbuf);
                        }
                    }

                    cJSON_Delete(args);
                }
            }

            cJSON_Delete(json);
        } else {
            if (content) {
                printf("%s\n", cJSON_GetStringValue(content));
            }
            cJSON_Delete(json);
            break;
        }
    }

    fprintf(stderr, "Logs from your program will appear here!\n");
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    cJSON_Delete(conversation_history);
    return 0;
}
