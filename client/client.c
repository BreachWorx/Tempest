#include <windows.h>
#include <winhttp.h>
#include <wincred.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "advapi32.lib")

#define CREDENTIAL_TARGET_NAME "LegacyGeneric:target=MSIX-Skype for Desktop"

#define MAX_TOKEN_LEN 128
#define MAX_NAME_LEN 256
#define SERVER_HOST L"127.0.0.1"
#define SERVER_PORT 8000

#define HEARTBEAT_INTERVAL_MIN_SEC 30
#define HEARTBEAT_INTERVAL_MAX_SEC 90

// Context structure passed into the thread
typedef struct {
    char token[128];
    char computer_name[256];
    volatile BOOL is_running;
} HEARTBEAT_CONTEXT;

// Helper function to extract JSON values: {"device_token": "VALUE"}
BOOL parse_json_value(const char *json, const char *key, char *output, size_t max_len) {
    char search_key[128];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);

    char *key_pos = strstr(json, search_key);
    if (!key_pos) return FALSE;

    char *colon_pos = strchr(key_pos, ':');
    if (!colon_pos) return FALSE;

    char *first_quote = strchr(colon_pos, '\"');
    if (!first_quote) return FALSE;

    char *second_quote = strchr(first_quote + 1, '\"');
    if (!second_quote) return FALSE;

    size_t length = second_quote - (first_quote + 1);
    if (length >= max_len) length = max_len - 1;

    strncpy_s(output, max_len, first_quote + 1, length);
    output[length] = '\0';
    return TRUE;
}

void get_computer_name(char *buffer, DWORD size) {
    DWORD name_size = size;
    if (!GetComputerNameA(buffer, &name_size)) {
        strncpy_s(buffer, size, "DESKTOP-UNKNOWN", _TRUNCATE);
    }
}

// Dynamically fetches OS platform string from Windows Registry
void get_os_platform(char *buffer, size_t max_len) {
    HKEY hKey;
    DWORD data_type = 0;
    DWORD data_size = (DWORD)max_len;

    // Read ProductName from HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, 
                      "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 
                      0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
        
        if (RegQueryValueExA(hKey, "ProductName", NULL, &data_type, (LPBYTE)buffer, &data_size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return;
        }
        RegCloseKey(hKey);
    }

    // Fallback if registry read fails
    strncpy_s(buffer, max_len, "Windows", _TRUNCATE);
}

// Detect system CPU Architecture
void get_system_architecture(char *buffer, size_t size) {
    SYSTEM_INFO sysInfo;
    GetNativeSystemInfo(&sysInfo);

    switch (sysInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            strncpy_s(buffer, size, "x64", _TRUNCATE);
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            strncpy_s(buffer, size, "ARM64", _TRUNCATE);
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
            strncpy_s(buffer, size, "x86", _TRUNCATE);
            break;
        default:
            strncpy_s(buffer, size, "Unknown", _TRUNCATE);
            break;
    }
}

