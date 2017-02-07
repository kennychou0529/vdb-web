#define MBEDTLS_SHA1_C
#include "sha1.c" // @ replace https://tools.ietf.org/html/rfc3174#page-10

size_t vdb_extract_user_key(const char *request, size_t request_len, char *key);

int vdb_generate_accept_key(const char *request, size_t request_len, char *accept_key)
{
    // The WebSocket standard has defined that the server must respond to a connection
    // request with a hash key that is generated from the user's key. This code implements
    // the stuff in https://tools.ietf.org/html/rfc6455#section-1.3
    char user_key[1024] = {0};
    unsigned char new_key[1024] = {0};
    size_t user_len = 0;
    size_t new_len = 0;

    user_len = vdb_extract_user_key(request, request_len, user_key);
    vdb_assert(user_len > 0);

    // Concatenate salt and user keys
    {
        const char *salt = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        size_t salt_len = strlen(salt);
        while (new_len < user_len)
            new_key[new_len++] = (unsigned char)user_key[new_len];
        while (new_len-user_len < salt_len)
            new_key[new_len++] = (unsigned char)salt[new_len-user_len];
    }

    // Compute the accept-key
    {
        // Compute sha1 hash
        unsigned char sha1[20];
        mbedtls_sha1(new_key, new_len, sha1);

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

size_t vdb_extract_user_key(const char *request, size_t request_len, char *key)
{
    // The user request contains this string somewhere in it:
    // "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n", This
    // code extracts the "dGhlIHNhbXBsZSBub25jZQ==" part.
    size_t i,j;
    size_t key_len = 0;
    const char *pattern = "Sec-WebSocket-Key:";
    size_t pattern_len = strlen(pattern);
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
