// websocket.c - Version 1 - Websocket utilities

// interface
typedef struct
{
    char *payload;
    int length;
    int fin;
    int opcode;
} vdb_msg_t;
int vdb_generate_handshake(const char *request, int request_len, char **out_response, int *out_length);
int vdb_self_test();
void vdb_form_frame(int length, unsigned char **out_frame, int *out_length);
int vdb_parse_message(void *recv_buffer, int received, vdb_msg_t *msg);

// implementation
#define MBEDTLS_SHA1_C
#include "sha1.c" // @ replace https://tools.ietf.org/html/rfc3174#page-10

int vdb_extract_user_key(const char *request, int request_len, char *key)
{
    // The user request contains this string somewhere in it:
    // "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n", This
    // code extracts the "dGhlIHNhbXBsZSBub25jZQ==" part.
    int i,j;
    int key_len = 0;
    const char *pattern = "Sec-WebSocket-Key:";
    int pattern_len = (int)strlen(pattern);
    vdb_assert(request_len >= pattern_len);
    for (i = 0; i < request_len-pattern_len; i++)
    {
        int is_equal = 1;
        for (j = 0; j < pattern_len; j++)
        {
            if (request[i+j] != pattern[j])
                is_equal = 0;
        }
        if (is_equal)
        {
            const char *src = request + i + pattern_len + 1;
            while (*src && *src != '\r')
            {
                key[key_len++] = *src;
                src++;
            }
        }
    }
    return key_len;
}

int vdb_generate_accept_key(const char *request, int request_len, char *accept_key)
{
    // The WebSocket standard has defined that the server must respond to a connection
    // request with a hash key that is generated from the user's key. This code implements
    // the stuff in https://tools.ietf.org/html/rfc6455#section-1.3
    char user_key[1024] = {0};
    unsigned char new_key[1024] = {0};
    int user_len = 0;
    int new_len = 0;

    user_len = vdb_extract_user_key(request, request_len, user_key);
    vdb_assert(user_len > 0);

    // Concatenate salt and user keys
    {
        const char *salt = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        int salt_len = (int)strlen(salt);
        for (new_len = 0; new_len < user_len; new_len++)
            new_key[new_len] = (unsigned char)user_key[new_len];
        for (; new_len < user_len + salt_len; new_len++)
            new_key[new_len] = (unsigned char)salt[new_len-user_len];
    }

    // Compute the accept-key
    {
        // Compute sha1 hash
        unsigned char sha1[20];
        mbedtls_sha1(new_key, (size_t)new_len, sha1);

        // Convert to base64 null-terminated string
        {
            const char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            unsigned char x1, x2, x3;
            int i;
            for (i = 2; i <= 20; i += 3)
            {
                x3 = (i < 20) ? sha1[i] : 0;
                x2 = sha1[i-1];
                x1 = sha1[i-2];
                *accept_key++ = b64[((x1 >> 2) & 63)];
                *accept_key++ = b64[((x1 &  3) << 4) | ((x2 >> 4) & 15)];
                *accept_key++ = b64[((x2 & 15) << 2) | ((x3 >> 6) & 3)];
                *accept_key++ = (i < 20) ? b64[((x3 >> 0) & 63)] : '=';
            }
            *accept_key = 0;
        }
    }

    return 1;
}

int vdb_generate_handshake(const char *request, int request_len, char **out_response, int *out_length)
{
    const char *header1 =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: ";
    const char *header2 = "\r\n\r\n";
    char accept_key[1024];
    char response[1024];
    int response_len = 0;
    size_t i = 0;

    vdb_assert(vdb_generate_accept_key(request, request_len, accept_key));
    for (i = 0; i < strlen(header1); i++)    response[response_len++] = header1[i];
    for (i = 0; i < strlen(accept_key); i++) response[response_len++] = accept_key[i];
    for (i = 0; i < strlen(header2); i++)    response[response_len++] = header2[i];

    *out_response = response;
    *out_length = response_len;
    return 1;
}

