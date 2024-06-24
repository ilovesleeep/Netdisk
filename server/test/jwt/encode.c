#include <l8w8jwt/encode.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    char* jwt;
    size_t jwt_length;

    struct l8w8jwt_encoding_params params;
    l8w8jwt_encoding_params_init(&params);

    params.alg = L8W8JWT_ALG_HS512;

    params.sub = "Gordon Freeman";
    params.iss = "Black Mesa";
    params.aud = "Administrator";

    params.iat = l8w8jwt_time(NULL);
    params.exp = l8w8jwt_time(NULL) +
                 600; /* Set to expire after 10 minutes (600 seconds). */

    params.secret_key = (unsigned char*)"YoUR sUpEr S3krEt 1337 HMAC kEy HeRE";
    params.secret_key_length = strlen(params.secret_key);

    params.out = &jwt;
    params.out_length = &jwt_length;

    int r = l8w8jwt_encode(&params);

    printf("\n l8w8jwt example HS512 token: %s \n",
           r == L8W8JWT_SUCCESS ? jwt : " (encoding failure) ");
    printf("len: %ld\n", strlen(jwt));

    /* Always free the output jwt string! */
    l8w8jwt_free(jwt);

    return 0;
}
