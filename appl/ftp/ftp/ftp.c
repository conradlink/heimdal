/*      $NetBSD: ftp.c,v 1.13 1995/09/16 22:32:59 pk Exp $      */

/*
 * Copyright (c) 1985, 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "ftp_locl.h"

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

struct	sockaddr_in hisctladdr;
struct	sockaddr_in data_addr;
int	data = -1;
int	abrtflag = 0;
jmp_buf	ptabort;
int	ptabflg;
int	ptflag = 0;
struct	sockaddr_in myctladdr;
off_t	restart_point = 0;


FILE	*cin, *cout;

typedef void (*sighand)(int);

char *
hookup(char *host, int port)
{
	struct hostent *hp = 0;
	int s, len, tos;
	static char hostnamebuf[80];

	memset((char *)&hisctladdr, 0, sizeof (hisctladdr));
	hisctladdr.sin_addr.s_addr = inet_addr(host);
	if (hisctladdr.sin_addr.s_addr != INADDR_NONE) {
		hisctladdr.sin_family = AF_INET;
		(void) strncpy(hostnamebuf, host, sizeof(hostnamebuf));
	} else {
		hp = gethostbyname(host);
		if (hp == NULL) {
			warnx("%s: %s", host, hstrerror(h_errno));
			code = -1;
			return ((char *) 0);
		}
		hisctladdr.sin_family = hp->h_addrtype;
		memmove((caddr_t)&hisctladdr.sin_addr,
				hp->h_addr_list[0], hp->h_length);
		memcpy(&hisctladdr.sin_addr, hp->h_addr, hp->h_length);
		(void) strncpy(hostnamebuf, hp->h_name, sizeof(hostnamebuf));
	}
	hostname = hostnamebuf;
	s = socket(hisctladdr.sin_family, SOCK_STREAM, 0);
	if (s < 0) {
		warn("socket");
		code = -1;
		return (0);
	}
	hisctladdr.sin_port = port;
	while (connect(s, (struct sockaddr *)&hisctladdr, sizeof (hisctladdr)) < 0) {
		if (hp && hp->h_addr_list[1]) {
			int oerrno = errno;
			char *ia;

			ia = inet_ntoa(hisctladdr.sin_addr);
			errno = oerrno;
			warn("connect to address %s", ia);
			hp->h_addr_list++;
			memmove((caddr_t)&hisctladdr.sin_addr,
					hp->h_addr_list[0], hp->h_length);
			fprintf(stdout, "Trying %s...\n",
				inet_ntoa(hisctladdr.sin_addr));
			(void) close(s);
			s = socket(hisctladdr.sin_family, SOCK_STREAM, 0);
			if (s < 0) {
				warn("socket");
				code = -1;
				return (0);
			}
			continue;
		}
		warn("connect");
		code = -1;
		goto bad;
	}
	len = sizeof (myctladdr);
	if (getsockname(s, (struct sockaddr *)&myctladdr, &len) < 0) {
		warn("getsockname");
		code = -1;
		goto bad;
	}
#ifdef IP_TOS
	tos = IPTOS_LOWDELAY;
	if (setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&tos, sizeof(int)) < 0)
		warn("setsockopt TOS (ignored)");
#endif
	cin = fdopen(s, "r");
	cout = fdopen(s, "w");
	if (cin == NULL || cout == NULL) {
		warnx("fdopen failed.");
		if (cin)
			(void) fclose(cin);
		if (cout)
			(void) fclose(cout);
		code = -1;
		goto bad;
	}
	if (verbose)
		printf("Connected to %s.\n", hostname);
	if (getreply(0) > 2) { 	/* read startup message from server */
		if (cin)
			(void) fclose(cin);
		if (cout)
			(void) fclose(cout);
		code = -1;
		goto bad;
	}
#ifdef SO_OOBINLINE
	{
	int on = 1;

	if (setsockopt(s, SOL_SOCKET, SO_OOBINLINE, (char *)&on, sizeof(on))
		< 0 && debug) {
			warn("setsockopt");
		}
	}
#endif /* SO_OOBINLINE */

	return (hostname);
bad:
	(void) close(s);
	return ((char *)0);
}

int
login(char *host)
{
	char tmp[80];
	char defaultpass[128];
	char *user, *pass, *acct;
	int n, aflag = 0;

	char *myname = NULL;
	struct passwd *pw = getpwuid(getuid());
	if (pw != NULL)
	    myname = pw->pw_name;

	user = pass = acct = 0;
	

	if(do_klogin(host))
	    printf("\n*** Using plaintext user and password ***\n\n");
	else{
	    printf("Kerberos login successful.\n\n");
	}

	if (ruserpass(host, &user, &pass, &acct) < 0) {
		code = -1;
		return (0);
	}
	while (user == NULL) {
	    if (myname)
		printf("Name (%s:%s): ", host, myname);
	    else
		printf("Name (%s): ", host);
	    fgets(tmp, sizeof(tmp) - 1, stdin);
	    tmp[strlen(tmp) - 1] = '\0';
	    if (*tmp == '\0')
		user = myname;
	    else
		user = tmp;
	}
	strcpy(username, user);
	n = command("USER %s", user);
	if (n == CONTINUE) {
	    if(auth_complete)
		pass = myname;
	    else if (pass == NULL) {
		char prompt[128];
		if(myname && 
		   (!strcmp(user, "ftp") || !strcmp(user, "anonymous"))){
		    sprintf(defaultpass, "%s@%s", myname, mydomain);
		    sprintf(prompt, "Password (%s): ", defaultpass);
		}else{
		    strcpy(defaultpass, "");
		    sprintf(prompt, "Password: ");
		}
		pass = defaultpass;
	        des_read_pw_string (tmp, sizeof(tmp), prompt, 0);
		if(tmp[0])
		    pass = tmp;
	    }
	    n = command("PASS %s", pass);
	}
	if (n == CONTINUE) {
		aflag++;
		acct = malloc(128); /* XXX */
		des_read_pw_string(acct, 128, "Account:", 0);
		n = command("ACCT %s", acct);
	}
	if (n != COMPLETE) {
		warnx("Login failed.");
		return (0);
	}
	if (!aflag && acct != NULL)
		(void) command("ACCT %s", acct);
	if (proxy)
		return (1);
	for (n = 0; n < macnum; ++n) {
		if (!strcmp("init", macros[n].mac_name)) {
			(void) strcpy(line, "$init");
			makeargv();
			domacro(margc, margv);
			break;
		}
	}
	sec_set_protection_level();
	return (1);
}

