#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <Windows.h>

#include "so_stdio.h"
#include "test_util.h"

#include "hooks.h"

int num_ReadFile;
HANDLE target_handle;

BOOL WINAPI hook_ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);

struct func_hook hooks[] = {
	{ "kernel32.dll", "ReadFile", (unsigned long)hook_ReadFile, 0 },
};


//this will declare buf[] and buf_len
#include "large_file.h"


BOOL WINAPI hook_ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
	BOOL (WINAPI *orig_ReadFile)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED) =
		(BOOL (WINAPI *)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED))hooks[0].orig_addr;

	if (hFile == target_handle)
		num_ReadFile++;

	return orig_ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
}


int main(int argc, char *argv[])
{
	SO_FILE *f;
	char *tmp;
	int ret;
	char *test_work_dir;
	char fpath[256];

	tmp = malloc(buf_len);
	FAIL_IF(!tmp, "malloc failed\n");

	install_hooks("so_stdio.dll", hooks, 1);

	if (argc == 2)
		test_work_dir = argv[1];
	else
		test_work_dir = "_test";

	sprintf(fpath, "%s/large_file", test_work_dir);

	ret = create_file_with_contents(fpath, buf, buf_len);
	FAIL_IF(ret != 0, "Couldn't create file: %s\n", fpath);


	/* --- BEGIN TEST --- */
	f = so_fopen(fpath, "r");
	FAIL_IF(!f, "Couldn't open file: %s\n", fpath);

	target_handle = so_fileno(f);

	num_ReadFile = 0;

	ret = so_fread(tmp, 1, 128, f);
	FAIL_IF(ret != 128, "Incorrect return value for so_fread: got %d, expected %d\n", ret, 128);
	FAIL_IF(num_ReadFile != 1, "Incorrect number of reads: got %d, expected %d\n", num_ReadFile, 1);
	FAIL_IF(memcmp(tmp, buf, 128), "Incorrect data\n");

	ret = so_fseek(f, 5000, SEEK_SET);
	FAIL_IF(ret != 0, "Incorrect return value for so_fseek: got %d, expected %d\n", ret, 0);

	ret = so_fread(tmp, 1, 128, f);
	FAIL_IF(ret != 128, "Incorrect return value for so_fread: got %d, expected %d\n", ret, 128);
	FAIL_IF(num_ReadFile < 2, "Incorrect number of reads: got %d, expected >= %d\n", num_ReadFile, 2);
	FAIL_IF(memcmp(tmp, &buf[5000], 128), "Incorrect data\n");

	ret = so_fseek(f, 5000, SEEK_SET);
	FAIL_IF(ret != 0, "Incorrect return value for so_fseek: got %d, expected %d\n", ret, 0);

	ret = so_fseek(f, -1000, SEEK_CUR);
	FAIL_IF(ret != 0, "Incorrect return value for so_fseek: got %d, expected %d\n", ret, 0);

	ret = so_fread(tmp, 1, 128, f);
	FAIL_IF(ret != 128, "Incorrect return value for so_fread: got %d, expected %d\n", ret, 128);
	FAIL_IF(memcmp(tmp, &buf[4000], 128), "Incorrect data\n");

	ret = so_fclose(f);
	FAIL_IF(ret != 0, "Incorrect return value for so_fclose: got %d, expected %d\n", ret, 0);

	free(tmp);

	return 0;
}
