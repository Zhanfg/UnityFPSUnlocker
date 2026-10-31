"use strict";
(() => {
  const ROOT = "/data/adb/modules/unityfpsunlocker_oemsafe";
  const BRIDGE = ROOT + "/bin/profile-ui.sh";
  const $ = id => document.getElementById(id);
  const validPkg = pkg => /^[A-Za-z0-9_]+(?:\.[A-Za-z0-9_]+)+$/.test(pkg) && pkg.length <= 220;
  const state = { apps:[], names:{}, icons:{}, selected:"", profiles:{}, profileReady:false, filter:"all", caps:"", opgce:{installed:false,enabled:false,version:"",mode:"",active:0,catalog:new Set()}, scrc:{installed:false,enabled:false,version:"",count:0,dual:false,opgs:"",kernel:"",catalog:new Set()} };
  const notice = (message, error=false) => { $("notice").textContent=message; $("notice").classList.toggle("error",error); };
  const shellEscape = s => "'" + String(s).replace(/'/g, "'\\''") + "'";
  let requestSeq = 0;
  // KSU-Next's async exec callback keeps the WebUI responsive while the
  // per-app PackageManager scan or SurfaceFlinger evidence capture runs.
  // Standard synchronous exec remains available for short, local reads.
  async function root(cmd, longRunning=false) {
    const api = window.ksu || window.apatch;
    if (!api || typeof api.exec !== "function")
      throw new Error("KernelSU/APatch Root WebUI 接口不可用，请在模块管理器内打开。");
    const command = "sh " + shellEscape(BRIDGE) + " " + cmd;
    if (longRunning && api === window.ksu) {
      const callback = "__ufps_call_" + (++requestSeq);
      return new Promise((resolve, reject) => {
        const timeout = setTimeout(() => {
          delete window[callback];
          reject(new Error("Root 操作超时；可重新扫描或使用手动包名。"));
        }, 45000);
        let settled = false;
        const finish = (code, stdout, stderr) => {
          if (settled) return;
          settled = true;
          clearTimeout(timeout);
          delete window[callback];
          if (Number(code) !== 0) reject(new Error(String(stderr || stdout || "Root 任务失败").trim()));
          else resolve(String(stdout || "").trim());
        };
        window[callback] = finish;
        try {
          // Native KernelSU WebUI expects a global callback *name*, not a JS function.
          const returned = api.exec(command, callback);
          // Older managers only support synchronous exec(command).
          if (typeof returned === "string" && returned.length)
            finish(0, returned, "");
          else if (returned && typeof returned === "object" && "errno" in returned)
            finish(returned.errno, returned.stdout || "", returned.stderr || "");
        } catch (e) {
          clearTimeout(timeout);
          delete window[callback];
          reject(e);
        }
      });
    }
    // Native ksu.exec returns stdout as a string. A bundled JS facade may
    // instead return {errno, stdout, stderr}. Handle both without eval.
    const result = await Promise.resolve(api.exec(command));
    if (result && typeof result === "object") {
      if ("errno" in result && Number(result.errno) !== 0)
        throw new Error(String(result.stderr || "Root exec failed"));
      return String(result.stdout ?? result.output ?? "").trim();
    }
    if (typeof result !== "string") throw new Error("无法识别 Root 命令输出");
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
    state.profileReady=true;
  }
  async function loadOpgce() {
    try {
      const status = await root("opgce-status");
      const values = {};
      for (const line of words(status)) {
        const divider = line.indexOf("|");
        if (divider > 0) values[line.slice(0, divider)] = line.slice(divider + 1);
      }
      const catalog = new Set();
      if (values.ENABLED === "1") {
        const lines = await root("opgce-list");
        for (const entry of words(lines)) {
          const idx = entry.indexOf("|");
          if (idx < 0) continue;
          const type = entry.slice(0, idx), pkg = entry.slice(idx + 1);
          if ((type === "OPGCE" || type === "OPGCE_ALIAS") && validPkg(pkg)) catalog.add(pkg);
        }
      }
      state.opgce = {
        installed:values.INSTALLED === "1",
        enabled:values.ENABLED === "1",
        version:values.VERSION || "unknown",
        mode:values.MODE || "unavailable",
        active:Number(values.ACTIVE_PROFILES) || 0,
        catalog
      };
      $("opgceStatus").textContent = !state.opgce.installed
        ? "未检测到 OPGCE · 本模块使用独立频率策略"
        : state.opgce.enabled
          ? "OPGCE " + state.opgce.version + " 已启用 · 已部署 " + state.opgce.active
            + " 个策略 · powercfg 状态：" + state.opgce.mode
          : "检测到 OPGCE " + state.opgce.version + "，但模块已禁用";
      renderApps();
      renderEditor();
    } catch (e) {
      $("opgceStatus").textContent="OPGCE 状态不可用："+String(e.message||e);
    }
  }
  async function loadScrc() {
    try {
      const output = await root("scrc-status");
      const values = {};
      for (const line of words(output)) {
        const p = line.indexOf("|");
        if (p > 0) values[line.slice(0, p)] = line.slice(p + 1);
      }
      const catalog = new Set();
      if (values.ENABLED === "1") {
        for (const line of words(await root("scrc-list"))) {
          if (!line.startsWith("SCRC|")) continue;
          const pkg = line.slice(5);
          if (validPkg(pkg)) catalog.add(pkg);
        }
      }
      state.scrc = {
        installed:values.INSTALLED === "1", enabled:values.ENABLED === "1",
        version:values.VERSION || "unknown", count:Number(values.DEPLOYED_PROFILES) || 0,
        dual:values.DUAL_INJECTORS === "1", opgs:values.OPGS_NODE || "?",
        kernel:values.OPGS_KERNEL_FAMILY || "unknown", catalog
      };
      $("scrcStatus").textContent = !state.scrc.installed
        ? "未安装 SCRC"
        : state.scrc.enabled
          ? "SCRC " + state.scrc.version + " 已启用 · 部署 " + state.scrc.count
            + " 份配置 · OPGS 节点：" + state.scrc.opgs + " · 内核：" + state.scrc.kernel
          : "SCRC " + state.scrc.version + " 已安装但未启用";
      $("scrcWarning").hidden = !state.scrc.dual;
      $("scrcWarning").textContent = state.scrc.dual
        ? "警告：SCRC 与 OPGCE 同时启用，两个注入器都可能修改 COSA 数据库。建议只保留一个 COSA 注入器；本模块不会自动处理其数据库。"
        : "";
      renderApps();
      renderEditor();
    } catch (e) {
      $("scrcStatus").textContent = "SCRC 状态不可用：" + String(e.message || e);
    }
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
      if(state.filter==="games" && !a.game && !saved(a.pkg) && !state.opgce.catalog.has(a.pkg) && !state.scrc.catalog.has(a.pkg))return false;
      return !search || (label(a.pkg)+" "+a.pkg).toLowerCase().includes(search);
    });
    const list=$("apps");list.replaceChildren();
    for(const a of visible) {
      const item=element("button","app"+(a.pkg===state.selected?" active":""));
      item.type="button";item.dataset.pkg=a.pkg;
      const badge=element("div","appbadge","◈");
      const txt=element("div","apptext");
      txt.append(element("strong","",label(a.pkg)),element("small","",a.pkg));
      const opgceMatch=state.opgce.enabled && state.opgce.catalog.has(a.pkg);
      const scrcMatch=state.scrc.enabled && state.scrc.catalog.has(a.pkg);
      const tag=element("div","appflag",saved(a.pkg)?"已配置":scrcMatch?"SCRC":opgceMatch?"OPGCE":a.game?"游戏候选":"");
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
    $("remove").disabled=!state.profileReady || !saved(pkg);
    $("save").disabled=!state.profileReady;
    $("evidence").hidden=true;
    $("diagOutput").hidden=true;
    $("scrcProfile").textContent = state.scrc.enabled
      ? state.scrc.catalog.has(pkg)
        ? "发现此游戏的 SCRC 部署配置。本模块停止对 CPU/GPU 最低频率节点写入，继续独立管理 Unity FPS。"
        : "SCRC 已启用。按其上游兼容要求，本模块对所有游戏停用 CPU/GPU 最低频率写入。"
      : "SCRC 未启用，调度能力由独立配置或 OPGCE 处理。";
    const opgceOwns = state.opgce.enabled && state.opgce.catalog.has(pkg);
    $("opgceProfile").textContent = opgceOwns
      ? "已发现该游戏在 OPGCE 部署策略/别名中：其调度优先，本模块将跳过 CPU/GPU 频率下限写入。"
      : state.opgce.enabled
        ? "OPGCE 已安装，但未发现该游戏的已部署策略；仍可使用本模块独立的频率控制。"
        : "未启用 OPGCE；独立频率控制按用户配置执行。";
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
      const committed = state.profiles[pkg];
      if(!committed || committed.fps!==fps ||
         committed.render_interval!==interval ||
         committed.cpu_min_khz!==cpu ||
         committed.gpu_min_mhz!==gpu ||
         committed.delay!==delay) {
        throw new Error("保存后复读校验失败：配置没有完整持久化，请勿认为已生效。");
      }
      renderApps();renderEditor();
      notice("已验证配置持久化：" + label(pkg) + " → " + (fps===0?"不调整帧率":fps+" FPS") +
        "。接下来必须完全结束并重新打开游戏；这是配置成功，不代表 Unity Hook 或实际 FPS 已生效。请使用“检查游戏注入状态”。");
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
  async function diagnose() {
    const pkg=state.selected;
    if(!validPkg(pkg)) throw new Error("请先选择已安装的游戏。");
    $("diagnose").disabled=true;
    $("diagOutput").hidden=false;
    $("diagOutput").textContent="正在读取游戏进程及 Unity Hook 日志…";
    try {
      const output=await root("diagnose "+shellEscape(pkg),true);
      const profile=state.profiles[pkg];
      const summary = profile
        ? "已保存 FPS=" + (profile.fps||0) + "，配置存在不等于 FPS 生效。\n"
        : "尚无该游戏的已保存配置；请先设定目标 FPS 并保存。\n";
      $("diagOutput").textContent=summary+(output||"未读取到游戏状态");
      if(/PROCESS\|not-running/.test(output))
        notice("未发现运行中的游戏进程：请打开游戏，再执行注入检查。",true);
      else if(/IL2CPP\|not-observed/.test(output))
        notice("未发现 libil2cpp.so：游戏可能不是 Unity IL2CPP，当前原生拦截路径不会生效。",true);
      else if(/IL2CPP\|loaded/.test(output))
        notice("Unity IL2CPP 已加载。继续查看诊断输出中的 ARMED、HIT、NO_HIT 日志；只有 HIT 才表示曾拦截原生设置调用。");
      else notice("已读取设备运行信息，但未能确认 Unity IL2CPP / Hook 命中状态。");
    } finally {
      $("diagnose").disabled=false;
    }
  }
  async function measure() {
    const pkg = state.selected;
    if (!validPkg(pkg)) throw new Error("请先选择一个游戏。");
    $("measure").disabled = true;
    $("evidence").hidden = false;
    $("evidence").textContent = "请立即切回游戏！5 秒后开始采集 SurfaceFlinger 帧率，保持前台运行至少 5 秒。";
    try {
      const result = await root("measure " + shellEscape(pkg), true);
      $("evidence").textContent = result || "采集任务已完成，但未输出数据。";
      notice("帧率证据已采集。请查看下方结果和设备 Download 目录中的归档。");
    } catch (e) {
      $("evidence").textContent = String(e.message || e);
      throw e;
    } finally {
      $("measure").disabled = false;
    }
  }
  function gameHint(pkg) {
    return /game|unity|unreal|mihoyo|hoyoverse|tmgp|netease|roblox|supercell|kurogame|hypergryph|yostar|bilibili/i.test(pkg);
  }
  // The manager has a native Android PackageManager cache.
  async function nativePackages() {
    const api=window.ksu || window.apatch;
    if (!api) return [];
    for (const [method,args] of [
      ["listUserPackages",[]],["listPackages",["user"]],["listAllPackages",[]]
    ]) {
      if (typeof api[method] !== "function") continue;
      try {
        const raw=await Promise.resolve(api[method](...args));
        const list=typeof raw==="string"?JSON.parse(raw):raw;
        if (!Array.isArray(list)) continue;
        const pkgs=list.filter(p=>typeof p==="string"&&validPkg(p));
        if (pkgs.length) return [...new Set(pkgs)];
      } catch (_) { /* try another manager interface */ }
    }
    return [];
  }
  async function scan() {
    $("rescan").disabled=true;
    notice("正在读取已安装应用…");
    try {
      const native=await nativePackages();
      const apps=[];
      let origin="";
      if(native.length) {
        for(const pkg of native) apps.push({pkg,game:gameHint(pkg)});
        origin="KernelSU 应用管理接口";
      } else {
        // Root PackageManager fallback for older WebUI managers.
        const result=await root("scan",true);
        for(const line of words(result)){
          const p=line.indexOf("|");if(p<0)continue;
          const tag=line.slice(0,p),pkg=line.slice(p+1);
          if(validPkg(pkg)) apps.push({pkg,game:tag==="GAME"||gameHint(pkg)});
        }
        origin="Root PackageManager";
      }
      state.apps=apps;
      if(!apps.some(a=>a.game)){
        state.filter="all";
        for(const f of document.querySelectorAll("[data-filter]"))
          f.classList.toggle("on",f.dataset.filter==="all");
      }
      renderApps();
      if(!apps.length) {
        notice("扫描为空：管理器原生接口及 Root PackageManager 都未返回应用。可手动输入包名，但这不代表 FPS 已注入。",true);
      } else {
        notice("从 "+origin+" 找到 "+apps.length+" 个应用。游戏识别仅作候选；切换“全部应用”可查看完整列表。");
      }
      enrich(apps.map(a=>a.pkg));
    } catch(e) {
      notice("扫描失败："+String(e.message||e)+"。可以尝试手动包名。",true);
    } finally {
      $("rescan").disabled=false;
    }
  }
  async function guard(fn) {
    try{await fn();}catch(e){notice(e.message||String(e),true);}
  }
  $("fps").addEventListener("change",()=>$("customFpsWrap").hidden=$("fps").value!=="custom");
  $("search").addEventListener("input",renderApps);
  $("rescan").addEventListener("click",()=>guard(scan));
  $("measure").addEventListener("click",()=>guard(measure));
  $("diagnose").addEventListener("click",()=>guard(diagnose));
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
  guard(async()=>{
    let profileFailure = null;
    try { await loadProfiles(); } catch (e) {
      profileFailure = e;
      state.profileReady = false;
      state.profiles = {};
    }
    await loadCaps();
    await loadOpgce();
    await loadScrc();
    await scan();
    if (profileFailure) {
      notice("游戏列表可用，但配置程序异常；请安装 hotfix1 或在 Root Shell 执行 chmod 755 /data/adb/modules/unityfpsunlocker_oemsafe/bin/fps-profilectl。详情：" + String(profileFailure.message || profileFailure), true);
      renderEditor();
    }
  });
})();