void
cmdabort(int sig)
{

	printf("\n");
	(void) fflush(stdout);
	abrtflag++;
	if (ptflag)
		longjmp(ptabort,1);
}

int
command(char *fmt, ...)
{
	va_list ap;
	int r;
	sighand oldintr;

	abrtflag = 0;
	if (debug)
		printf("---> ");
	if (cout == NULL) {
		warn("No control connection for command");
		code = -1;
		return (0);
	}
	oldintr = signal(SIGINT, cmdabort);
	va_start(ap, fmt);
	if(debug)
	    if (strncmp("PASS ", fmt, 5) == 0)
		printf("PASS XXXX");
	    else 
		vfprintf(stdout, fmt, ap);
	if(auth_complete)
	    krb4_write_enc(cout, fmt, ap);
	else
	    vfprintf(cout, fmt, ap);
	va_end(ap);
	if(debug){
	    printf("\n");
	    (void) fflush(stdout);
	}
	fprintf(cout, "\r\n");
	(void) fflush(cout);
	cpend = 1;
	r = getreply(!strcmp(fmt, "QUIT"));
	if (abrtflag && oldintr != SIG_IGN)
		(*oldintr)(SIGINT);
	(void) signal(SIGINT, oldintr);
	return (r);
}

char reply_string[BUFSIZ];		/* last line of previous reply */

int
getreply(int expecteof)
{
    char *p;
    char *lead_string;
    int c;
    struct sigaction sa, osa;
    char buf[1024];

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = cmdabort;
    sigaction(SIGINT, &sa, &osa);
    
    p = buf;

    while(1){
	c = getc(cin);
	switch(c){
	case EOF:
	    if (expecteof) {
		sigaction(SIGINT,&osa, NULL);
		code = 221;
		return 0;
	    }
	    lostpeer(0);
	    if (verbose) {
		printf("421 Service not available, remote server has closed connection\n");
		fflush(stdout);
	    }
	    code = 421;
	    return (4);
	    break;
	case IAC:
	    c = getc(cin);
	    if(c == WILL || c == WONT)
		fprintf(cout, "%c%c%c", IAC, DONT, getc(cin));
	    if(c == DO || c == DONT)
		fprintf(cout, "%c%c%c", IAC, WONT, getc(cin));
	    continue;
	case '\n':
	    *p++ = 0;
	    if(isdigit(buf[0])){
		sscanf(buf, "%d", &code);
		if(code == 631){
		    krb4_read_mic(buf);
		    sscanf(buf, "%d", &code);
		    lead_string = "S:";
		} else if(code == 632){
		    krb4_read_enc(buf);
		    sscanf(buf, "%d", &code);
		    lead_string = "P:";
		}else if(code == 633){
		    printf("Received confidential reply!\n");
		}else if(auth_complete)
		    lead_string = "!!";
		else
		    lead_string = "";
		if(verbose > 0 || (verbose > -1 && code > 499))
		    fprintf(stdout, "%s%s\n", lead_string, buf);
		if(buf[3] == ' '){
		    strcpy(reply_string, buf);
		    if (code < 200)
			cpend = 0;
		    sigaction(SIGINT, &osa, NULL);
		    if (code == 421)
			lostpeer(0);
#if 0
		    if (abrtflag && 
			osa.sa_handler != cmdabort && 
			osa.sa_handler != SIG_IGN)
			osa.sa_handler(SIGINT);
#endif
		    return code / 100;
		}
	    }else{
		if(verbose > 0 || (verbose > -1 && code > 499)){
		    if(auth_complete)
			fprintf(stdout, "!!");
		    fprintf(stdout, "%s\n", buf);
		}
	    }
	    p = buf;
	    continue;
	default:
	    *p++ = c;
	}
    }
    
}


