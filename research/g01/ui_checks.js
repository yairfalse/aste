// Node-only interaction model tests. Not a browser layout/accessibility audit.
'use strict';
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
class Element {constructor(tag){this.tag=tag;this.children=[];this.attrs={};this.selectedIndex=0;this.style={};this.value='0'}append(...children){this.children.push(...children)}replaceChildren(...children){this.children=children}setAttribute(k,v){this.attrs[k]=v}set innerHTML(s){this.html=s;this.children=[]}get innerHTML(){return this.html||''}click(){if(this.onclick)this.onclick()}}
const elements=new Map();const get=id=>{if(!elements.has(id))elements.set(id,new Element(id));return elements.get(id)};
const document={querySelector:get,createElement:tag=>new Element(tag),createElementNS:(_,tag)=>new Element(tag)};
const code=fs.readFileSync(__dirname+'/interface.html','utf8').split('<script>')[1].split('</script>')[0];const context={document,console,Blob,URL,setTimeout};vm.createContext(context);vm.runInContext(code,context);
assert.equal(get('#nodes').children.length,3);assert.equal(get('#graph').children.filter(c=>c.tag==='path').length,2);
get('#scene').selectedIndex=2;get('#scene').onchange();assert.equal(get('#graph').children.filter(c=>c.tag==='path').length,7);
get('#from').selectedIndex=1;get('#to').selectedIndex=2;get('#connect').click();assert.equal(vm.runInContext('model.routes[2][1]',context),.5);
get('#nodes').children[2].click();assert.equal(vm.runInContext('selected',context),2);get('#nodeFreeze').click();assert.equal(vm.runInContext('model.nodes[2].freeze',context),true);
get('#freeze').click();assert.equal(vm.runInContext('model.freeze',context),true);
get('#clear').click();assert.match(get('#feedback').textContent,/prototype only/);
get('#mutate').click();assert.ok(vm.runInContext('model.nodes[2].position>0',context));
assert.equal(get('#matrix').children[0].children.length,3);
console.log('G01 UI model: graph/matrix routing, selection, freeze, seeded mutation and clear feedback passed');