int vdb_self_test()
{
    int request_len;
    char accept_key[1024];
    const char *request =
        "GET /chat HTTP/1.1\r\n"
        "Host: server.example.com\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Protocol: chat, superchat\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Origin: http://example.com\r\n\r\n";
    request_len = (int)strlen(request);
    vdb_generate_accept_key(request, request_len, accept_key);
    vdb_assert(strcmp(accept_key, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") == 0);
    return 1;
}

void vdb_print_bytes(void *recv_buffer, int n)
{
    int index;
    for (index = 0; index < n; index++)
    {
        unsigned char c = ((unsigned char*)recv_buffer)[index];
        int i;
        printf("%d:\t", index);
        for (i = 7; i >= 4; i--) printf("%d", (c >> i) & 1);
        printf(" ");
        for (i = 3; i >= 0; i--) printf("%d", (c >> i) & 1);
        printf("\n");
    }
}

void vdb_form_frame(int length, unsigned char **out_frame, int *out_length)
{
    static unsigned char frame[16] = {0};
    int frame_length = 0;
    {
        // fin rsv1 rsv2 rsv3 opcode
        // 1   0    0    0    0010
        frame[0] = 0x82;
        if (length <= 125)
        {
            frame[1] = (unsigned char)(length & 0xFF);
            frame_length = 2;
        }
        else if (length <= 65535)
        {
            frame[1] = 126;
            frame[2] = (unsigned char)((length >> 8) & 0xFF);
            frame[3] = (unsigned char)((length >> 0) & 0xFF);
            frame_length = 4;
        }
        else
        {
            frame[1] = 127;
            frame[2] = 0; // @ assuming length is max 32 bit
            frame[3] = 0; // @ assuming length is max 32 bit
            frame[4] = 0; // @ assuming length is max 32 bit
            frame[5] = 0; // @ assuming length is max 32 bit
            frame[6] = (length >> 24) & 0xFF;
            frame[7] = (length >> 16) & 0xFF;
            frame[8] = (length >>  8) & 0xFF;
            frame[9] = (length >>  0) & 0xFF;
            frame_length = 10;
        }
    }
    *out_frame = frame;
    *out_length = frame_length;
}

int vdb_parse_message(void *recv_buffer, int received, vdb_msg_t *msg)
{
    // https://tools.ietf.org/html/rfc6455#section-5.4
    // Note: WebSocket does not send fields unless
    // they are needed. For example, extended len
    // is not sent if the len fits inside payload
    // len.
    uint32_t opcode;
    uint32_t fin;
    uint64_t len;
    uint32_t mask;
    unsigned char key[4] = {0};
    unsigned char *frame = (unsigned char*)recv_buffer;
    int i = 0;

    // extract header
    vdb_assert(i + 2 <= received);
    opcode = ((frame[i  ] >> 0) & 0xF);
    fin    = ((frame[i++] >> 7) & 0x1);
    len    = ((frame[i  ] >> 0) & 0x7F);
    mask   = ((frame[i++] >> 7) & 0x1);

    // client messages must be masked according to spec
    vdb_assert(mask == 1);

    // extract payload length in number of bytes
    if (len == 126)
    {
        vdb_assert(i + 2 <= received);
        len = 0;
        len |= frame[i++]; len <<= 8;
        len |= frame[i++];
    }
    else if (len == 127)
    {
        vdb_assert(i + 8 <= received);
        len = 0;
        len |= frame[i++]; len <<= 8;
        len |= frame[i++]; len <<= 8;
        len |= frame[i++]; len <<= 8;
        len |= frame[i++]; len <<= 8;
        len |= frame[i++]; len <<= 8;
        len |= frame[i++]; len <<= 8;
        len |= frame[i++]; len <<= 8;
        len |= frame[i++];
    }

    // verify that we read the length correctly
    vdb_assert(len <= (uint64_t)received);

    // extract key used to decode payload
    {
        vdb_assert(i + 4 <= received);
        key[0] = frame[i++];
        key[1] = frame[i++];
        key[2] = frame[i++];
        key[3] = frame[i++];
    }

    // decode payload
    {
        int j = 0;
        vdb_assert(i + (int)len <= received);
        for (j = 0; j < (int)len; j++)
            frame[i+j] = frame[i+j] ^ key[j % 4];
        frame[i+len] = 0;
    }

    msg->payload = (char*)(frame + i);
    msg->length = (int)len;
    msg->opcode = (int)opcode;
    msg->fin = (int)fin;
    return 1;
}