#if 0
int
getreply(int expecteof)
{
	int c, n;
	int dig;
	int originalcode = 0, continuation = 0;
	sighand oldintr;
	int pflag = 0;
	char *cp, *pt = pasv;

	oldintr = signal(SIGINT, cmdabort);
	for (;;) {
		dig = n = code = 0;
		cp = reply_string;
		while ((c = getc(cin)) != '\n') {
			if (c == IAC) {     /* handle telnet commands */
				switch (c = getc(cin)) {
				case WILL:
				case WONT:
					c = getc(cin);
					fprintf(cout, "%c%c%c", IAC, DONT, c);
					(void) fflush(cout);
					break;
				case DO:
				case DONT:
					c = getc(cin);
					fprintf(cout, "%c%c%c", IAC, WONT, c);
					(void) fflush(cout);
					break;
				default:
					break;
				}
				continue;
			}
			dig++;
			if (c == EOF) {
				if (expecteof) {
					(void) signal(SIGINT,oldintr);
					code = 221;
					return (0);
				}
				lostpeer(0);
				if (verbose) {
					printf("421 Service not available, remote server has closed connection\n");
					(void) fflush(stdout);
				}
				code = 421;
				return (4);
			}
			if (c != '\r' && (verbose > 0 ||
			    (verbose > -1 && n == '5' && dig > 4))) {
				if (proxflag &&
				   (dig == 1 || dig == 5 && verbose == 0))
					printf("%s:",hostname);
				(void) putchar(c);
			}
			if (dig < 4 && isdigit(c))
				code = code * 10 + (c - '0');
			if (!pflag && code == 227)
				pflag = 1;
			if (dig > 4 && pflag == 1 && isdigit(c))
				pflag = 2;
			if (pflag == 2) {
				if (c != '\r' && c != ')')
					*pt++ = c;
				else {
					*pt = '\0';
					pflag = 3;
				}
			}
			if (dig == 4 && c == '-') {
				if (continuation)
					code = 0;
				continuation++;
			}
			if (n == 0)
				n = c;
			if (cp < &reply_string[sizeof(reply_string) - 1])
				*cp++ = c;
		}
		if (verbose > 0 || verbose > -1 && n == '5') {
			(void) putchar(c);
			(void) fflush (stdout);
		}
		if (continuation && code != originalcode) {
			if (originalcode == 0)
				originalcode = code;
			continue;
		}
		*cp = '\0';
		if(auth_complete){
		    if(code == 631)
			krb4_read_mic(reply_string);
		    else
			krb4_read_enc(reply_string);
		    n = code / 100 + '0';
		}
		
		    

		if (n != '1')
			cpend = 0;
		(void) signal(SIGINT,oldintr);
		if (code == 421 || originalcode == 421)
			lostpeer(0);
		if (abrtflag && oldintr != cmdabort && oldintr != SIG_IGN)
			(*oldintr)(SIGINT);
		return (n - '0');
	}
}
#endif

int
empty(fd_set *mask, int sec)
{
	struct timeval t;

	t.tv_sec = (long) sec;
	t.tv_usec = 0;
	return (select(32, mask, (fd_set *) 0, (fd_set *) 0, &t));
}

jmp_buf	sendabort;

void
abortsend(int sig)
{

	mflag = 0;
	abrtflag = 0;
	printf("\nsend aborted\nwaiting for remote to finish abort\n");
	(void) fflush(stdout);
	longjmp(sendabort, 1);
}

#define HASHBYTES 1024


static int
copy_stream(FILE *from, FILE *to)
{
    char buf[BUFSIZ];
    int n;
    int bytes = 0;
    struct stat st;

    int werr;
    
    void *chunk;

#ifdef HAVE_MMAP
    if(fstat(fileno(from), &st) == 0 && S_ISREG(st.st_mode)){
	chunk = mmap(0, st.st_size, PROT_READ, MAP_SHARED, fileno(from), 0);
	if(chunk != NULL){
	    sec_write(fileno(to), chunk, st.st_size);
	    munmap(chunk, st.st_size);
	    sec_fflush(to);
	    return st.st_size;
	}
    }
#endif

    while((n = read(fileno(from), buf, sizeof(buf))) > 0){
	werr = sec_write(fileno(to), buf, n);
	if(werr < 0)
	    break;
	bytes += werr;
    }
    sec_fflush(to);
    if(n < 0)
	warn("local");

    if(werr < 0){
	if(errno != EPIPE)
	    warn("netout");
	bytes = -1;
    }
    return bytes;
}

