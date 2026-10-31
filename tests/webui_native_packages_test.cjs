"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const vm = require("node:vm");

function fakeElement(tag="div") {
  const el={
    tag,children:[],dataset:{},hidden:false,disabled:false,
    value:"",textContent:"",type:"",
    events:{},className:"",
    classList:{toggle(){},add(){},remove(){}},
    addEventListener(type,fn){this.events[type]=fn;},
    replaceChildren(...nodes){this.children=[...nodes];},
    append(...nodes){this.children.push(...nodes);},
    get options(){return this._options||[];},
    set options(v){this._options=v;}
  };
  return el;
}
async function runScenario(native) {
  const ids=new Map();
  const $=id=>{
    if(!ids.has(id)) ids.set(id,fakeElement());
    return ids.get(id);
  };
  const filters=["games","all","saved"].map(filter=>{
    const el=fakeElement("button");
    el.dataset.filter=filter;
    return el;
  });
  $("fps").options=[{value:"0"},{value:"60"},{value:"120"},{value:"custom"}];
  $("fps").value="0";$("delay").value="0";
  const profile={global:{fps:0,delay:0,render_interval:0,scale:-1},custom:{}};
  const calls=[];
  const rootAPI={
    exec(command,callback){
      calls.push(command);
      const operation=command.match(/profile-ui\.sh' ([a-z-]+)/)?.[1];
      let output="";
      if(operation==="read") output=JSON.stringify(profile);
      if(operation==="opgce-status") output="INSTALLED|0\nENABLED|0\n";
      if(operation==="scrc-status") output="INSTALLED|0\nENABLED|0\n";
      if(operation==="caps") output="CPU|policy0|300000|2700000";
      if(operation==="scan") output="APP|com.example.fallback";
      if(operation==="set"){
        const m=command.match(/set '([^']+)' '([0-9]+)' '([0-9]+)' '([0-9]+)' '([0-9]+)' '([0-9]+)'/);
        assert(m,"unexpected set arguments: "+command);
        profile.custom[m[1]]={
          fps:+m[2],render_interval:+m[3],cpu_min_khz:+m[4],
          gpu_min_mhz:+m[5],delay:+m[6]
        };
        output="OK";
      }
      if(callback){
        const cb=callback;
        queueMicrotask(()=>window[cb](0,output,""));
        return null;
      }
      return output;
    },
    getPackagesInfo(input){return JSON.stringify(JSON.parse(input).map(
      packageName=>({packageName,appLabel:packageName.split(".").at(-1)})
    ));}
  };
  if(native) rootAPI.listUserPackages=()=>JSON.stringify([
    "com.example.ordinary","com.miHoYo.Yuanshen"
  ]);
  const window={ksu:rootAPI};
  const document={
    getElementById:$,
    createElement:fakeElement,
    querySelectorAll(s){return s==="[data-filter]"?filters:[];}
  };
  const context={window,document,console,setTimeout,clearTimeout,confirm:()=>true,
    Promise,Set,Map,Number,Array,JSON,String,Error};
  vm.runInNewContext(fs.readFileSync("webroot/app.js","utf8"),context);
  for(let i=0;i<12;i++)await new Promise(resolve=>setImmediate(resolve));
  if(native){
    assert.equal($("count").textContent,"2/2");
    assert(!calls.some(c=>c.includes(" scan")),
      "native PackageManager should bypass shell scan");
    assert.equal($("apps").children.length,2);
    const item=$("apps").children.find(el=>el.dataset.pkg==="com.example.ordinary");
    assert(item,"unknown game must appear in All Apps");
    item.events.click();
    $("fps").value="120";
    $("interval").checked=false;
    $("delay").value="0";
    $("cpu").value="";$("gpu").value="";
    $("save").events.click();
    for(let i=0;i<10;i++)await new Promise(resolve=>setImmediate(resolve));
    assert.equal(profile.custom["com.example.ordinary"].fps,120);
    assert.match($("notice").textContent,/已验证配置持久化/);
  }else{
    assert.equal($("count").textContent,"1/1");
    assert(calls.some(c=>c.includes(" scan")),"missing shell fallback");
  }
}
(async()=>{
  await runScenario(true);
  await runScenario(false);
  console.log("WebUI native PackageManager + shell fallback + save readback PASS");
})().catch(e=>{console.error(e);process.exitCode=1;});
