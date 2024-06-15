#include <func.h>
#include <shadow.h>

void help(void)
{
    printf("用户密码验证程序\n 第一个参数为用户名!\n");
    exit(-1);
}

void error_quit(char *msg)
{
    perror(msg);
    exit(-2);
}

void get_setting(char *salt,char *passwd)
{
    int i,j;
    //取出salt,i 记录密码字符下标，j记录$出现次数
    for(i = 0,j = 0;passwd[i]&& j!=4;++i)
    {
        if(passwd[i] == '$')
            ++j;
    }
    strncpy(salt,passwd,i);
}

int main(int argc,char *argv[])
{
    struct spwd *sp;
    char *passwd;
    char setting[512] = {0};
    if(argc != 2)
        help();
    passwd = getpass("请输入密码:");
    printf("passwd:%s\n", passwd);

    if((sp = getspnam(argv[1]))==NULL)
    {
        error_quit("获取用户名和密码");
    }

    get_setting(setting,sp->sp_pwdp);
    char * encoded = crypt(passwd, setting);
    printf("encrypted: %s\n", encoded);

    if(strcmp(sp->sp_pwdp, encoded)==0)
        printf("验证通过!\n");
    else
        printf("验证失败!\n");
    return 0;
}