void
sendrequest(char *cmd, char *local, char *remote, int printnames)
{
	struct stat st;
	struct timeval start, stop;
	int c, d;
	FILE *fin, *dout = 0, *popen(const char *, const char *);
	int (*closefunc) __P((FILE *));
	sighand oldintr, oldintp;
	long bytes = 0, hashbytes = HASHBYTES;
	char *lmode, buf[BUFSIZ], *bufp;

	if (verbose && printnames) {
		if (local && *local != '-')
			printf("local: %s ", local);
		if (remote)
			printf("remote: %s\n", remote);
	}
	if (proxy) {
		proxtrans(cmd, local, remote);
		return;
	}
	if (curtype != type)
		changetype(type, 0);
	closefunc = NULL;
	oldintr = NULL;
	oldintp = NULL;
	lmode = "w";
	if (setjmp(sendabort)) {
		while (cpend) {
			(void) getreply(0);
		}
		if (data >= 0) {
			(void) close(data);
			data = -1;
		}
		if (oldintr)
			(void) signal(SIGINT,oldintr);
		if (oldintp)
			(void) signal(SIGPIPE,oldintp);
		code = -1;
		return;
	}
	oldintr = signal(SIGINT, abortsend);
	if (strcmp(local, "-") == 0)
		fin = stdin;
	else if (*local == '|') {
		oldintp = signal(SIGPIPE,SIG_IGN);
		fin = popen(local + 1, "r");
		if (fin == NULL) {
			warn("%s", local + 1);
			(void) signal(SIGINT, oldintr);
			(void) signal(SIGPIPE, oldintp);
			code = -1;
			return;
		}
		closefunc = pclose;
	} else {
		fin = fopen(local, "r");
		if (fin == NULL) {
			warn("local: %s", local);
			(void) signal(SIGINT, oldintr);
			code = -1;
			return;
		}
		closefunc = fclose;
		if (fstat(fileno(fin), &st) < 0 ||
		    (st.st_mode&S_IFMT) != S_IFREG) {
			fprintf(stdout, "%s: not a plain file.\n", local);
			(void) signal(SIGINT, oldintr);
			fclose(fin);
			code = -1;
			return;
		}
	}
	if (initconn()) {
		(void) signal(SIGINT, oldintr);
		if (oldintp)
			(void) signal(SIGPIPE, oldintp);
		code = -1;
		if (closefunc != NULL)
			(*closefunc)(fin);
		return;
	}
	if (setjmp(sendabort))
		goto abort;

	if (restart_point &&
	    (strcmp(cmd, "STOR") == 0 || strcmp(cmd, "APPE") == 0)) {
		int rc;

		switch (curtype) {
		case TYPE_A:
			rc = fseek(fin, (long) restart_point, SEEK_SET);
			break;
		case TYPE_I:
		case TYPE_L:
			rc = lseek(fileno(fin), restart_point, SEEK_SET);
			break;
		}
		if (rc < 0) {
			warn("local: %s", local);
			restart_point = 0;
			if (closefunc != NULL)
				(*closefunc)(fin);
			return;
		}
		if (command("REST %ld", (long) restart_point)
			!= CONTINUE) {
			restart_point = 0;
			if (closefunc != NULL)
				(*closefunc)(fin);
			return;
		}
		restart_point = 0;
		lmode = "r+w";
	}
	if (remote) {
		if (command("%s %s", cmd, remote) != PRELIM) {
			(void) signal(SIGINT, oldintr);
			if (oldintp)
				(void) signal(SIGPIPE, oldintp);
			if (closefunc != NULL)
				(*closefunc)(fin);
			return;
		}
	} else
		if (command("%s", cmd) != PRELIM) {
			(void) signal(SIGINT, oldintr);
			if (oldintp)
				(void) signal(SIGPIPE, oldintp);
			if (closefunc != NULL)
				(*closefunc)(fin);
			return;
		}
	dout = dataconn(lmode);
	if (dout == NULL)
		goto abort;
	(void) gettimeofday(&start, (struct timezone *)0);
	oldintp = signal(SIGPIPE, SIG_IGN);
	switch (curtype) {

	case TYPE_I:
	case TYPE_L:
		errno = d = c = 0;
		bytes = copy_stream(fin, dout);
		break;

	case TYPE_A:
	    while ((c = getc(fin)) != EOF) {
		if (c == '\n') {
		    while (hash && (bytes >= hashbytes)) {
			(void) putchar('#');
			(void) fflush(stdout);
			hashbytes += HASHBYTES;
		    }
		    if (ferror(dout))
			break;
		    (void) sec_putc('\r', dout);
		    bytes++;
		}
		sec_putc(c, dout);
		bytes++;
	    }
	    sec_fflush(dout);
	    if (hash) {
		if (bytes < hashbytes)
		    (void) putchar('#');
		(void) putchar('\n');
		(void) fflush(stdout);
	    }
	    if (ferror(fin))
		warn("local: %s", local);
	    if (ferror(dout)) {
		if (errno != EPIPE)
		    warn("netout");
		bytes = -1;
	    }
	    break;
	}
	if (closefunc != NULL)
		(*closefunc)(fin);
	(void) fclose(dout);
	(void) gettimeofday(&stop, (struct timezone *)0);
	(void) getreply(0);
	(void) signal(SIGINT, oldintr);
	if (oldintp)
		(void) signal(SIGPIPE, oldintp);
	if (bytes > 0)
		ptransfer("sent", bytes, &start, &stop);
	return;
abort:
	(void) signal(SIGINT, oldintr);
	if (oldintp)
		(void) signal(SIGPIPE, oldintp);
	if (!cpend) {
		code = -1;
		return;
	}
	if (data >= 0) {
		(void) close(data);
		data = -1;
	}
	if (dout)
		(void) fclose(dout);
	(void) getreply(0);
	code = -1;
	if (closefunc != NULL && fin != NULL)
		(*closefunc)(fin);
	(void) gettimeofday(&stop, (struct timezone *)0);
	if (bytes > 0)
		ptransfer("sent", bytes, &start, &stop);
}

jmp_buf	recvabort;

void
abortrecv(int sig)
{

	mflag = 0;
	abrtflag = 0;
	printf("\nreceive aborted\nwaiting for remote to finish abort\n");
	(void) fflush(stdout);
	longjmp(recvabort, 1);
}

