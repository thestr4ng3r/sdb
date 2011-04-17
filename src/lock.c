#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static char buf[128];

const char *sdb_lockfile(const char *f) {
	int len = strlen (f);
	if (len+10>sizeof buf)
		return NULL;
	memcpy (buf, f, len);
	strcpy (buf+len, ".lock");
	return buf;
}

int sdb_lock(const char *s) {
	int ret;
	if (!s) return 0;
	ret = open (s, O_CREAT | O_TRUNC | O_WRONLY | O_EXCL, 0644);
	if (ret!=-1) {
		close (ret);
		return 1;
	}
	return 0;
}

void sdb_unlock(const char *s) {
	unlink (s);
}

#if TEST
main () {
	int r;
	r = sdb_lock (".lock");
	printf ("%d\n", r);
	r = sdb_lock (".lock");
	printf ("%d\n", r);
	sdb_unlock (".lock");
	r = sdb_lock (".lock");
	printf ("%d\n", r);
}
#endif
