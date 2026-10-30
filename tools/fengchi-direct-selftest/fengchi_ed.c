// SPDX-License-Identifier: MIT
// OPlus game_opt Early Detect experiment. Requires explicit user invocation.
// Not an OEM GPU interpolation implementation. Android arm64 only.
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <time.h>
#include <unistd.h>
#define BASE "/proc/game_opt"
#define GAMEPID BASE "/game_pid"
#define TARGET BASE "/early_detect/target_fps"
#define ENABLE BASE "/early_detect/ed_enable"
#define ARGS BASE "/early_detect/ed_args"
#define PRODUCE BASE "/early_detect/frame_produce"

static volatile sig_atomic_t keep=1;
static int pid_set=0,target_set=0,args_set=0,enabled=0,write_fail=0;
static int fps=60,duration=3;
static pid_t me;
static int original_args[12];
static int args_valid=0;
static void on_signal(int x){(void)x;keep=0;}
static int64_t nsec(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (int64_t)t.tv_sec*1000000000LL+t.tv_nsec;}
static int read_text(const char*path,char*buf,size_t cap) {
    int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0)return -1;
    ssize_t len=read(fd,buf,cap-1);
    int err=errno;close(fd);errno=err;
    if(len<0)return -1;
    buf[len]=0;
    return 0;
}
static int write_text(const char*path,const char*buf){
    int fd=open(path,O_WRONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0)return -1;
    size_t size=strlen(buf);
    ssize_t got=write(fd,buf,size);
    int err=errno;close(fd);errno=err;
    return got==(ssize_t)size?0:-1;
}
static int read_int(const char*path,int*x){
    char b[128];return read_text(path,b,sizeof(b))==0&&sscanf(b,"%d",x)==1?0:-1;
}
static int set_int(const char*path,int v){
    char b[48];snprintf(b,sizeof(b),"%d\n",v);return write_text(path,b);
}
static int get_pid(int*value){
    char b[128];
    return read_text(GAMEPID,b,sizeof(b))==0&&sscanf(b,"game_pid=%d",value)==1?0:-1;
}
static int get_args(int a[12]){
    char b[400];if(read_text(ARGS,b,sizeof(b))<0)return -1;
    int n=sscanf(b,"mbt_ms=%d, b2ft_offset=%d, fdrml_offset=%d, fst_offset=%d, flt_offset=%d, rbt_mode=%d, rbt_us=%d, rst_us=%d, b2rst_us=%d, rrt_us=%d, fet_us=%d, rw_times=%d",
      &a[0],&a[1],&a[2],&a[3],&a[4],&a[5],&a[6],&a[7],&a[8],&a[9],&a[10],&a[11]);
    return n==12?0:-1;
}
static int default_args(const int a[12]){
    int def[12]={100,20,-20,-100,-100,0,0,0,0,0,0,0};
    for(int i=0;i<12;i++)if(a[i]!=def[i])return 0;
    return 1;
}
static int64_t freq(int cpu){
    char path[160],value[60];snprintf(path,sizeof(path),"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq",cpu);
    return read_text(path,value,sizeof(value))==0?atoll(value):-1;
}
static int64_t gpu(void){
    char b[60];return read_text("/sys/class/kgsl/kgsl-3d0/devfreq/cur_freq",b,sizeof(b))==0?atoll(b):-1;
}
static void cleanup(void){
    // Restore configuration only if nobody changed the process ownership.
    int owner=-2;
    int ours=get_pid(&owner)==0 && owner==me;
    if(!ours && (pid_set || enabled || args_set || target_set)){
        fprintf(stderr,"ALERT owner changed; unsafe to overwrite OEM configuration. game_pid=%d\n",owner);
        return;
    }
    if(enabled){
        int v=-1;
        if(read_int(ENABLE,&v)==0 && v==1){
            if(set_int(ENABLE,0)==0)puts("restore_ed_enable=0");
            else fprintf(stderr,"ERROR restore_ed_enable errno=%d\n",errno);
        } else puts("restore_ed_enable=skipped_changed");
    }
    if(args_set && args_valid) {
        // Source-level reset: writing -1 restores all default args and feature switches.
        int a[12];
        if(get_args(a)==0 && a[3]==10 && a[4]==30){
            if(write_text(ARGS,"-1\n")==0)puts("restore_ed_args=defaults");
            else fprintf(stderr,"ERROR restore_ed_args errno=%d\n",errno);
        } else puts("restore_ed_args=skipped_changed");
    }
    if(target_set){
        int v=-1;
        if(read_int(TARGET,&v)==0&&v==fps){
            if(set_int(TARGET,0)==0)puts("restore_target_fps=0");
            else fprintf(stderr,"ERROR restore_target_fps errno=%d\n",errno);
        } else puts("restore_target_fps=skipped_changed");
    }
    if(pid_set){
        if(set_int(GAMEPID,-1)==0)puts("restore_game_pid=-1");
        else fprintf(stderr,"ERROR restore_game_pid errno=%d\n",errno);
    }
}
static int checked_arg(const char*s,int min,int max){
    char *end=NULL;errno=0;long x=strtol(s,&end,10);
    return s[0]&&end&&*end==0&&!errno&&x>=min&&x<=max?(int)x:-1;
}
static void tick(FILE*out,const char*stage,int seq){
    fprintf(out,"%s,%d,%" PRId64 ",%" PRId64 ",%" PRId64 ",%" PRId64 "\n",
      stage,seq,nsec(),freq(6),freq(7),gpu());fflush(out);
}
int main(int argc,char**argv){
    const char* csv=NULL;
    int info=0;
    for(int i=1;i<argc;i++){
        if(!strcmp(argv[i],"--info"))info=1;
        else if(!strcmp(argv[i],"--fps")&&i+1<argc)fps=checked_arg(argv[++i],30,185);
        else if(!strcmp(argv[i],"--seconds")&&i+1<argc)duration=checked_arg(argv[++i],1,5);
        else if(!strcmp(argv[i],"--csv")&&i+1<argc)csv=argv[++i];
        else{fprintf(stderr,"usage: %s [--info] [--fps 30..185] [--seconds 1..5] --csv FILE\n",argv[0]);return 64;}
    }
    if(fps<0||duration<0)return 64;
    me=getpid();
    setvbuf(stdout,NULL,_IOLBF,0);
    prctl(PR_SET_NAME,"fengchi-ed-test",0,0,0);
    signal(SIGINT,on_signal);signal(SIGTERM,on_signal);signal(SIGHUP,on_signal);
    atexit(cleanup);
    int owner=-2,target=-2,ed=-2;
    int has_pid=get_pid(&owner)==0,has_target=read_int(TARGET,&target)==0,has_ed=read_int(ENABLE,&ed)==0;
    args_valid=get_args(original_args)==0;
    printf("pid=%d owner=%d target_fps=%d ed_enable=%d args_valid=%d\n",me,owner,target,ed,args_valid);
    if(info)return 0;
    if(geteuid()!=0){puts("ERROR root required");return 77;}
    if(!csv||!has_pid||!has_target||!has_ed||!args_valid){puts("ERROR required kernel interfaces unreadable");return 75;}
    if(owner!=-1||target!=0||ed!=0||!default_args(original_args)){
        puts("ERROR OEM state not idle/default; refusing to overwrite");
        return 73;
    }
    if(set_int(GAMEPID,me)<0){perror("register game_pid");return 74;}
    int verify=-2;
    if(get_pid(&verify)<0||verify!=me){
        // The test may have partially acquired the PID. Release only if we own it.
        int cur=-2;
        if(get_pid(&cur)==0&&cur==me)set_int(GAMEPID,-1);
        puts("ERROR registration verification failed");return 74;
    }
    pid_set=1;
    if(set_int(TARGET,fps)<0){perror("set target_fps");return 76;}
    target_set=1;
    if(write_text(ARGS,"100 20 -20 10 30 0 0 0 0 0 0 0\n")<0){perror("set ed_args");return 76;}
    args_set=1;
    if(set_int(ENABLE,1)<0){perror("set ed_enable");return 76;}
    enabled=1;
    FILE*out=fopen(csv,"w");if(!out){perror("open csv");return 78;}
    fprintf(out,"phase,seq,monotonic_ns,cpu6_khz,cpu7_khz,gpu_hz\n");
    printf("active_ed_test=1 fps=%d seconds=%d\n",fps,duration);
    int events=0;
    int64_t start=nsec(),period=1000000000LL/fps,next=start;
    for(int i=0;i<fps*duration && keep;i++){
        int cur=-2;
        if(i%fps==0 && (get_pid(&cur)<0||cur!=me)){
            fprintf(stderr,"ERROR ownership lost\n");break;
        }
        char b[90];
        // On each 11th event add a controlled one-frame-long gap to exercise deadline timers.
        int actual_fps=fps-5;if(actual_fps<1)actual_fps=1;
        snprintf(b,sizeof(b),"%" PRIu64 " %d %d %d\n",(uint64_t)nsec(),1+(i&1),actual_fps,2);
        if(write_text(PRODUCE,b)<0){
            fprintf(stderr,"ERROR early_detect frame_produce errno=%d\n",errno);
            write_fail++;break;
        }
        events++;
        if(i%((fps+9)/10)==0)tick(out,"ED",i);
        next+=period;
        if(i>0 && i%11==0)next+=period/2;
        int64_t gap=next-nsec();
        if(gap>0){
            struct timespec sleep_time={gap/1000000000LL,gap%1000000000LL};
            while(nanosleep(&sleep_time,&sleep_time)<0&&errno==EINTR&&keep){}
        }
    }
    tick(out,"END",events);
    fclose(out);
    printf("frame_produce_writes=%d errors=%d interrupted=%d\n",events,write_fail,!keep);
    return events>0&&!write_fail&&keep?0:1;
}