void
recvrequest(char *cmd, char *local, char *remote, char *lmode, int printnames)
{
	FILE *fout, *din = 0;
	int (*closefunc) __P((FILE *));
	sighand oldintr, oldintp;
	int c, d, is_retr, tcrflag, bare_lfs = 0;
	static int bufsize;
	static char *buf;
	long bytes = 0, hashbytes = HASHBYTES;
	struct timeval start, stop;
	struct stat st;

	is_retr = strcmp(cmd, "RETR") == 0;
	if (is_retr && verbose && printnames) {
		if (local && *local != '-')
			printf("local: %s ", local);
		if (remote)
			printf("remote: %s\n", remote);
	}
	if (proxy && is_retr) {
		proxtrans(cmd, local, remote);
		return;
	}
	closefunc = NULL;
	oldintr = NULL;
	oldintp = NULL;
	tcrflag = !crflag && is_retr;
	if (setjmp(recvabort)) {
		while (cpend) {
			(void) getreply(0);
		}
		if (data >= 0) {
			(void) close(data);
			data = -1;
		}
		if (oldintr)
			(void) signal(SIGINT, oldintr);
		code = -1;
		return;
	}
	oldintr = signal(SIGINT, abortrecv);
	if (strcmp(local, "-") && *local != '|') {
		if (access(local, 2) < 0) {
			char *dir = strrchr(local, '/');

			if (errno != ENOENT && errno != EACCES) {
				warn("local: %s", local);
				(void) signal(SIGINT, oldintr);
				code = -1;
				return;
			}
			if (dir != NULL)
				*dir = 0;
			d = access(dir ? local : ".", 2);
			if (dir != NULL)
				*dir = '/';
			if (d < 0) {
				warn("local: %s", local);
				(void) signal(SIGINT, oldintr);
				code = -1;
				return;
			}
			if (!runique && errno == EACCES &&
			    chmod(local, 0600) < 0) {
				warn("local: %s", local);
				(void) signal(SIGINT, oldintr);
				(void) signal(SIGINT, oldintr);
				code = -1;
				return;
			}
			if (runique && errno == EACCES &&
			   (local = gunique(local)) == NULL) {
				(void) signal(SIGINT, oldintr);
				code = -1;
				return;
			}
		}
		else if (runique && (local = gunique(local)) == NULL) {
			(void) signal(SIGINT, oldintr);
			code = -1;
			return;
		}
	}
	if (!is_retr) {
		if (curtype != TYPE_A)
			changetype(TYPE_A, 0);
	} else if (curtype != type)
		changetype(type, 0);
	if (initconn()) {
		(void) signal(SIGINT, oldintr);
		code = -1;
		return;
	}
	if (setjmp(recvabort))
		goto abort;
	if (is_retr && restart_point &&
	    command("REST %ld", (long) restart_point) != CONTINUE)
		return;
	if (remote) {
		if (command("%s %s", cmd, remote) != PRELIM) {
			(void) signal(SIGINT, oldintr);
			return;
		}
	} else {
		if (command("%s", cmd) != PRELIM) {
			(void) signal(SIGINT, oldintr);
			return;
		}
	}
	din = dataconn("r");
	if (din == NULL)
		goto abort;
	if (strcmp(local, "-") == 0)
		fout = stdout;
	else if (*local == '|') {
		oldintp = signal(SIGPIPE, SIG_IGN);
		fout = popen(local + 1, "w");
		if (fout == NULL) {
			warn("%s", local+1);
			goto abort;
		}
		closefunc = pclose;
	} else {
		fout = fopen(local, lmode);
		if (fout == NULL) {
			warn("local: %s", local);
			goto abort;
		}
		closefunc = fclose;
	}
	{
	    size_t blocksize = BUFSIZ;
#ifdef HAVE_ST_BLKSIZE
	    if (fstat(fileno(fout), &st) >= 0 && st.st_blksize > 0)
		blocksize = st.st_blksize;
#endif
	    if (blocksize > bufsize) {
		if (buf)
		    (void) free(buf);
		buf = malloc(blocksize);
		if (buf == NULL) {
		    warn("malloc");
		    bufsize = 0;
		    goto abort;
		}
		bufsize = blocksize;
	    }
	}
	(void) gettimeofday(&start, (struct timezone *)0);
	switch (curtype) {

	case TYPE_I:
	case TYPE_L:
		if (restart_point &&
		    lseek(fileno(fout), restart_point, SEEK_SET) < 0) {
			warn("local: %s", local);
			if (closefunc != NULL)
				(*closefunc)(fout);
			return;
		}
		errno = d = 0;
		while ((c = sec_read(fileno(din), buf, bufsize)) > 0) {
		    if ((d = write(fileno(fout), buf, c)) != c)
			break;
		    bytes += c;
		    if (hash) {
			while (bytes >= hashbytes) {
			    (void) putchar('#');
			    hashbytes += HASHBYTES;
			}
			(void) fflush(stdout);
		    }
		}
		if (hash && bytes > 0) {
			if (bytes < HASHBYTES)
				(void) putchar('#');
			(void) putchar('\n');
			(void) fflush(stdout);
		}
		if (c < 0) {
			if (errno != EPIPE)
				warn("netin");
			bytes = -1;
		}
		if (d < c) {
			if (d < 0)
				warn("local: %s", local);
			else
				warnx("%s: short write", local);
		}
		break;

	case TYPE_A:
		if (restart_point) {
			int i, n, ch;

			if (fseek(fout, 0L, SEEK_SET) < 0)
				goto done;
			n = restart_point;
			for (i = 0; i++ < n;) {
				if ((ch = sec_getc(fout)) == EOF)
					goto done;
				if (ch == '\n')
					i++;
			}
			if (fseek(fout, 0L, SEEK_CUR) < 0) {
done:
				warn("local: %s", local);
				if (closefunc != NULL)
					(*closefunc)(fout);
				return;
			}
		}

		while ((c = sec_getc(din)) != EOF) {
			if (c == '\n')
				bare_lfs++;
			while (c == '\r') {
				while (hash && (bytes >= hashbytes)) {
					(void) putchar('#');
					(void) fflush(stdout);
					hashbytes += HASHBYTES;
				}
				bytes++;
				if ((c = sec_getc(din)) != '\n' || tcrflag) {
					if (ferror(fout))
						goto break2;
					(void) putc('\r', fout);
					if (c == '\0') {
						bytes++;
						goto contin2;
					}
					if (c == EOF)
						goto contin2;
				}
			}
			(void) putc(c, fout);
			bytes++;
	contin2:	;
		}
break2:
		if (bare_lfs) {
			printf("WARNING! %d bare linefeeds received in ASCII mode\n", bare_lfs);
			printf("File may not have transferred correctly.\n");
		}
		if (hash) {
			if (bytes < hashbytes)
				(void) putchar('#');
			(void) putchar('\n');
			(void) fflush(stdout);
		}
		if (ferror(din)) {
			if (errno != EPIPE)
				warn("netin");
			bytes = -1;
		}
		if (ferror(fout))
			warn("local: %s", local);
		break;
	}
	if (closefunc != NULL)
		(*closefunc)(fout);
	(void) signal(SIGINT, oldintr);
	if (oldintp)
		(void) signal(SIGPIPE, oldintp);
	(void) fclose(din);
	(void) gettimeofday(&stop, (struct timezone *)0);
	(void) getreply(0);
	if (bytes > 0 && is_retr)
		ptransfer("received", bytes, &start, &stop);
	return;
abort:

/* abort using RFC959 recommended IP,SYNC sequence  */

	if (oldintp)
		(void) signal(SIGPIPE, oldintr);
	(void) signal(SIGINT, SIG_IGN);
	if (!cpend) {
		code = -1;
		(void) signal(SIGINT, oldintr);
		return;
	}

	abort_remote(din);
	code = -1;
	if (data >= 0) {
		(void) close(data);
		data = -1;
	}
	if (closefunc != NULL && fout != NULL)
		(*closefunc)(fout);
	if (din)
		(void) fclose(din);
	(void) gettimeofday(&stop, (struct timezone *)0);
	if (bytes > 0)
		ptransfer("received", bytes, &start, &stop);
	(void) signal(SIGINT, oldintr);
}

