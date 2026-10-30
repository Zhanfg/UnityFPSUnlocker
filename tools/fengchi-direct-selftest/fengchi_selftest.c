/* Fengchi game_opt direct self-test, based on publicly released OPlus interface.
 * Do not use concurrently with an actual game. SPDX-License-Identifier: MIT */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/ioctl.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#define PIDPATH "/proc/game_opt/game_pid"
#define SYNCPATH "/proc/game_opt/frame_sync/sync_ctrl"
#define PRODUCEPATH "/proc/game_opt/frame_sync/produce"
struct frame_data { int32_t mode; int32_t bufferN; long timeStamp1; long timeStamp2; char reserved[128]; };
_Static_assert(sizeof(struct frame_data)==152,"AArch64 frame ABI mismatch");
#define PRODUCE _IOWR(0xDF,0,struct frame_data)
#define CONSUME _IOWR(0xDF,1,struct frame_data)
#define STOP _IOWR(0xDF,3,struct frame_data)
static volatile sig_atomic_t active=1;
static int sync_fd=-1,registered=0,sent=0,received=0,failed=0;
static long me=0;
static void sig_handler(int sig) {(void)sig;active=0;}
static int64_t monotime(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (int64_t)t.tv_sec*1000000000LL+t.tv_nsec;}
static int readfile(const char *path,char *out,size_t cap) {
    int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0)return -1;
    ssize_t n=read(fd,out,cap-1);int err=errno;close(fd);errno=err;
    if(n<0) return -1;
    out[n]='\0';
    return 0;
}
static int game_pid(int *p) {
    char b[128];if(readfile(PIDPATH,b,sizeof(b))<0)return -1;
    return sscanf(b,"game_pid=%d",p)==1 ? 0:-1;
}
static int write_pid(long pid) {
    char s[36];int n=snprintf(s,sizeof(s),"%ld\n",pid);
    int fd=open(PIDPATH,O_WRONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0)return -1;
    ssize_t w=write(fd,s,n);int err=errno;close(fd);errno=err;
    return w==n?0:-1;
}
static void cleanup(void) {
    if(sync_fd>=0) {
        struct frame_data f={0};
        int r=ioctl(sync_fd,STOP,&f);
        printf("stop_ioctl=%s errno=%d\n",r==0?"ok":"error",r==0?0:errno);
        close(sync_fd);sync_fd=-1;
    }
    if(registered) {
        int current=-2;
        if(game_pid(&current)==0 && current==me) {
            if(write_pid(-1)==0)puts("game_pid_restored=-1");
            else printf("game_pid_restore_failed=%d\n",errno);
        } else puts("game_pid_restore=skipped_owner_changed");
        registered=0;
    }
    fflush(stdout);
}
static int do_event(unsigned long request,int mode,int bufferN) {
    struct frame_data f={0};
    f.mode=mode;f.bufferN=bufferN;f.timeStamp1=(long)monotime();f.timeStamp2=f.timeStamp1;
    int r=ioctl(sync_fd,request,&f);
    if(r<0)printf("event_error mode=%d errno=%d (%s)\n",mode,errno,strerror(errno));
    return r;
}
static int getint(const char *s,int lo,int hi) {
    char *end;errno=0;long n=strtol(s,&end,10);
    return errno||!s[0]||*end||n<lo||n>hi ? -1:(int)n;
}
int main(int argc,char **argv) {
    int fps=60,duration=3,without_registration=0,info=0;
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--info"))info=1;
        else if(!strcmp(argv[i],"--no-register"))without_registration=1;
        else if(!strcmp(argv[i],"--fps")&&i+1<argc){fps=getint(argv[++i],10,185);if(fps<0)return 64;}
        else if(!strcmp(argv[i],"--seconds")&&i+1<argc){duration=getint(argv[++i],1,5);if(duration<0)return 64;}
        else{fprintf(stderr,"usage: %s [--info] [--no-register] [--fps N] [--seconds N]\n",argv[0]);return 64;}
    }
    setvbuf(stdout,NULL,_IOLBF,0); me=getpid();
    prctl(PR_SET_NAME,"fengchi-test",0,0,0);
    signal(SIGINT,sig_handler);signal(SIGTERM,sig_handler);signal(SIGHUP,sig_handler);
    atexit(cleanup);
    printf("process_pid=%ld frame_abi=%zu produce_ioctl=0x%lx\n",me,sizeof(struct frame_data),(unsigned long)PRODUCE);
    char buf[128];
    if(readfile(PIDPATH,buf,sizeof(buf))==0)printf("original_%s",buf);
    else printf("game_pid_unreadable_errno=%d\n",errno);
    if(info) {
        printf("game_opt_frame_sync_present=%d\n",access(SYNCPATH,F_OK)==0);
        printf("geas_present=%d\n",access("/proc/game_opt/geas_ctrl",F_OK)==0);
        return 0;
    }
    if(geteuid()!=0){puts("error=root_required");return 77;}
    if(!without_registration) {
        int existing=-2;
        if(game_pid(&existing)!=0){puts("register=unavailable_proceed_without");}
        else if(existing!=-1){printf("error=game_pid_occupied_%d\n",existing);return 73;}
        else if(write_pid(me)==0) {
            int now=-2;
            if(game_pid(&now)==0&&now==me){registered=1;puts("registered_test_process=1");}
            else{puts("error=register_not_confirmed");return 74;}
        } else printf("register=denied_errno_%d_proceed_without\n",errno);
    } else puts("register=skipped_by_request");
    sync_fd=open(SYNCPATH,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(sync_fd<0){printf("error=frame_sync_open_errno_%d\n",errno);return 72;}
    int frames=fps*duration;
    int64_t origin=monotime();
    for(int i=0;i<frames&&active;i++) {
        if(registered&&i%fps==0) {
            int now=-2;
            if(game_pid(&now)==0&&now!=me){printf("error=game_pid_reassigned_%d\n",now);break;}
        }
        int bufN=1+(i%2);
        if(do_event(PRODUCE,0,bufN)==0)sent++;else if(++failed>=2)break;
        volatile uint64_t dummy=0;
        for(int j=0;j<10000;j++)dummy+=((uint64_t)i+37u)*(uint64_t)j;
        (void)dummy;
        if(do_event(CONSUME,1,bufN)==0)received++;else if(++failed>=2)break;
        int64_t until=origin+(int64_t)(i+1)*1000000000LL/fps;
        int64_t remaining=until-monotime();
        if(remaining>0) {
            struct timespec d={remaining/1000000000LL,remaining%1000000000LL};
            while(nanosleep(&d,&d)<0&&errno==EINTR&&active){}
        }
        if((i+1)%fps==0)printf("progress=%d produced=%d consumed=%d\n",i+1,sent,received);
    }
    if(readfile(PRODUCEPATH,buf,sizeof(buf))==0)printf("frame_sync_readback=%s",buf);
    printf("result produce=%d consume=%d failures=%d interrupted=%d\n",sent,received,failed,!active);
    return (sent>0&&failed==0&&active)?0:1;
}
