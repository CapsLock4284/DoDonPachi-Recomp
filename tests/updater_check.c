/**
 *    Name:        updater_check.c
 *    Description: the updater (ports/sdl2/updater.c) checked on Linux: a release's JSON read, the version compared,
 *                 a .zip and a .tar.gz unpacked, the files put in place and the program started again
 *                 (updater_install, updater_restart), and the .old files deleted at the next start
 *    Created:     18-11-2026
 *
 *    license:BSD-3-Clause
 *    Usage: updater_check RELEASE.json PACKAGE.zip PACKAGE.tar.gz FOLDER/   (FOLDER: an empty folder to update in)
 */
#include "../ports/sdl2/updater.c"

static int failures;

static void expect(bool ok, const char *what)
{
	printf("%s %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok)
		failures++;
}

static char *read_all(const char *path, size_t *size)
{
	FILE *file = fopen(path, "rb");
	char *data;
	long length;

	if (!file)
		return NULL;
	fseek(file, 0, SEEK_END);
	length = ftell(file);
	fseek(file, 0, SEEK_SET);
	data = malloc((size_t)length + 1u);
	*size = fread(data, 1, (size_t)length, file);
	data[*size] = '\0';
	fclose(file);
	return data;
}

int main(int argc, char **argv)
{
	size_t size;
	char *json, *package, path[1700];

	if (argc == 6)    /* (the program started again by updater_apply) */
	{
		char old[1700];

		printf("ok   started again: %s\n", argv[5]);
		updater_cleanup(argv[4]);
		snprintf(old, sizeof(old), "%sREADME.txt.old", argv[4]);
		expect(!exists(old), "the .old files deleted at the start");
		snprintf(old, sizeof(old), "%s" LIST_FILE, argv[4]);
		expect(!exists(old), "update_old.txt deleted");
		return failures != 0;
	}
	if (argc != 5)
		return 2;
	json = read_all(argv[1], &size);
	snprintf(suffix, sizeof(suffix), "-win64.zip");
	snprintf(version, sizeof(version), "0.8.2");
	read_release(json);
	expect(updater_state() == UPDATER_AVAILABLE && !strcmp(latest, "0.9.0"), "0.8.2 -> 0.9.0 available");
	expect(strstr(asset_url, "DonPachi-0.9.0-win64.zip") != NULL, asset_url);
	expect(!strcmp(asset_digest, "b6afe9d85e4fcd0348cf032106a0e9055008907666799f42daa27f55784d38f6"),
		"its own asset's SHA-256 (not the .tar.gz's before it)");
	{
		char digest[65];
		static const char *const known[3][2] = {
			{ "", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" },
			{ "abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" },
			{ "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
			  "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1" } };
		bool all = true;

		for (int index = 0; index < 3; index++)
		{
			sha256_hex((const unsigned char *)known[index][0], strlen(known[index][0]), digest);
			all = all && !strcmp(digest, known[index][1]);
		}
		expect(all, "SHA-256's test vectors");
	}
	snprintf(version, sizeof(version), "0.9.0");
	read_release(json);
	expect(updater_state() == UPDATER_UP_TO_DATE, "0.9.0 up to date");
	snprintf(version, sizeof(version), "0.10.0");
	read_release(json);
	expect(updater_state() == UPDATER_UP_TO_DATE, "0.10.0 newer than 0.9.0");
	snprintf(version, sizeof(version), "0.1.0");
	snprintf(suffix, sizeof(suffix), "-macos.zip");
	read_release(json);
	expect(updater_state() == UPDATER_FAILED, problem);
	expect(!safe_name("../x") && !safe_name("a/../x") && !safe_name("C:x") && !safe_name("/x") && safe_name("a..b/c"),
		"unsafe names refused");

	snprintf(folder, sizeof(folder), "%s", argv[4]);
	package = read_all(argv[2], &size);
	{
		char digest[65];

		sha256_hex((const unsigned char *)package, size, digest);
		expect(!strcmp(digest, "b6afe9d85e4fcd0348cf032106a0e9055008907666799f42daa27f55784d38f6"),
			"the downloaded .zip's SHA-256 is the release's");
	}
	expect(unpack((unsigned char *)package, size) && new_count == 7 && top_length == strlen("DonPachi-0.9.0-win64/"),
		"the .zip unpacked (7 files, top folder left off)");
	free(package);
	package = read_all(argv[3], &size);
	expect(unpack((unsigned char *)package, size) && new_count == 6
		&& top_length == strlen("DonPachi-0.9.0-linux-x64/"), "the .tar.gz unpacked (6 files)");
	free(package);
	local_path(path, sizeof(path), NEW_FOLDER "/", "DonPachi-0.9.0-linux-x64/donpachi");
	{
		struct stat info;

		expect(stat(path, &info) == 0 && (info.st_mode & 0100) && info.st_size == 7453072, "donpachi executable");
	}
	/* an older program in place, then the update */
	local_path(path, sizeof(path), "", "README.txt");
	fclose(fopen(path, "w"));
	SDL_AtomicSet(&state, UPDATER_READY);
	{
		char *again[] = { argv[0], argv[1], argv[2], argv[3], argv[4], "the updated program", NULL };

		expect(updater_install(), "the new files put in place");
		if (failures)
			return 1;
		fflush(stdout);
		updater_restart(again);    /* (Linux: the test itself started again, as the 'program') */
	}
	printf("FAIL not started again: %s\n", problem);
	return 1;
}