/*
 * Need to start a listen on the data channel before we send the command,
 * otherwise the server's connect may fail.
 */
int
initconn(void)
{
	char *p, *a;
	int result, len, tmpno = 0;
	int on = 1;
	int a0, a1, a2, a3, p0, p1;

	if (passivemode) {
	        u_int32_t tmpaddr;
		u_int16_t tmpport;

		data = socket(AF_INET, SOCK_STREAM, 0);
		if (data < 0) {
			perror("ftp: socket");
			return(1);
		}
		if ((options & SO_DEBUG) &&
		    setsockopt(data, SOL_SOCKET, SO_DEBUG, (char *)&on,
			       sizeof (on)) < 0)
			perror("ftp: setsockopt (ignored)");
		if (command("PASV") != COMPLETE) {
			printf("Passive mode refused.\n");
			goto bad;
		}

		/*
		 * What we've got at this point is a string of comma
		 * separated one-byte unsigned integer values.
		 * The first four are the an IP address. The fifth is
		 * the MSB of the port number, the sixth is the LSB.
		 * From that we'll prepare a sockaddr_in.
		 */

		if (sscanf(pasv,"%d,%d,%d,%d,%d,%d",
			   &a0, &a1, &a2, &a3, &p0, &p1) != 6) {
			printf("Passive mode address scan failure. "
			       "Shouldn't happen!\n");
			goto bad;
		}

		memset(&data_addr, 0, sizeof(data_addr));
		data_addr.sin_family = AF_INET;
		tmpaddr = data_addr.sin_addr.s_addr;
		a = (char *)&tmpaddr;
		a[0] = a0 & 0xff;
		a[1] = a1 & 0xff;
		a[2] = a2 & 0xff;
		a[3] = a3 & 0xff;
		tmpport = data_addr.sin_port;
		p = (char *)&tmpport;
		p[0] = p0 & 0xff;
		p[1] = p1 & 0xff;

		if (connect(data, (struct sockaddr *)&data_addr,
			    sizeof(data_addr)) < 0) {
			perror("ftp: connect");
			goto bad;
		}
#ifdef IP_TOS
		on = IPTOS_THROUGHPUT;
		if (setsockopt(data, IPPROTO_IP, IP_TOS, (char *)&on,
			       sizeof(int)) < 0)
			perror("ftp: setsockopt TOS (ignored)");
#endif
		return(0);
	}

noport:
	data_addr = myctladdr;
	if (sendport)
		data_addr.sin_port = 0;	/* let system pick one */ 
	if (data != -1)
		(void) close(data);
	data = socket(AF_INET, SOCK_STREAM, 0);
	if (data < 0) {
		warn("socket");
		if (tmpno)
			sendport = 1;
		return (1);
	}
	if (!sendport)
		if (setsockopt(data, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof (on)) < 0) {
			warn("setsockopt (reuse address)");
			goto bad;
		}
	if (bind(data, (struct sockaddr *)&data_addr, sizeof (data_addr)) < 0) {
		warn("bind");
		goto bad;
	}
	if (options & SO_DEBUG &&
	    setsockopt(data, SOL_SOCKET, SO_DEBUG, (char *)&on, sizeof (on)) < 0)
		warn("setsockopt (ignored)");
	len = sizeof (data_addr);
	if (getsockname(data, (struct sockaddr *)&data_addr, &len) < 0) {
		warn("getsockname");
		goto bad;
	}
	if (listen(data, 1) < 0)
		warn("listen");
	if (sendport) {
	    unsigned int a = ntohl(data_addr.sin_addr.s_addr);
	    unsigned int p = ntohs(data_addr.sin_port);
	    result = command("PORT %d,%d,%d,%d,%d,%d", 
			     (a >> 24) & 0xff,
			     (a >> 16) & 0xff,
			     (a >> 8) & 0xff,
			     a & 0xff,
			     (p >> 8) & 0xff,
			     p & 0xff);
	    if (result == ERROR && sendport == -1) {
		sendport = 0;
		tmpno = 1;
		goto noport;
	    }
	    return (result != COMPLETE);
	}
	if (tmpno)
		sendport = 1;
#ifdef IP_TOS
	on = IPTOS_THROUGHPUT;
	if (setsockopt(data, IPPROTO_IP, IP_TOS, (char *)&on, sizeof(int)) < 0)
		warn("setsockopt TOS (ignored)");
#endif
	return (0);
bad:
	(void) close(data), data = -1;
	if (tmpno)
		sendport = 1;
	return (1);
}

