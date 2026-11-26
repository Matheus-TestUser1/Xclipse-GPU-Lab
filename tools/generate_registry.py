#!/usr/bin/env python3
"""Generate read-only Vulkan feature/property queries from the pinned Khronos registry."""
import sys, re, xml.etree.ElementTree as ET
from collections import defaultdict
xml, output = sys.argv[1:]
r = ET.parse(xml).getroot()
def vk(n): return not n.get('api') or 'vulkan' in n.get('api').split(',')
def name(t): return t.get('name') or t.findtext('name')
types = {name(t):t for t in r.findall('./types/type') if vk(t)}
def canonical(n):
    while n in types and types[n].get('alias'): n=types[n].get('alias')
    return n
owners=defaultdict(set); cores=defaultdict(set)
for f in r.findall('feature'):
    if not vk(f): continue
    for req in f.findall('require'):
        if not vk(req): continue
        for t in req.findall('type'):
            if vk(t): cores[canonical(t.get('name'))].add(f.get('number'))
for e in r.findall('./extensions/extension'):
    if 'vulkan' not in e.get('supported','').split(',') or e.get('type')!='device' or e.get('platform') or e.get('protect') or e.get('provisional')=='true': continue
    for req in e.findall('require'):
        if not vk(req) or req.get('depends') or req.get('protect'): continue
        for t in req.findall('type'):
            if vk(t): owners[canonical(t.get('name'))].add(e.get('name'))
lines=['// Generated from vk.xml. Queries are independent, never enable features.', 'static void registry_scan(std::ostream& o, VkPhysicalDevice d, uint32_t a, const std::function<bool(const char*)>& has) {', 'o << "\\n[Registry audit: feature/property structures]\\n";', 'if(a < VK_API_VERSION_1_1) {o << "INCONCLUSIVO: Vulkan 1.1 necessario para este scanner\\n";return;}']
count=0
for n,t in sorted(types.items(), key=lambda x:x[0] or ''):
    if t.get('category')!='struct' or t.get('alias') or t.get('protect'): continue
    parents=t.get('structextends','').split(',')
    feature='VkPhysicalDeviceFeatures2' in parents
    prop='VkPhysicalDeviceProperties2' in parents
    if not feature and not prop: continue
    members=[m for m in t.findall('member') if vk(m)]
    st=next((m.get('values') for m in members if m.findtext('name')=='sType'),None)
    if not st: continue
    fields=[m for m in members if m.findtext('name') not in ('pNext','sType')]
    if any('*' in ''.join(m.itertext()).split(m.findtext('name') or '?')[0] for m in fields): continue
    os=sorted(owners[n]); cv=sorted(cores[n],key=lambda x:tuple(map(int,x.split('.'))))
    # Aggregate Vulkan11/12 feature structs were introduced in 1.2.
    if n in ('VkPhysicalDeviceVulkan11Features','VkPhysicalDeviceVulkan11Properties'): cv=['1.2']
    conditions=[f'has("{x}")' for x in os]
    if cv:
        ma,mi=cv[0].split('.'); conditions.append(f'a >= VK_MAKE_API_VERSION(0,{ma},{mi},0)')
    if not conditions: continue
    count+=1
    owner=','.join(os) or 'CORE'
    lines += [f'if ({" || ".join(conditions)}) {{',f'  {n} v{{{st}}};']
    if feature: lines += ['  VkPhysicalDeviceFeatures2 b{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2}; b.pNext=&v; vkGetPhysicalDeviceFeatures2(d,&b);']
    else: lines += ['  VkPhysicalDeviceProperties2 b{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2}; b.pNext=&v; vkGetPhysicalDeviceProperties2(d,&b);']
    lines += [f'  o << "STRUCT {n} | {owner}\\n";']
    bools=[]
    for m in fields:
        mn=m.findtext('name'); mt=m.findtext('type'); tail=m.find('name').tail or ''
        if not mn: continue
        if mt=='VkBool32' and '[' not in tail: bools.append(mn)
        # Generic C++ overloads print scalars and arrays; aggregates explicitly marked.
        lines += [f'  registry_field(o,"{mn}",v.{mn});']
    if feature and bools:
        lines += ['  unsigned yes=0,no=0,invalid=0;']
        for b in bools: lines += [f'  if(v.{b}==VK_TRUE) ++yes; else if(v.{b}==VK_FALSE) ++no; else ++invalid;']
        lines += [f'  o << "AUDIT|{n}|{owner}|" << yes << "|" << no << "|" << invalid << "\\n";']
    lines += ['}']
lines += ['o << "Registry structures generated: '+str(count)+' (not all are applicable to this GPU)\\n";', '}']
open(output,'w').write('\n'.join(lines)+'\n')
print(f'{count} registry queries generated')