// FIRST RUN: Fetch device token from server endpoint with detailed payload
BOOL fetch_token_from_server(const char *computer_name, char *token_out, size_t max_token_len) {
    BOOL bSuccess = FALSE;
    HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;

    char arch[32] = {0};
    get_system_architecture(arch, sizeof(arch));

    char os_platform[128] = {0};
    get_os_platform(os_platform, sizeof(os_platform));

    const char *initial_status = "online";

    printf("[First Run] Token not found locally. Requesting token from server...\n");

    hSession = WinHttpOpen(L"Windows API Client/1.0",
                           WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return FALSE;

    hConnect = WinHttpConnect(hSession, SERVER_HOST, SERVER_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return FALSE;
    }

    hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/client/register",
                                  NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return FALSE;
    }

    LPCWSTR headers = L"Content-Type: application/json\r\n";
    WinHttpAddRequestHeaders(hRequest, headers, (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);

    // Registration Payload with extra details
    char json_payload[512];
    snprintf(json_payload, sizeof(json_payload),
             "{\n"
             "  \"computer_name\": \"%s\",\n"
             "  \"os_platform\": \"%s\",\n"
             "  \"architecture\": \"%s\",\n"
             "  \"status\": \"%s\"\n"
             "}",
             computer_name, os_platform, arch, initial_status);

    DWORD data_size = (DWORD)strlen(json_payload);

    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)json_payload, data_size, data_size, 0) &&
        WinHttpReceiveResponse(hRequest, NULL)) {

        DWORD status_code = 0;
        DWORD size = sizeof(status_code);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &size, WINHTTP_NO_HEADER_INDEX);

        if (status_code == 200 || status_code == 201) {
            char response_buffer[1024] = {0};
            DWORD bytes_read = 0;

            if (WinHttpReadData(hRequest, response_buffer, sizeof(response_buffer) - 1, &bytes_read) && bytes_read > 0) {
                if (parse_json_value(response_buffer, "device_token", token_out, max_token_len)) {
                    printf("[First Run] Received token from server: %s\n", token_out);
                    bSuccess = TRUE;
                }
            }
        } else {
            printf("[First Run Error] Server returned HTTP status: %lu\n", status_code);
        }
    }

    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);

    return bSuccess;
}

// Manages state: Load existing token OR fetch from server and save locally
BOOL get_or_register_device_token(char *token_buffer, size_t max_len, const char *computer_name) {
    PCREDENTIALA pcred = NULL;

    // 1. SUBSEQUENT RUNS: Check if token exists in Windows Credential Manager
    if (CredReadA(CREDENTIAL_TARGET_NAME, CRED_TYPE_GENERIC, 0, &pcred)) {
        size_t token_bytes = pcred->CredentialBlobSize;
        if (token_bytes >= max_len) token_bytes = max_len - 1;

        memcpy(token_buffer, pcred->CredentialBlob, token_bytes);
        token_buffer[token_bytes] = '\0';

        printf("[Credential Manager] Found stored token locally.\n");
        CredFree(pcred);
        return TRUE;
    }

    // 2. FIRST RUN: Request device token from server
    if (!fetch_token_from_server(computer_name, token_buffer, max_len)) {
        printf("[Error] Failed to retrieve device token from server.\n");
        return FALSE;
    }

    // 3. FIRST RUN: Save token to Windows Credential Manager
    CREDENTIALA cred = {0};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = CREDENTIAL_TARGET_NAME;
    cred.CredentialBlobSize = (DWORD)strlen(token_buffer);
    cred.CredentialBlob = (LPBYTE)token_buffer;
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    cred.UserName = "App Info";

    if (CredWriteA(&cred, 0)) {
        printf("[Credential Manager] Saved server-provided token securely to local store.\n");
    } else {
        printf("[Warning] Failed to write token to Credential Manager. Error: %lu\n", GetLastError());
    }

    return TRUE;
}

// Helper function to erase token from Windows Credential Manager
BOOL delete_stored_device_token(void) {
    if (CredDeleteA(CREDENTIAL_TARGET_NAME, CRED_TYPE_GENERIC, 0)) {
        printf("[Credential Manager] Erased invalid local token.\n");
        return TRUE;
    }
    return FALSE;
}

