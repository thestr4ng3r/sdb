/* Copyleft 2011 - sdb (aka SimpleDB) - pancake<nopcode.org> */
#include "memcache.h"

static int whileread (int fd, char *b, int len) {
	int all = 0;
	int ret;
	for (;len>0;) {
		ret = fread (b, 1, len, stdin);
		if (ret==-1) return -1;
		if (ret>0) {
			b += ret;
			all += ret;
			len -= ret;
		}
	}
	return all;
}

static void handle_get(MemcacheSdb *ms, char *key, int smode) {
	ut64 exptime = 0LL;
	int n = 0;
	char *k, *K = key;
	if (!key) {
		printf ("ERROR\r\n");
		return;
	}
	for (;;) {
		k = strchr (K, ' ');
		if (k) *k=0;
		// TODO: split key with spaces
		char *s = memcache_get (ms, K, &exptime);
		if (s) {
			if (smode) printf ("VALUE %s %llu 0 %d\r\n", K, exptime, (int)strlen (s));
			else printf ("VALUE %s %llu %d\r\n", K, exptime, (int)strlen (s));
			printf ("%s\r\nEND\r\n", s);
			n++;
			free (s);
		}
		if (k) K = k+1;
		else break;
	}
	if (!n) printf ("END\r\n");
}

int protocol_handle (char *buf) {
	char *p, *cmd = buf, *key, tmp[256], tmp2[256];
	int flags = 0, bytes = 0;
	ut64 exptime = 0LL;

	if (!*buf) {
		printf ("ERROR\r\n");
		return 0;
	}
	p = strchr (buf, ' ');
	if (p) {
		*p = 0;
		key = p + 1;
	}
	if (!strcmp (cmd, "quit")) {
		return -1;
	} else
	if (!strncmp (buf, "gets ", 5)) {
		handle_get (ms, key, 1);
	} else
	if (	!strcmp (cmd, "incr") ||
		!strcmp (cmd, "decr")
			) {
		char *ret;
		ut64 n = 0;
		p = strchr (key, ' ');
		if (!p) {
			return 0;
		}
		*p++ = 0;
		sscanf (p, "%llu", &n);
		if (*cmd=='i') ret = memcache_incr (ms, key, n);
		else ret = memcache_decr (ms, key, n);
		if (ret) printf ("%s\r\n", ret);
		else printf ("SERVER_ERROR numeric overflow\r\n");
		free (ret);
	} else
	if (!strcmp (cmd, "stats")) {
		struct rusage ru;
		getrusage (0, &ru);
		printf ("STAT pid %d\r\n", getpid ());
		printf ("STAT uptime %llu\r\n", sdb_now ()-ms->time);
		printf ("STAT time %llu\r\n", sdb_now ());
		printf ("STAT version "MEMCACHE_VERSION"\r\n");
		printf ("STAT pointer_size %u\r\n", (int)sizeof (void*)*8);
		printf ("STAT rusage_user %u.%u\r\n",
			(ut32)ru.ru_utime.tv_sec, (ut32)ru.ru_utime.tv_usec);
		printf ("STAT rusage_system %u.%u\r\n",
			(ut32)ru.ru_stime.tv_sec, (ut32)ru.ru_stime.tv_usec);
		printf ("STAT cmd_get %llu\r\n", ms->gets);
		printf ("STAT cmd_set %llu\r\n", ms->sets);
		printf ("STAT get_hits %llu\r\n", ms->hits);
		printf ("STAT get_misses %llu\r\n", ms->misses);
		printf ("STAT evictions %llu\r\n", ms->evictions);
		printf ("STAT bytes_read %llu\r\n", ms->bread);
		printf ("STAT bytes_written %llu\r\n", ms->bwrite);
		// ?? printf ("STAT limit_maxbytes 0\r\n");
		printf ("STAT threads 1\r\n");
		printf ("END\r\n");
	} else
	if (!strcmp (cmd, "version")) {
		printf ("VERSION 0.1\r\n");
	} else
	if (!strcmp (cmd, "get")) {
		handle_get (ms, key, 0);
	} else
	if (!strcmp (cmd, "delete")) {
		p = strchr (key, ' ');
		if (p) {
			*p = 0;
			exptime = 0LL;
			sscanf (p+1, "%llu", &exptime);
			if (memcache_delete (ms, key, exptime))
				printf ("DELETED\r\n");
			else printf ("NOT_FOUND\r\n");
		} else return 0;
	} else
	if (	!strcmp (cmd, "add") ||
		!strcmp (cmd, "set") ||
		!strcmp (cmd, "append") ||
		!strcmp (cmd, "prepend") ||
		!strcmp (cmd, "replace")
			) {
		int ret, stored = 1;
		char *b;
		if (!key || !((p=strchr(key, ' ')))) {
			printf ("ERROR\r\n");
			return 0;
		}
		*p = 0;
		ret = sscanf (p+1, "%d %llu %d", &flags, &exptime, &bytes);
		if (ret != 3) {
			printf ("ERROR\r\n");
			return 0;
		}
		if (bytes<1) {
			printf ("CLIENT_ERROR invalid length\r\n");
			printf ("ERROR\r\n");
			return 0;
		}
		bytes++; // '\n'
		b = malloc (bytes+1);
		if (b) {
			int ret = whileread (0, b, bytes); // XXX
			if (ret != bytes) {
				printf ("CLIENT_ERROR bad data chunk\r\n");
				printf ("ERROR\r\n");
				return 0;
			}
		} else {
			printf ("CLIENT_ERROR invalid\r\n");
			printf ("ERROR\r\n");
			return 0;
		}
		if (feof (stdin))
			return -1;
		b[--bytes] = 0;
		switch (*cmd) {
		case 's': memcache_set (ms, key, exptime, b); break;
		case 'a': if (cmd[1]=='p') memcache_append (ms, key, exptime, b);
			else stored = memcache_add (ms, key, exptime, b); break;
		case 'p': memcache_prepend (ms, key, exptime, b); break;
		case 'r': stored = memcache_replace (ms, key, exptime, b); break;
		}
		if (stored) printf ("STORED\r\n");
		else printf ("NOT_STORED\r\n");
		return 1;
	} else printf ("ERROR\r\n");
	return 0;
}
