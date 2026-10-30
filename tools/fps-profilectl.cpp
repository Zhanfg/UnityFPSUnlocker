#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

using namespace rapidjson;
static constexpr const char* PATH="/data/local/tmp/TargetList.json";
static bool validpkg(const std::string& s) {
    if(s.size()<3 || s.size()>220 || s.find('.')==std::string::npos || s[0]=='.')return false;
    for(char c:s)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='.'))return false;
    return s.find("..")==std::string::npos;
}
static bool number(const char* s,int min,int max,int& out) {
    if(!s||!*s)return false;
    for(auto p=s;*p;p++)if(*p<'0'||*p>'9')return false;
    errno=0;char* e=nullptr;long n=strtol(s,&e,10);
    if(errno||*e||n<min||n>max)return false;
    out=static_cast<int>(n);return true;
}
static void ensure(Document& d) {
    auto& a=d.GetAllocator();
    if(!d.IsObject())d.SetObject();
    if(!d.HasMember("global")||!d["global"].IsObject()){
        if(d.HasMember("global"))d.RemoveMember("global");
        Value g(kObjectType);
        g.AddMember("delay",0,a);g.AddMember("fps",0,a);
        g.AddMember("mod_opcode",false,a);g.AddMember("render_interval",0,a);
        g.AddMember("scale",-1.0,a);d.AddMember("global",g,a);
    }
    if(!d.HasMember("custom")||!d["custom"].IsObject()) {
        if(d.HasMember("custom"))d.RemoveMember("custom");
        Value o(kObjectType);d.AddMember("custom",o,a);
    }
}
static bool load(Document& d) {
    FILE* f=fopen(PATH,"rb");
    if(!f){ensure(d);return true;}
    char buffer[16384];FileReadStream stream(f,buffer,sizeof(buffer));
    d.ParseStream(stream);fclose(f);
    if(d.HasParseError()||!d.IsObject())return false;
    ensure(d);return true;
}
static bool write_doc(const Document& d){
    StringBuffer output;Writer<StringBuffer> writer(output);
    if(!d.Accept(writer)||output.GetSize()>1024*1024)return false;
    const char* temp="/data/local/tmp/.TargetList.webui.XXXXXX";
    char path[sizeof("/data/local/tmp/.TargetList.webui.XXXXXX")];
    std::strcpy(path,temp);
    int fd=mkstemp(path);
    if(fd<0)return false;
    fchmod(fd,0600);
    const char* p=output.GetString();size_t left=output.GetSize();bool ok=true;
    while(left){ssize_t n=::write(fd,p,left);if(n<0&&errno==EINTR)continue;if(n<=0){ok=false;break;}p+=n;left-=n;}
    if(ok && fsync(fd)!=0)ok=false;
    if(close(fd)!=0)ok=false;
    if(ok && rename(path,PATH)!=0)ok=false;
    if(!ok)unlink(path);
    return ok;
}
static void put(Value& obj,const char* key,int val,Document::AllocatorType& a){
    if(obj.HasMember(key))obj[key].SetInt(val);
    else{Value k(key,a);obj.AddMember(k,Value(val),a);}
}
static void putbool(Value& obj,const char* key,bool val,Document::AllocatorType& a){
    if(obj.HasMember(key))obj[key].SetBool(val);
    else{Value k(key,a);obj.AddMember(k,Value(val),a);}
}
static void putscale(Value& obj,Document::AllocatorType& a){
    if(obj.HasMember("scale"))return;
    obj.AddMember("scale",Value(-1.0),a);
}
static int integer(const Value& obj,const char* key){
    if(!obj.IsObject()||!obj.HasMember(key)||!obj[key].IsInt())return 0;
    return obj[key].GetInt();
}
static void print(const Document& d) {
    StringBuffer b;Writer<StringBuffer>w(b);d.Accept(w);puts(b.GetString());
}
int main(int argc,char**argv) {
    if(argc<2){fprintf(stderr,"usage: profilectl read|set|remove|runtime\n");return 64;}
    Document d;if(!load(d)){fprintf(stderr,"Invalid existing TargetList JSON; refusing to overwrite\n");return 65;}
    const std::string op=argv[1];
    if(op=="read" && argc==2){print(d);return 0;}
    if(op=="runtime" && argc==2){
        const Value& v=d["custom"];
        for(auto it=v.MemberBegin();it!=v.MemberEnd();++it){
            if(!it->value.IsObject()||!validpkg(it->name.GetString()))continue;
            const int cpu=integer(it->value,"cpu_min_khz");
            const int gpu=integer(it->value,"gpu_min_hz");
            if(cpu>0||gpu>0)printf("%s %d %d\n",it->name.GetString(),cpu,gpu);
        }
        return 0;
    }
    if((op=="set"&&argc!=8)||(op=="remove"&&argc!=3))return 64;
    if(op!="set"&&op!="remove")return 64;
    const std::string pkg=argv[2];
    if(!validpkg(pkg)){fprintf(stderr,"Invalid package name\n");return 65;}
    Value& custom=d["custom"];auto it=custom.FindMember(pkg.c_str());
    if(op=="remove") {
        if(it!=custom.MemberEnd())custom.RemoveMember(it);
    } else {
        int fps,interval,cpu,gpu,delay;
        if(!number(argv[3],0,240,fps)|| (fps!=0&&fps<30) ||
           !number(argv[4],0,1,interval)||
           !number(argv[5],0,5000000,cpu)||
           !number(argv[6],0,300000000,gpu)||
           !number(argv[7],0,15,delay)){
            fprintf(stderr,"Invalid FPS / interval / CPU-kHz / GPU-kHz / delay\n");return 65;
        }
        if(cpu!=0&&cpu<100000)return 65;
        if(gpu!=0&&gpu<10000)return 65;
        if(it==custom.MemberEnd()){
            Value key(pkg.c_str(),d.GetAllocator());Value obj(kObjectType);
            custom.AddMember(key,obj,d.GetAllocator());it=custom.FindMember(pkg.c_str());
        }
        Value& obj=it->value;
        if(!obj.IsObject())obj.SetObject();
        auto& a=d.GetAllocator();
        put(obj,"fps",fps,a);put(obj,"delay",delay,a);
        put(obj,"render_interval",interval,a);
        put(obj,"cpu_min_khz",cpu,a);put(obj,"gpu_min_khz",gpu,a);
        putbool(obj,"mod_opcode",false,a);putscale(obj,a);
    }
    if(!write_doc(d)){perror("Cannot atomically save config");return 74;}
    puts("OK");return 0;
}