// Unified status update function compatible with both HTTP/2 and HTTP/1.1
// Returns: 1 = Success, 0 = Failure, -1 = Re-registration Required
int send_update_request(const char *token, const char *computer_name) {
    int result_status = 0;
    BOOL bResults = FALSE;
    HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;

    hSession = WinHttpOpen(L"Windows API Client/1.0",
                           WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return 0;

    hConnect = WinHttpConnect(hSession, SERVER_HOST, SERVER_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return 0;
    }

    hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/updatestatus",
                                  NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return 0;
    }

    // Enable HTTP/2 option (WinHTTP automatically falls back to HTTP/1.1 if unsupported/cleartext)
    DWORD http_version = WINHTTP_PROTOCOL_FLAG_HTTP2;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL, &http_version, sizeof(http_version));

    // Send Connection: keep-alive (Required for HTTP/1.1 persistent connections, ignored by HTTP/2)
    LPCWSTR headers = L"Content-Type: application/json\r\nConnection: keep-alive\r\n";
    WinHttpAddRequestHeaders(hRequest, headers, (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);

    char json_payload[512];
    snprintf(json_payload, sizeof(json_payload),
             "{\n"
             "  \"device_token\": \"%s\",\n"
             "  \"computer_name\": \"%s\",\n"
             "  \"status\": \"online\"\n"
             "}", token, computer_name);

    DWORD data_size = (DWORD)strlen(json_payload);

    bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, 
                                  (LPVOID)json_payload, data_size, data_size, 0);
    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }

    if (bResults) {
        // 1. Query HTTP Response Code
        DWORD status_code = 0;
        DWORD size = sizeof(status_code);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &size, WINHTTP_NO_HEADER_INDEX);

        // 2. Query Negotiated Protocol
        DWORD protocol_used = 0;
        DWORD proto_size = sizeof(protocol_used);
        WinHttpQueryOption(hRequest, WINHTTP_OPTION_HTTP_PROTOCOL_USED, &protocol_used, &proto_size);

        if (protocol_used & WINHTTP_PROTOCOL_FLAG_HTTP2) {
            printf("[HTTP/2 Update] Response Code: %lu\n", status_code);
        } else {
            printf("[HTTP/1.1 Update] Response Code: %lu\n", status_code);
        }

        // 3. Read Body Payload
        char response_buffer[1024] = {0};
        DWORD bytes_read = 0;
        WinHttpReadData(hRequest, response_buffer, sizeof(response_buffer) - 1, &bytes_read);

        // 4. Process Status & Execute Re-registration Signal Logic
        if (status_code == 404) {
            char action_value[64] = {0};
            if (bytes_read > 0) {
                parse_json_value(response_buffer, "action", action_value, sizeof(action_value));
            }

            if (strcmp(action_value, "register_required") == 0) {
                printf("[Warning] Server returned 404 (register_required). Invalidation detected!\n");
                result_status = -1; // Flag re-registration required
            } else {
                printf("[Error] Server returned 404: Unknown resource.\n");
                result_status = 0;
            }
        } else if (status_code == 200 || status_code == 204) {
            printf("[HTTP Update] Status successfully reported.\n");
            result_status = 1;
        } else {
            printf("[HTTP Update] Unexpected server response code: %lu\n", status_code);
            result_status = 0;
        }

    } else {
        printf("[Error] Update request failed. Win32 Error: %lu\n", GetLastError());
        result_status = 0;
    }

    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);

    return result_status;
}

// Helper to calculate a random interval between MIN and MAX seconds
DWORD get_random_heartbeat_interval_ms(void) {
    int range = HEARTBEAT_INTERVAL_MAX_SEC - HEARTBEAT_INTERVAL_MIN_SEC + 1;
    int seconds = HEARTBEAT_INTERVAL_MIN_SEC + (rand() % range);
    return (DWORD)(seconds * 1000); // Convert to milliseconds for Sleep()
}

