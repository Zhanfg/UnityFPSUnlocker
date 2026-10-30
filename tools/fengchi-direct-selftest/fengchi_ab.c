// SPDX-License-Identifier: MIT
// Standalone Fengchi game_opt A/B ioctl experiment for Android AArch64.
// Evidence of kernel frame-event handling is NOT evidence of GPU frame interpolation.
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/ioctl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define PID_PATH "/proc/game_opt/game_pid"
#define SYNC_PATH "/proc/game_opt/frame_sync/sync_ctrl"
#define MAX_CPUS 12

struct frame_data {
    int32_t mode;
    int32_t bufferN;
    long timeStamp1;
    long timeStamp2;
    char reserved[128];
};
_Static_assert(sizeof(struct frame_data) == 152, "Expected Android ARM64 152-byte gameopt_frame_data");
#define FRAME_PRODUCE _IOWR(0xDF,0,struct frame_data)
#define FRAME_CONSUME _IOWR(0xDF,1,struct frame_data)
#define FRAME_STOP _IOWR(0xDF,3,struct frame_data)

static volatile sig_atomic_t running=1;
static int frame_fd=-1,owns_pid=0;
static pid_t test_pid;
static unsigned long long checksum=1;

static void caught(int signo) { (void)signo; running=0; }

static int64_t now_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC,&t);
    return (int64_t)t.tv_sec*1000000000LL+t.tv_nsec;
}
static int file_text(const char* path,char* dst,size_t cap) {
    int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0)return -1;
    ssize_t n=read(fd,dst,cap-1);
    int err=errno;
    close(fd);
    errno=err;
    if(n<0)return -1;
    dst[n]=0;
    return 0;
}
static int current_game_pid(int* pid) {
    char buf[128];
    return file_text(PID_PATH,buf,sizeof(buf))==0 && sscanf(buf,"game_pid=%d",pid)==1 ? 0:-1;
}
static int set_game_pid(int pid) {
    char buf[40];
    int len=snprintf(buf,sizeof(buf),"%d\n",pid);
    int fd=open(PID_PATH,O_WRONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0)return -1;
    ssize_t done=write(fd,buf,(size_t)len);
    int err=errno;
    close(fd);
    errno=err;
    return done==len?0:-1;
}
static int frame_event(unsigned long request,int mode,int buffer) {
    struct frame_data data={0};
    data.mode=mode;
    data.bufferN=buffer;
    data.timeStamp1=(long)now_ns();
    data.timeStamp2=data.timeStamp1;
    return ioctl(frame_fd,request,&data);
}
static void restore(void) {
    if(frame_fd>=0) {
        struct frame_data data={0};
        int rc=ioctl(frame_fd,FRAME_STOP,&data);
        fprintf(stderr,"cleanup_stop_rc=%d errno=%d\n",rc,rc<0?errno:0);
        close(frame_fd);
        frame_fd=-1;
    }
    if(owns_pid) {
        int cur=-99;
        if(current_game_pid(&cur)==0 && cur==test_pid) {
            if(set_game_pid(-1)==0)fprintf(stderr,"cleanup_game_pid=-1\n");
            else fprintf(stderr,"cleanup_game_pid_failed=%d\n",errno);
        } else {
            fprintf(stderr,"cleanup_game_pid=skipped_owner_changed value=%d\n",cur);
        }
        owns_pid=0;
    }
}
static int parse_int(const char* input,int lo,int hi) {
    char* end=NULL;
    errno=0;
    long v=strtol(input,&end,10);
    return input[0] && end && *end==0 && !errno && v>=lo && v<=hi ? (int)v:-1;
}
static long freq_khz(int cpu) {
    char p[160],s[60];
    snprintf(p,sizeof(p),"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq",cpu);
    return file_text(p,s,sizeof(s))==0 ? atol(s):-1;
}
static long gpu_hz(void) {
    char s[60];
    return file_text("/sys/class/kgsl/kgsl-3d0/devfreq/cur_freq",s,sizeof(s))==0 ? atol(s):-1;
}
static long cpu_min_khz(int cpu) {
    char p[160],s[60];
    snprintf(p,sizeof(p),"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_min_freq",cpu);
    return file_text(p,s,sizeof(s))==0 ? atol(s):-1;
}
static long cpu_max_khz(int cpu) {
    char p[160],s[60];
    snprintf(p,sizeof(p),"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq",cpu);
    return file_text(p,s,sizeof(s))==0 ? atol(s):-1;
}
static void sample(FILE* out,const char* phase,int sample_id,int frames_produced,int failures) {
    fprintf(out,"%s,%d,%" PRId64 ",%d,%d,%ld",phase,sample_id,now_ns(),frames_produced,failures,gpu_hz());
    for(int i=0;i<MAX_CPUS;i++)fprintf(out,",%ld",freq_khz(i));
    fputc('\n',out);
    fflush(out);
}
static void burn_us(int usec) {
    int64_t end=now_ns()+(int64_t)usec*1000;
    uint64_t x=checksum;
    do {
        for(int i=0;i<150;i++)x=(x*1664525u+1013904223u)^(x>>11);
    }while(running && now_ns()<end);
    checksum=(unsigned long long)x;
}
static int run_phase(FILE* out,const char* phase,int emit,int fps,int sec,int busy_us) {
    int frames=0,failed=0,samples=0;
    int64_t start=now_ns();
    int64_t interval=1000000000LL/fps;
    int64_t next=start;
    sample(out,phase,samples++,frames,failed);
    for(int i=0;i<fps*sec&&running;i++) {
        int owner=-99;
        if(i%fps==0 && (current_game_pid(&owner)<0 || owner!=test_pid)) {
            fprintf(stderr,"ERROR game_pid_ownership_lost=%d\n",owner);
            running=0;
            break;
        }
        if(emit) {
            if(frame_event(FRAME_PRODUCE,0,1+(i&1))<0) {
                failed++;
                fprintf(stderr,"ERROR produce_ioctl_errno=%d\n",errno);
            } else frames++;
        }
        burn_us(busy_us);
        if(emit) {
            if(frame_event(FRAME_CONSUME,1,1+(i&1))<0) {
                failed++;
                fprintf(stderr,"ERROR consume_ioctl_errno=%d\n",errno);
            }
        }
        next+=interval;
        int64_t remaining=next-now_ns();
        if(remaining>0) {
            struct timespec request={remaining/1000000000LL,remaining%1000000000LL};
            while(nanosleep(&request,&request)<0 && errno==EINTR && running){}
        }
        if(i%((fps+9)/10)==0)sample(out,phase,samples++,frames,failed);
        if(failed>2)break;
    }
    sample(out,phase,samples++,frames,failed);
    fprintf(stderr,"phase=%s mode=%s frames=%d failures=%d duration_ms=%" PRId64 "\n",
        phase,emit?"ioctl":"control",frames,failed,(now_ns()-start)/1000000LL);
    return failed==0 && running ? 0:-1;
}
int main(int argc,char** argv) {
    int fps=60,seconds=3,busy_us=2500,info=0;
    const char* csv=NULL;
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--info"))info=1;
        else if(!strcmp(argv[i],"--fps")&&i+1<argc)fps=parse_int(argv[++i],20,185);
        else if(!strcmp(argv[i],"--seconds")&&i+1<argc)seconds=parse_int(argv[++i],1,6);
        else if(!strcmp(argv[i],"--busy-us")&&i+1<argc)busy_us=parse_int(argv[++i],100,4500);
        else if(!strcmp(argv[i],"--csv")&&i+1<argc)csv=argv[++i];
        else{fprintf(stderr,"Usage: %s [--info] [--fps 20-185] [--seconds 1-6] [--busy-us 100-4500] --csv PATH\n",argv[0]);return 64;}
    }
    if(fps<0||seconds<0||busy_us<0)return 64;
    setvbuf(stdout,NULL,_IOLBF,0);
    test_pid=getpid();
    prctl(PR_SET_NAME,"fengchi-ab",0,0,0);
    signal(SIGINT,caught);signal(SIGTERM,caught);signal(SIGHUP,caught);
    atexit(restore);
    printf("test_pid=%ld ioctl_produce=0x%lx abi=%zu\n",(long)test_pid,(unsigned long)FRAME_PRODUCE,sizeof(struct frame_data));
    int original=-99;
    int got=current_game_pid(&original);
    printf("original_game_pid=%d available=%d\n",original,got==0);
    printf("frame_sync_present=%d geas_present=%d\n",access(SYNC_PATH,F_OK)==0,access("/proc/game_opt/geas_ctrl",F_OK)==0);
    if(info)return 0;
    if(geteuid()!=0){puts("ERROR need_root");return 77;}
    if(!csv){puts("ERROR need_csv_path");return 64;}
    if(got<0){puts("ERROR game_pid_unavailable");return 72;}
    if(original!=-1){printf("ERROR game_pid_already_in_use=%d\n",original);return 73;}
    if(access(SYNC_PATH,R_OK)!=0){printf("ERROR frame_sync_not_accessible_errno=%d\n",errno);return 74;}
    if(set_game_pid(test_pid)<0){printf("ERROR register_game_pid_errno=%d\n",errno);return 75;}
    int owner=-99;
    if(current_game_pid(&owner)<0 || owner!=test_pid){printf("ERROR game_pid_register_mismatch=%d\n",owner);return 76;}
    owns_pid=1;
    frame_fd=open(SYNC_PATH,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(frame_fd<0){printf("ERROR open_frame_sync_errno=%d\n",errno);return 77;}
    FILE* out=fopen(csv,"w");
    if(!out){printf("ERROR open_csv_errno=%d\n",errno);return 78;}
    setvbuf(out,NULL,_IOLBF,0);
    fprintf(out,"phase,sample,monotonic_ns,produce_ok,failures,gpu_hz");
    for(int i=0;i<MAX_CPUS;i++)fprintf(out,",cpu%d_khz",i);
    fputc('\n',out);
    printf("registered_game_pid=%ld\n",(long)test_pid);
    printf("test_config fps=%d seconds_per_phase=%d busy_us_per_frame=%d\n",fps,seconds,busy_us);
    printf("cpu6_policy_min=%ld max=%ld cpu7_policy_min=%ld max=%ld\n",
        cpu_min_khz(6),cpu_max_khz(6),cpu_min_khz(7),cpu_max_khz(7));
    // BAAB order: control, ioctl, ioctl, control counters time-dependent throttling.
    int ok=0;
    if(run_phase(out,"B1",0,fps,seconds,busy_us)<0)ok=1;
    if(!ok && run_phase(out,"A1",1,fps,seconds,busy_us)<0)ok=1;
    if(!ok && run_phase(out,"A2",1,fps,seconds,busy_us)<0)ok=1;
    if(!ok && run_phase(out,"B2",0,fps,seconds,busy_us)<0)ok=1;
    fclose(out);
    printf("checksum=%llu result=%s\n",checksum,ok?"failed":"complete");
    return ok?1:0;
}
