/*
 * Portable (120 lines) True Open C Source of Linux FTP Server
 * --------------------------------------------------------
 * This code compiles on ANY Linux PC using tcc or the internal gcc compiler.
 * Build: tcc -o tcc_ftp tcc_ftp.c
 * Usage: ./tcc_ftp [root] [optional_ip_override_for_Chromebook]
 * NOTE: if your Linux does't have 200KB tcc you should get it: "sudo apt install tcc"
 * --------------------------------------------------------
 * LICENSE: MIT (Free, no-strings-attached)
 * ORIGIN: Human + Gemini 3 Flash + Copilot (Refined)
 * * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal 
 * in the Software without restriction, including without limitation the rights 
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies.
 * * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
 * --------------------------------------------------------
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <utime.h>
#define CTRL_PORT 2121
#define DATA_PORT 2122
#define BUF_SIZE 8192
#define MAX_PATH 4096
static char u_key[] = "vpn"; static char p_key[] = "vpn";
int data_listener = -1;
char global_ip_comma[64] = "127,0,0,1";
char root_dir[MAX_PATH];
void send_s(int s, const char* msg) { send(s, msg, (int)strlen(msg), 0); }
void clean_cmd(char* t) { for(int i=0; t[i]; i++) if(t[i]=='\r' || t[i]=='\n') t[i] = 0; }
int get_lp(char* lp, const char* cur, const char* fn) {
    if (fn && strstr(fn, "..")) return 0;
    if (fn && fn[0] == '/') sprintf(lp, "%s/%s", root_dir, fn+1);
    else sprintf(lp, "%s%s/%s", root_dir, strcmp(cur,"/")==0?"":cur, fn?fn:"");
    return 1;
}
void send_list_data(const char* v_path, int is_mlsd) {
    if (data_listener == -1) return;
    struct sockaddr_in dcaddr; socklen_t dlen = sizeof(dcaddr);
    int cl = accept(data_listener, (struct sockaddr*)&dcaddr, &dlen);
    if (cl != -1) {
        char lp[MAX_PATH]; get_lp(lp, v_path, NULL);
        DIR *d = opendir(lp);
        if (d) {
            struct dirent *dir; struct stat st; char line[512], fpath[MAX_PATH];
            const char* mos[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
            while ((dir = readdir(d)) != NULL) {
                if (dir->d_name[0] == '.') continue;
                sprintf(fpath, "%s/%s", lp, dir->d_name);
                if (stat(fpath, &st) == 0) {
                    struct tm *tm = gmtime(&st.st_mtime);
                    if (is_mlsd) sprintf(line, "modify=%04d%02d%02d%02d%02d%02d;type=%s;size=%ld; %s\r\n", tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, S_ISDIR(st.st_mode)?"dir":"file", (long)st.st_size, dir->d_name);
                    else sprintf(line, "%crwxr-xr-x 1 ftp ftp %ld %s %2d %02d:%02d %s\r\n", S_ISDIR(st.st_mode)?'d':'-', (long)st.st_size, mos[tm->tm_mon], tm->tm_mday, tm->tm_hour, tm->tm_min, dir->d_name);
                    send(cl, line, strlen(line), 0);
                }
            } closedir(d);
        } close(cl);
    } close(data_listener); data_listener = -1;
}
int file_op(const char* v_path, const char* fn, int is_retr) {
    int ok = 0; if (data_listener == -1) return 0;
    struct sockaddr_in dcaddr; socklen_t dlen = sizeof(dcaddr);
    int cl = accept(data_listener, (struct sockaddr*)&dcaddr, &dlen);
    if (cl != -1) {
        char lp[MAX_PATH]; if (get_lp(lp, v_path, fn)) {
            FILE* f = fopen(lp, is_retr ? "rb" : "wb");
            if (f) {
                ok = 1; char b[BUF_SIZE]; int n;
                if (is_retr) while ((n = fread(b, 1, BUF_SIZE, f)) > 0) send(cl, b, n, 0);
                else while ((n = recv(cl, b, BUF_SIZE, 0)) > 0) { if(fwrite(b, 1, n, f) < n) ok = 0; }
                fclose(f);
            }
        } close(cl);
    } close(data_listener); data_listener = -1; return ok;
}
int main(int argc, char* argv[]) {
    if (!isatty(STDIN_FILENO)) { char cmd[1024]; snprintf(cmd, sizeof(cmd), "x-terminal-emulator -e '%s' &", argv[0]); system(cmd); return 0; }
    prctl(PR_SET_PDEATHSIG, SIGHUP); signal(SIGPIPE, SIG_IGN);
    char *target = (argc > 1 && strcmp(argv[1], "~") != 0) ? argv[1] : getenv("HOME");
    if (target) strncpy(root_dir, target, MAX_PATH-1); else getcwd(root_dir, MAX_PATH);
    chdir(root_dir);
    char local_ip[64] = "127.0.0.1"; if (argc > 2) strncpy(local_ip, argv[2], 63);
    else {
        int s_ip = socket(AF_INET, SOCK_DGRAM, 0); struct sockaddr_in dns = {AF_INET, htons(53)}; dns.sin_addr.s_addr = inet_addr("8.8.8.8");
        if (connect(s_ip, (struct sockaddr*)&dns, sizeof(dns)) == 0) {
            struct sockaddr_in name; socklen_t nl = sizeof(name); getsockname(s_ip, (struct sockaddr*)&name, &nl);
            inet_ntop(AF_INET, &name.sin_addr, local_ip, 64);
        } close(s_ip);
    }
    for (int i=0, j=0; local_ip[i]; i++) global_ip_comma[j++] = (local_ip[i] == '.') ? ',' : local_ip[i];
    int ls = socket(AF_INET, SOCK_STREAM, 0), opt = 1; setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = {AF_INET, INADDR_ANY, .sin_port = htons(CTRL_PORT)};
    if (bind(ls, (struct sockaddr*)&addr, sizeof(addr)) < 0) return 1;
    listen(ls, 5); printf("\nLAB FTP SERVER\nRoot: %s\nURL: ftp://%s:%s@%s:%d\nPress [Ctrl+C] to stop.\n", root_dir, u_key,p_key,local_ip, CTRL_PORT);
    while (1) {
        struct sockaddr_in caddr; socklen_t len = sizeof(caddr); int cs = accept(ls, (struct sockaddr*)&caddr, &len);
        if (cs < 0) continue;
        struct timeval tv = {60, 0}; setsockopt(cs, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        char v_dir[MAX_PATH] = "/", buf[BUF_SIZE], rnf[MAX_PATH] = {0}; struct stat st; send_s(cs, "220 Ready\r\n");
        int n; while ((n = recv(cs, buf, BUF_SIZE - 1, 0)) > 0) {
            buf[n] = 0; char *arg = strchr(buf, ' '); if (arg) { arg++; clean_cmd(arg); }
            if (strncmp(buf, "USER", 4) == 0) { if (strstr(buf, u_key)) send_s(cs, "331 OK\r\n"); else break; }
            else if (strncmp(buf, "PASS", 4) == 0) { if (strstr(buf, p_key)) send_s(cs, "230 OK\r\n"); else break; }
            else if (strncmp(buf, "PASV", 4) == 0) {
                if (data_listener != -1) close(data_listener);
                data_listener = socket(AF_INET, SOCK_STREAM, 0); struct sockaddr_in daddr = {AF_INET, .sin_port = htons(DATA_PORT), .sin_addr.s_addr = INADDR_ANY};
                setsockopt(data_listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
                bind(data_listener, (struct sockaddr*)&daddr, sizeof(daddr)); listen(data_listener, 1);
                usleep(20000); char m[128]; sprintf(m, "227 Entering Passive Mode (%s,8,74).\r\n", global_ip_comma); send_s(cs, m);
            }
            else if (strncmp(buf, "LIST", 4) == 0 || strncmp(buf, "MLSD", 4) == 0) { send_s(cs, "150 OK\r\n"); send_list_data(v_dir, buf[0]=='M'); send_s(cs, "226 Done\r\n"); }
            else if (strncmp(buf, "PWD", 3) == 0 || strncmp(buf, "XPWD", 4) == 0) { char m[MAX_PATH+32]; sprintf(m, "257 \"%s\"\r\n", v_dir); send_s(cs, m); }
            else if (strncmp(buf, "CWD", 3) == 0 && arg) { if (strstr(arg, "..")) send_s(cs, "550 Fail\r\n"); else { if (arg[0] == '/') strcpy(v_dir, arg); else { if (strcmp(v_dir, "/")!=0) strcat(v_dir, "/"); strcat(v_dir, arg); } send_s(cs, "250 OK\r\n"); } }
            else if ((strncmp(buf, "RETR", 4) == 0 || strncmp(buf, "STOR", 4) == 0) && arg) { send_s(cs, "150 OK\r\n"); if (file_op(v_dir, arg, buf[0]=='R')) send_s(cs, "226 Done\r\n"); else send_s(cs, "550 Fail\r\n"); }
            else if (strncmp(buf, "MDTM", 4) == 0 && arg) { char lp[MAX_PATH]; if (get_lp(lp, v_dir, arg) && stat(lp, &st) == 0) { struct tm *tm = gmtime(&st.st_mtime); char m[64]; sprintf(m, "213 %04d%02d%02d%02d%02d%02d\r\n", tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec); send_s(cs, m); } else send_s(cs, "550 Fail\r\n"); }
            else if (strncmp(buf, "MFMT", 4) == 0 && arg) { char ts[16], fn[MAX_PATH], lp[MAX_PATH]; sscanf(arg, "%14s %s", ts, fn); if (get_lp(lp, v_dir, fn)) { struct tm t = {0}; struct utimbuf ut; sscanf(ts, "%4d%2d%2d%2d%2d%2d", &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec); t.tm_year -= 1900; t.tm_mon -= 1; ut.actime = ut.modtime = timegm(&t); if (utime(lp, &ut) == 0) { char m[128]; sprintf(m, "213 Modify=%s; %s\r\n", ts, fn); send_s(cs, m); } else send_s(cs, "550 Fail\r\n"); } else send_s(cs, "550 Fail\r\n"); }
            else if (strncmp(buf, "MKD", 3) == 0 && arg) { char lp[MAX_PATH]; if (get_lp(lp, v_dir, arg) && (mkdir(lp, 0755) == 0 || errno == EEXIST)) send_s(cs, "257 OK\r\n"); else send_s(cs, "550 Fail\r\n"); }
            else if ((strncmp(buf, "DELE", 4) == 0 || strncmp(buf, "RMD", 3) == 0) && arg) { char lp[MAX_PATH]; if (get_lp(lp, v_dir, arg) && (buf[0]=='D' ? unlink(lp) : rmdir(lp)) == 0) send_s(cs, "250 OK\r\n"); else send_s(cs, "550 Fail\r\n"); }
            else if (strncmp(buf, "RNFR", 4) == 0 && arg) { strcpy(rnf, arg); send_s(cs, "350 OK\r\n"); }
            else if (strncmp(buf, "RNTO", 4) == 0 && arg) { char lp1[MAX_PATH], lp2[MAX_PATH]; if (get_lp(lp1, v_dir, rnf) && get_lp(lp2, v_dir, arg) && rename(lp1, lp2) == 0) send_s(cs, "250 OK\r\n"); else send_s(cs, "550 Fail\r\n"); rnf[0]=0; }
            else if (strncmp(buf, "QUIT", 4) == 0) { send_s(cs, "221 Bye\r\n"); break; }
            else if (strncmp(buf, "FEAT", 4) == 0) send_s(cs, "211-Extensions:\r\n MLSD\r\n MDTM\r\n MFMT\r\n UTF8\r\n211 End\r\n");
            else send_s(cs, "200 OK\r\n");
        }close(cs); if (data_listener != -1) { close(data_listener); data_listener = -1; }
    }return 0;
}