FILE *
dataconn(char *lmode)
{
	struct sockaddr_in from;
	int s, fromlen = sizeof (from), tos;

	if (passivemode)
		return (fdopen(data, lmode));

	s = accept(data, (struct sockaddr *) &from, &fromlen);
	if (s < 0) {
		warn("accept");
		(void) close(data), data = -1;
		return (NULL);
	}
	(void) close(data);
	data = s;
#ifdef IP_TOS
	tos = IPTOS_THROUGHPUT;
	if (setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&tos, sizeof(int)) < 0)
		warn("setsockopt TOS (ignored)");
#endif
	return (fdopen(data, lmode));
}

void
ptransfer(char *direction, long int bytes, 
	  struct timeval *t0, struct timeval *t1)
{
    struct timeval td;
    float s;
    long bs;

    if (verbose) {
	td.tv_sec = t1->tv_sec - t0->tv_sec;
	td.tv_usec = t1->tv_usec - t0->tv_usec;
	if(td.tv_usec < 0){
	    td.tv_sec--;
	    td.tv_usec += 1000000;
	}
	s = td.tv_sec + (td.tv_usec / 1000000.);
	bs = bytes / (s?s:1);
	printf("%ld bytes %s in %.3g seconds (%ld bytes/s)\n",
	       bytes, direction, s, bs);
    }
}

void
psabort(int sig)
{

	abrtflag++;
}

void
pswitch(int flag)
{
	sighand oldintr;
	static struct comvars {
		int connect;
		char name[MaxHostNameLen];
		struct sockaddr_in mctl;
		struct sockaddr_in hctl;
		FILE *in;
		FILE *out;
		int tpe;
		int curtpe;
		int cpnd;
		int sunqe;
		int runqe;
		int mcse;
		int ntflg;
		char nti[17];
		char nto[17];
		int mapflg;
		char mi[MaxPathLen];
		char mo[MaxPathLen];
	} proxstruct, tmpstruct;
	struct comvars *ip, *op;

	abrtflag = 0;
	oldintr = signal(SIGINT, psabort);
	if (flag) {
		if (proxy)
			return;
		ip = &tmpstruct;
		op = &proxstruct;
		proxy++;
	} else {
		if (!proxy)
			return;
		ip = &proxstruct;
		op = &tmpstruct;
		proxy = 0;
	}
	ip->connect = connected;
	connected = op->connect;
	if (hostname) {
		(void) strncpy(ip->name, hostname, sizeof(ip->name) - 1);
		ip->name[strlen(ip->name)] = '\0';
	} else
		ip->name[0] = 0;
	hostname = op->name;
	ip->hctl = hisctladdr;
	hisctladdr = op->hctl;
	ip->mctl = myctladdr;
	myctladdr = op->mctl;
	ip->in = cin;
	cin = op->in;
	ip->out = cout;
	cout = op->out;
	ip->tpe = type;
	type = op->tpe;
	ip->curtpe = curtype;
	curtype = op->curtpe;
	ip->cpnd = cpend;
	cpend = op->cpnd;
	ip->sunqe = sunique;
	sunique = op->sunqe;
	ip->runqe = runique;
	runique = op->runqe;
	ip->mcse = mcase;
	mcase = op->mcse;
	ip->ntflg = ntflag;
	ntflag = op->ntflg;
	(void) strncpy(ip->nti, ntin, 16);
	(ip->nti)[strlen(ip->nti)] = '\0';
	(void) strcpy(ntin, op->nti);
	(void) strncpy(ip->nto, ntout, 16);
	(ip->nto)[strlen(ip->nto)] = '\0';
	(void) strcpy(ntout, op->nto);
	ip->mapflg = mapflag;
	mapflag = op->mapflg;
	(void) strncpy(ip->mi, mapin, MaxPathLen - 1);
	(ip->mi)[strlen(ip->mi)] = '\0';
	(void) strcpy(mapin, op->mi);
	(void) strncpy(ip->mo, mapout, MaxPathLen - 1);
	(ip->mo)[strlen(ip->mo)] = '\0';
	(void) strcpy(mapout, op->mo);
	(void) signal(SIGINT, oldintr);
	if (abrtflag) {
		abrtflag = 0;
		(*oldintr)(SIGINT);
	}
}

void
abortpt(int sig)
{

	printf("\n");
	(void) fflush(stdout);
	ptabflg++;
	mflag = 0;
	abrtflag = 0;
	longjmp(ptabort, 1);
}

