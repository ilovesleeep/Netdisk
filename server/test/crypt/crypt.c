#include <crypt.h>
#include <stdio.h>

int main(void) {
    char str[] = "qwer1234";
    char salt[] = "cctv1";

    char* encrypted_msg = crypt(str, salt);
    puts(encrypted_msg);

    return 0;
}
