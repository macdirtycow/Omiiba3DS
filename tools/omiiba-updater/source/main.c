#include <3ds.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define OMIIBA_BOOT_FIRM_URL "https://github.com/macdirtycow/Omiiba3DS/releases/latest/download/boot.firm"
#define TMP_PATH "sdmc:/omiiba/update/boot.firm.tmp"
#define BACKUP_PATH "sdmc:/omiiba/backups/boot.firm.bak"
#define BOOT_PATH "sdmc:/boot.firm"

static bool isRedirect(u32 statusCode)
{
    return (statusCode >= 301 && statusCode <= 303) || (statusCode >= 307 && statusCode <= 308);
}

static Result openRequest(httpcContext *context, const char **url, char *redirectUrl, size_t redirectUrlSize)
{
    Result ret;
    u32 statusCode = 0;

    do
    {
        ret = httpcOpenContext(context, HTTPC_METHOD_GET, *url, 1);
        if(R_FAILED(ret))
            return ret;

        // GitHub uses HTTPS. libctru homebrew commonly disables verification here.
        httpcSetSSLOpt(context, SSLCOPT_DisableVerify);
        httpcSetKeepAlive(context, HTTPC_KEEPALIVE_ENABLED);
        httpcAddRequestHeaderField(context, "User-Agent", "OmiibaUpdater/1.0");
        httpcAddRequestHeaderField(context, "Connection", "Keep-Alive");

        ret = httpcBeginRequest(context);
        if(R_FAILED(ret))
        {
            httpcCloseContext(context);
            return ret;
        }

        ret = httpcGetResponseStatusCode(context, &statusCode);
        if(R_FAILED(ret))
        {
            httpcCloseContext(context);
            return ret;
        }

        if(isRedirect(statusCode))
        {
            memset(redirectUrl, 0, redirectUrlSize);
            ret = httpcGetResponseHeader(context, "Location", redirectUrl, redirectUrlSize);
            httpcCloseContext(context);
            if(R_FAILED(ret))
                return ret;

            *url = redirectUrl;
            printf("Redirecting...\n");
        }
    }
    while(isRedirect(statusCode));

    if(statusCode != 200)
    {
        httpcCloseContext(context);
        printf("HTTP status: %" PRIu32 "\n", statusCode);
        return -2;
    }

    return 0;
}

static Result downloadToFile(const char *url, const char *path)
{
    Result ret;
    httpcContext context;
    char redirectUrl[0x1000];
    u8 *buffer = NULL;
    FILE *out = NULL;
    u32 totalSize = 0;

    printf("Downloading boot.firm...\n");

    ret = openRequest(&context, &url, redirectUrl, sizeof(redirectUrl));
    if(R_FAILED(ret))
        return ret;

    out = fopen(path, "wb");
    if(out == NULL)
    {
        httpcCloseContext(&context);
        return -3;
    }

    buffer = (u8 *)malloc(0x4000);
    if(buffer == NULL)
    {
        fclose(out);
        httpcCloseContext(&context);
        return -4;
    }

    while(true)
    {
        u32 readSize = 0;
        ret = httpcDownloadData(&context, buffer, 0x4000, &readSize);

        if(readSize > 0)
        {
            if(fwrite(buffer, 1, readSize, out) != readSize)
            {
                ret = -5;
                break;
            }
            totalSize += readSize;
            printf("\rDownloaded: %" PRIu32 " bytes", totalSize);
        }

        if(ret != (Result)HTTPC_RESULTCODE_DOWNLOADPENDING)
            break;
    }

    printf("\n");
    free(buffer);
    fclose(out);
    httpcCloseContext(&context);

    if(R_FAILED(ret))
        remove(path);

    return ret;
}

static Result installBootFirm(void)
{
    mkdir("sdmc:/omiiba", 0777);
    mkdir("sdmc:/omiiba/update", 0777);
    mkdir("sdmc:/omiiba/backups", 0777);

    remove(TMP_PATH);

    Result ret = downloadToFile(OMIIBA_BOOT_FIRM_URL, TMP_PATH);
    if(R_FAILED(ret))
        return ret;

    remove(BACKUP_PATH);
    rename(BOOT_PATH, BACKUP_PATH);

    if(rename(TMP_PATH, BOOT_PATH) != 0)
        return -6;

    return 0;
}

int main(void)
{
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    printf("Omiiba Updater\n");
    printf("==============\n\n");
    printf("Downloads latest boot.firm from:\n%s\n\n", OMIIBA_BOOT_FIRM_URL);
    printf("A: update SD:/boot.firm\n");
    printf("START: exit\n\n");

    Result ret = httpcInit(0);
    if(R_FAILED(ret))
        printf("httpcInit failed: 0x%08" PRIX32 "\n", (u32)ret);

    while(aptMainLoop())
    {
        gspWaitForVBlank();
        hidScanInput();

        u32 down = hidKeysDown();
        if(down & KEY_START)
            break;

        if((down & KEY_A) && R_SUCCEEDED(ret))
        {
            printf("Starting update...\n");
            Result installRet = installBootFirm();
            if(R_SUCCEEDED(installRet))
            {
                printf("\nUpdate installed.\n");
                printf("Backup: %s\n", BACKUP_PATH);
                printf("Reboot to use the new firmware.\n");
            }
            else
            {
                printf("\nUpdate failed: 0x%08" PRIX32 "\n", (u32)installRet);
                printf("Your previous boot.firm should still be available\n");
                printf("unless the failure happened during final rename.\n");
            }
            printf("\nSTART: exit\n");
        }
    }

    if(R_SUCCEEDED(ret))
        httpcExit();
    gfxExit();
    return 0;
}