// Background Worker Thread Function
DWORD WINAPI HeartbeatThreadProc(LPVOID lpParam) {
  HEARTBEAT_CONTEXT *ctx = (HEARTBEAT_CONTEXT*)lpParam;

  printf("[Heartbeat Worker] Thread started. Initializing periodic loop...\n");

  while (ctx->is_running) {
    printf("[Heartbeat Worker] Sending update to server...\n");

    // Execute unified status update request
    int status = send_update_request(ctx->token, ctx->computer_name);

    // Handle Server Invalidation Signal (Re-registration Required)
    if (status == -1) {
      printf("[Re-registration] Server requested re-registration. Invalidating local state...\n");

      // 1. Purge stale token from Windows Credential Manager
      delete_stored_device_token();

      // 2. Request a fresh token from the server
      char new_token[MAX_TOKEN_LEN] = {0};
      if (fetch_token_from_server(ctx->computer_name, new_token, sizeof(new_token))) {

        // 3. Persist the new token securely
        CREDENTIALA cred = {0};
        cred.Type = CRED_TYPE_GENERIC;
        cred.TargetName = CREDENTIAL_TARGET_NAME;
        cred.CredentialBlobSize = (DWORD)strlen(new_token);
        cred.CredentialBlob = (LPBYTE)new_token;
        cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
        cred.UserName = "App Info";

        if (CredWriteA(&cred, 0)) {
          printf("[Credential Manager] New device token stored successfully.\n");
        } else {
          printf("[Warning] Failed to write new token to Credential Manager. Error: %lu\n", GetLastError());
        }

        // 4. Update the active thread context token in-memory
        strncpy_s(ctx->token, sizeof(ctx->token), new_token, _TRUNCATE);
        printf("[Re-registration] Completed successfully. Active Token: %s\n", ctx->token);

      } else {
        printf("[Re-registration Error] Failed to obtain new token. Will retry next cycle.\n");
      }
    }

    // Calculate jittered delay (30 to 90 seconds)
    DWORD delay_ms = get_random_heartbeat_interval_ms();
    printf("[Heartbeat Worker] Next heartbeat in %lu seconds...\n", delay_ms / 1000);

    // Responsive sleep loop (1-second intervals for fast thread termination)
    DWORD elapsed = 0;
    while (elapsed < delay_ms && ctx->is_running) {
      Sleep(1000);
      elapsed += 1000;
    }
  }

  printf("[Heartbeat Worker] Thread shutting down cleanly.\n");
  return 0;
}

int main(void) {

    // Seed random number generator for variable interval timing
    srand((unsigned int)time(NULL));

    char device_token[MAX_TOKEN_LEN] = {0};
    char computer_name[MAX_NAME_LEN] = {0};

    get_computer_name(computer_name, sizeof(computer_name));
    printf("Computer Name: %s\n\n", computer_name);

    // Get token (Credential Manager on subsequent runs; Server on first run)
    if (!get_or_register_device_token(device_token, sizeof(device_token), computer_name)) {
        printf("[Fatal] Unable to obtain token. Program terminating.\n");
        return 1;
    }

    printf("Active Token  : %s\n\n", device_token);

    // Initialize Thread Context
    HEARTBEAT_CONTEXT heartbeat_ctx = {0};
    strncpy_s(heartbeat_ctx.token, sizeof(heartbeat_ctx.token), device_token, _TRUNCATE);
    strncpy_s(heartbeat_ctx.computer_name, sizeof(heartbeat_ctx.computer_name), computer_name, _TRUNCATE);
    heartbeat_ctx.is_running = TRUE;

    // Launch Background Thread
    HANDLE hThread = CreateThread(
        NULL,                   // Default security attributes
        0,                      // Default stack size
        HeartbeatThreadProc,    // Thread function
        &heartbeat_ctx,         // Context argument
        0,                      // Default creation flags
        NULL                    // Returns thread identifier
    );

    if (hThread == NULL) {
        printf("[Fatal] Failed to create heartbeat thread. Error: %lu\n", GetLastError());
        return 1;
    }

    printf("====================================================\n");
    printf(" Client Running. Press ENTER in this console to exit.\n");
    printf("====================================================\n\n");

    // Main thread remains free while background thread manages heartbeats
    getchar();

    // Clean Thread Shutdown Protocol
    printf("[Main] Stop signal sent to worker thread...\n");
    heartbeat_ctx.is_running = FALSE;

    // Wait up to 3 seconds for the thread to exit gracefully
    WaitForSingleObject(hThread, 3000);
    CloseHandle(hThread);

    printf("[Main] Application exiting.\n");

    return 0;
}