void
proxtrans(char *cmd, char *local, char *remote)
{
	sighand oldintr;
	int secndflag = 0, prox_type, nfnd;
	char *cmd2;
	fd_set mask;

	if (strcmp(cmd, "RETR"))
		cmd2 = "RETR";
	else
		cmd2 = runique ? "STOU" : "STOR";
	if ((prox_type = type) == 0) {
		if (unix_server && unix_proxy)
			prox_type = TYPE_I;
		else
			prox_type = TYPE_A;
	}
	if (curtype != prox_type)
		changetype(prox_type, 1);
	if (command("PASV") != COMPLETE) {
		printf("proxy server does not support third party transfers.\n");
		return;
	}
	pswitch(0);
	if (!connected) {
		printf("No primary connection\n");
		pswitch(1);
		code = -1;
		return;
	}
	if (curtype != prox_type)
		changetype(prox_type, 1);
	if (command("PORT %s", pasv) != COMPLETE) {
		pswitch(1);
		return;
	}
	if (setjmp(ptabort))
		goto abort;
	oldintr = signal(SIGINT, abortpt);
	if (command("%s %s", cmd, remote) != PRELIM) {
		(void) signal(SIGINT, oldintr);
		pswitch(1);
		return;
	}
	sleep(2);
	pswitch(1);
	secndflag++;
	if (command("%s %s", cmd2, local) != PRELIM)
		goto abort;
	ptflag++;
	(void) getreply(0);
	pswitch(0);
	(void) getreply(0);
	(void) signal(SIGINT, oldintr);
	pswitch(1);
	ptflag = 0;
	printf("local: %s remote: %s\n", local, remote);
	return;
abort:
	(void) signal(SIGINT, SIG_IGN);
	ptflag = 0;
	if (strcmp(cmd, "RETR") && !proxy)
		pswitch(1);
	else if (!strcmp(cmd, "RETR") && proxy)
		pswitch(0);
	if (!cpend && !secndflag) {  /* only here if cmd = "STOR" (proxy=1) */
		if (command("%s %s", cmd2, local) != PRELIM) {
			pswitch(0);
			if (cpend)
				abort_remote((FILE *) NULL);
		}
		pswitch(1);
		if (ptabflg)
			code = -1;
		(void) signal(SIGINT, oldintr);
		return;
	}
	if (cpend)
		abort_remote((FILE *) NULL);
	pswitch(!proxy);
	if (!cpend && !secndflag) {  /* only if cmd = "RETR" (proxy=1) */
		if (command("%s %s", cmd2, local) != PRELIM) {
			pswitch(0);
			if (cpend)
				abort_remote((FILE *) NULL);
			pswitch(1);
			if (ptabflg)
				code = -1;
			(void) signal(SIGINT, oldintr);
			return;
		}
	}
	if (cpend)
		abort_remote((FILE *) NULL);
	pswitch(!proxy);
	if (cpend) {
		FD_ZERO(&mask);
		FD_SET(fileno(cin), &mask);
		if ((nfnd = empty(&mask, 10)) <= 0) {
			if (nfnd < 0) {
				warn("abort");
			}
			if (ptabflg)
				code = -1;
			lostpeer(0);
		}
		(void) getreply(0);
		(void) getreply(0);
	}
	if (proxy)
		pswitch(0);
	pswitch(1);
	if (ptabflg)
		code = -1;
	(void) signal(SIGINT, oldintr);
}

void
reset(int argc, char **argv)
{
	fd_set mask;
	int nfnd = 1;

	FD_ZERO(&mask);
	while (nfnd > 0) {
		FD_SET(fileno(cin), &mask);
		if ((nfnd = empty(&mask,0)) < 0) {
			warn("reset");
			code = -1;
			lostpeer(0);
		}
		else if (nfnd) {
			(void) getreply(0);
		}
	}
}

char *
gunique(char *local)
{
	static char new[MaxPathLen];
	char *cp = strrchr(local, '/');
	int d, count=0;
	char ext = '1';

	if (cp)
		*cp = '\0';
	d = access(cp ? local : ".", 2);
	if (cp)
		*cp = '/';
	if (d < 0) {
		warn("local: %s", local);
		return ((char *) 0);
	}
	(void) strcpy(new, local);
	cp = new + strlen(new);
	*cp++ = '.';
	while (!d) {
		if (++count == 100) {
			printf("runique: can't find unique file name.\n");
			return ((char *) 0);
		}
		*cp++ = ext;
		*cp = '\0';
		if (ext == '9')
			ext = '0';
		else
			ext++;
		if ((d = access(new, 0)) < 0)
			break;
		if (ext != '0')
			cp--;
		else if (*(cp - 2) == '.')
			*(cp - 1) = '1';
		else {
			*(cp - 2) = *(cp - 2) + 1;
			cp--;
		}
	}
	return (new);
}

void
abort_remote(FILE *din)
{
	char buf[BUFSIZ];
	int nfnd;
	fd_set mask;

	/*
	 * send IAC in urgent mode instead of DM because 4.3BSD places oob mark
	 * after urgent byte rather than before as is protocol now
	 */
	sprintf(buf, "%c%c%c", IAC, IP, IAC);
	if (send(fileno(cout), buf, 3, MSG_OOB) != 3)
		warn("abort");
	fprintf(cout,"%cABOR\r\n", DM);
	(void) fflush(cout);
	FD_ZERO(&mask);
	FD_SET(fileno(cin), &mask);
	if (din) { 
		FD_SET(fileno(din), &mask);
	}
	if ((nfnd = empty(&mask, 10)) <= 0) {
		if (nfnd < 0) {
			warn("abort");
		}
		if (ptabflg)
			code = -1;
		lostpeer(0);
	}
	if (din && FD_ISSET(fileno(din), &mask)) {
		while (read(fileno(din), buf, BUFSIZ) > 0)
			/* LOOP */;
	}
	if (getreply(0) == ERROR && code == 552) {
		/* 552 needed for nic style abort */
		(void) getreply(0);
	}
	(void) getreply(0);
}
