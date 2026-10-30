"use strict";
(() => {
  const ROOT = "/data/adb/modules/unityfpsunlocker_oemsafe";
  const BRIDGE = ROOT + "/bin/profile-ui.sh";
  const $ = id => document.getElementById(id);
  const validPkg = pkg => /^[A-Za-z0-9_]+(?:\.[A-Za-z0-9_]+)+$/.test(pkg) && pkg.length <= 220;
  const state = { apps:[], names:{}, icons:{}, selected:"", profiles:{}, filter:"games", caps:"" };
  const notice = (message, error=false) => { $("notice").textContent=message; $("notice").classList.toggle("error",error); };
  const shellEscape = s => "'" + String(s).replace(/'/g, "'\\''") + "'";
  async function root(cmd) {
    const api = window.ksu || window.apatch;
    if (!api || typeof api.exec !== "function") throw new Error("KernelSU/APatch WebUI Root 接口不可用。请在支持 WebUI 的管理器内打开模块。");
    let result = await Promise.resolve(api.exec("sh "+shellEscape(BRIDGE)+" "+cmd));
    if (result && typeof result === "object") {
      if ("errno" in result && Number(result.errno)!==0)
        throw new Error(result.stderr || "Root exec errno=" + result.errno);
      result = result.stdout ?? result.output ?? "";
    }
    if (typeof result !== "string") throw new Error("无法识别 WebUI exec 返回值");
    return result.trim();
  }
  const words = data => data.split(/\r?\n/).map(s=>s.trim()).filter(Boolean);
  const saved = pkg => Object.prototype.hasOwnProperty.call(state.profiles,pkg);
  function element(tag,cls,text) {
    const e=document.createElement(tag);if(cls)e.className=cls;if(text!==undefined)e.textContent=text;return e;
  }
  function label(pkg) { return state.names[pkg] || pkg; }
  async function enrich(pkgs) {
    const api=window.ksu;
    if(!api || typeof api.getPackagesInfo!=="function")return;
    for(let i=0;i<pkgs.length;i+=40) {
      try{
        const raw=await Promise.resolve(api.getPackagesInfo(JSON.stringify(pkgs.slice(i,i+40))));
        const infos=typeof raw==="string"?JSON.parse(raw):raw;
        if(!Array.isArray(infos))continue;
        for(const inf of infos) {
          if(inf && validPkg(inf.packageName) && inf.appLabel)
            state.names[inf.packageName]=String(inf.appLabel);
        }
      }catch(_){/* API optional on older KernelSU */}
    }
    renderApps();renderEditor();
  }
  async function loadProfiles() {
    const raw=await root("read");
    let data;try {data=JSON.parse(raw);}catch(_){throw new Error("设备 TargetList.json 无法解析；已拒绝覆盖原配置");}
    state.profiles=(data && data.custom && typeof data.custom==="object")?data.custom:{};
  }
  async function loadCaps() {
    try{
      state.caps=await root("caps");
      const lines=words(state.caps);
      $("caps").textContent=lines.length?lines.map(line=>{
        const [type,device,minimum,maximum]=line.split("|");
        if(type==="CPU")return device+" · "+(+minimum/1000).toFixed(0)+"–"+(+maximum/1000).toFixed(0)+" MHz";
        if(type==="GPU"){
          const factor=(+maximum>10000000)?1e6:1000;
          return "GPU · "+(+minimum/factor).toFixed(0)+"–"+(+maximum/factor).toFixed(0)+" MHz";
        }
        return line;
      }).join("\n"):"内核未公开可查询的频率范围；频率锁定可能不可用。";
    }catch(e){$("caps").textContent="频率能力查询不可用："+e.message;}
  }
  function renderApps() {
    const search=$("search").value.toLowerCase().trim();
    const all=[...state.apps];
    for(const pkg of Object.keys(state.profiles))
      if(validPkg(pkg) && !all.some(a=>a.pkg===pkg))all.push({pkg,game:true});
    all.sort((a,b)=>Number(saved(b.pkg))-Number(saved(a.pkg)) ||
      Number(b.game)-Number(a.game) || label(a.pkg).localeCompare(label(b.pkg),"zh-CN"));
    const visible=all.filter(a=>{
      if(state.filter==="saved" && !saved(a.pkg))return false;
      if(state.filter==="games" && !a.game && !saved(a.pkg))return false;
      return !search || (label(a.pkg)+" "+a.pkg).toLowerCase().includes(search);
    });
    const list=$("apps");list.replaceChildren();
    for(const a of visible) {
      const item=element("button","app"+(a.pkg===state.selected?" active":""));
      item.type="button";item.dataset.pkg=a.pkg;
      const badge=element("div","appbadge","◈");
      const txt=element("div","apptext");
      txt.append(element("strong","",label(a.pkg)),element("small","",a.pkg));
      const tag=element("div","appflag",saved(a.pkg)?"已配置":a.game?"游戏候选":"");
      item.append(badge,txt,tag);
      item.addEventListener("click",()=>select(a.pkg));
      list.append(item);
    }
    if(!visible.length)list.append(element("div","empty","没有匹配的应用，试试“全部应用”或手动包名。"));
    $("count").textContent=visible.length+"/"+all.length;
  }
  async function showIcon(pkg) {
    $("selectedIcon").replaceChildren();$("selectedIcon").textContent="◈";
    const api=window.ksu;
    if(!api || typeof api.getPackagesIcons!=="function")return;
    try {
      const raw=await Promise.resolve(api.getPackagesIcons(JSON.stringify([pkg]),64));
      const list=typeof raw==="string"?JSON.parse(raw):raw;
      if(state.selected!==pkg || !Array.isArray(list) || !list[0]?.icon)return;
      const icon=String(list[0].icon);
      let src="";
      if(/^data:image\/png;base64,[A-Za-z0-9+/=]+$/.test(icon))src=icon;
      else if(/^[A-Za-z0-9+/=]+$/.test(icon))src="data:image/png;base64,"+icon;
      if(src){const img=element("img");img.alt="";img.src=src;$("selectedIcon").replaceChildren(img);}
    }catch(_){/* optional native icon API */}
  }
  function select(pkg) {
    if(!validPkg(pkg)){notice("无效的 Android 包名",true);return;}
    state.selected=pkg;
    renderApps();renderEditor();showIcon(pkg);
  }
  function renderEditor() {
    const pkg=state.selected;const active=!!pkg;
    $("empty").hidden=active;$("fields").hidden=!active;
    $("status").textContent=!active?"未选择":saved(pkg)?"已保存":"未配置";
    if(!active)return;
    $("appname").textContent=label(pkg);$("pkgname").textContent=pkg;
    const p=state.profiles[pkg]||{};
    const fps=Number.isInteger(p.fps)?p.fps:0;
    const known=Array.from($("fps").options).some(o=>o.value!=="custom"&&Number(o.value)===fps);
    $("fps").value=known?String(fps):"custom";
    $("customFpsWrap").hidden=known;
    $("customFps").value=fps||120;
    $("interval").checked=p.render_interval===1;
    $("delay").value=Number.isInteger(p.delay)?p.delay:0;
    $("cpu").value=p.cpu_min_khz?Math.round(p.cpu_min_khz/1000):"";
    $("gpu").value=p.gpu_min_mhz||"";
    $("remove").disabled=!saved(pkg);
  }
  function numberField(id,min,max) {
    const v=$(id).value.trim();
    if(!v)return 0;
    const n=Number(v);
    if(!Number.isInteger(n)||n<min||n>max)throw new Error(id+"：范围 "+min+"–"+max+"，必须输入整数");
    return n;
  }
  async function save() {
    const pkg=state.selected;if(!validPkg(pkg))return;
    const fps=$("fps").value==="custom"?numberField("customFps",30,240):Number($("fps").value);
    const interval=$("interval").checked?1:0;
    const cpu=numberField("cpu",0,5000)*1000;
    const gpu=numberField("gpu",0,3000);
    if(cpu>0&&cpu<100000)throw new Error("CPU 最低频率须 ≥ 100 MHz");
    if(gpu>0&&gpu<100)throw new Error("GPU 最低频率须 ≥ 100 MHz");
    const delay=numberField("delay",0,15);
    const args=[pkg,fps,interval,cpu,gpu,delay].map(v=>shellEscape(String(v))).join(" ");
    $("save").disabled=true;
    try {
      const result=await root("set "+args);
      if(result!=="OK")throw new Error(result||"保存失败");
      await loadProfiles();
      renderApps();renderEditor();
      notice("已固定 "+label(pkg)+" 的配置。帧率调整须重启该游戏；频率下限由守护进程自动应用。");
    }finally{$("save").disabled=false;}
  }
  async function remove() {
    const pkg=state.selected;if(!saved(pkg))return;
    if(!confirm("移除 "+label(pkg)+" 的所有帧率与性能配置？"))return;
    const result=await root("remove "+shellEscape(pkg));
    if(result!=="OK")throw new Error(result||"删除失败");
    await loadProfiles();renderApps();renderEditor();
    notice("已移除配置。请重启游戏以移除该进程中的 Native Hook。");
  }
  async function scan() {
    $("rescan").disabled=true;notice("正在扫描用户安装的应用并识别游戏候选…");
    try{
      const result=await root("scan");
      const apps=[];
      for(const line of words(result)){
        const p=line.indexOf("|");if(p<0)continue;
        const tag=line.slice(0,p),pkg=line.slice(p+1);
        if(validPkg(pkg))apps.push({pkg,game:tag==="GAME"});
      }
      state.apps=apps;
      renderApps();
      notice("扫描到 "+apps.length+" 个应用，游戏候选为启发式识别；切换“全部应用”可选择任何游戏。");
      // Get names when native KSU Next supports app-label lookup.
      enrich(apps.map(a=>a.pkg));
    }catch(e){notice("扫描失败："+e.message,true);}
    finally{$("rescan").disabled=false;}
  }
  async function guard(fn) {
    try{await fn();}catch(e){notice(e.message||String(e),true);}
  }
  $("fps").addEventListener("change",()=>$("customFpsWrap").hidden=$("fps").value!=="custom");
  $("search").addEventListener("input",renderApps);
  $("rescan").addEventListener("click",()=>guard(scan));
  $("save").addEventListener("click",()=>guard(save));
  $("remove").addEventListener("click",()=>guard(remove));
  $("manualadd").addEventListener("click",()=>select($("manualpkg").value.trim()));
  $("restore").addEventListener("click",()=>guard(async()=>{
    const r=await root("restore");if(r!=="OK")throw new Error(r||"恢复操作失败");
    notice("已请求恢复模块写入的 CPU/GPU 最低频率。");
  }));
  for(const f of document.querySelectorAll("[data-filter]"))f.addEventListener("click",()=>{
    state.filter=f.dataset.filter;
    for(const other of document.querySelectorAll("[data-filter]"))other.classList.toggle("on",other===f);
    renderApps();
  });
  guard(async()=>{await loadProfiles();await loadCaps();await scan();});
})();