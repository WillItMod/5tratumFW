// A settings page may submit only its own changed fields. Array placeholders
// retain the physical pool/fan index for the firmware's positional PATCH API.
export const SECTION_FIELDS: Record<string, readonly string[]> = {
  performance: ['frequency','coreVoltage','jobInterval','vrFrequency'],
  pool: ['poolMode','poolBalance','stratumKeep','stratumDifficulty',
    'stratumURL','stratumPort','stratumUser','stratumPassword','stratumProtocol','stratumTLS','stratumEnonceSubscribe','sv2AuthorityPubkey','sv2ChannelType','coinbaseVerifyMode','coinbaseMaxFee','coinbaseVerifyForce',
    'fallbackStratumURL','fallbackStratumPort','fallbackStratumUser','fallbackStratumPassword','fallbackStratumProtocol','fallbackStratumTLS','fallbackStratumEnonceSubscribe','fallbackSv2AuthorityPubkey','fallbackSv2ChannelType','fallbackCoinbaseVerifyMode','fallbackCoinbaseMaxFee','fallbackCoinbaseVerifyForce'],
  cooling: ['autofanspeed','manualFanSpeed','overheat_temp','pidTargetTemp','pidP','pidI','pidD','invertFanPolarity','pidUseMax','fan1Mode','fan1ManualSpeed','fan1OverheatTemp','fan1PidTargetTemp','fan1PidP','fan1PidI','fan1PidD'],
  network: ['hostname','ssid','wifiPass'],
  display: ['flipScreen','invertScreen','autoScreenOff','timeFormat'],
  advanced: ['canMaster','customMempoolEnabled','mempoolUrl'],
};
export function sectionFields(section: string, fanCount = 2): readonly string[] {
  return (SECTION_FIELDS[section] || []).filter(key => !(section === 'cooling' && fanCount < 2 && key.startsWith('fan1')));
}
export function changedFields(values: Record<string, any>, baseline: Record<string, any>, section: string, fanCount = 2): string[] {
  return sectionFields(section,fanCount).filter(key => values[key] !== baseline[key]);
}
export function settingsPatch(values: Record<string, any>, baseline: Record<string, any>, section: string, fanCount = 2): Record<string, any> {
  const changed = new Set(changedFields(values,baseline,section,fanCount));
  const out: Record<string, any> = {};
  const copy=(source:string,target=source,bool=false,into=out)=>{if(changed.has(source) && values[source] !== '*****')into[target]=bool ? !!values[source] : values[source];};
  if(section === 'performance')sectionFields(section).forEach(k=>copy(k));
  if(section === 'network')sectionFields(section).forEach(k=>copy(k));
  if(section === 'display') ['flipScreen','invertScreen','autoScreenOff'].forEach(k=>copy(k,k,true));
  if(section === 'advanced') {
    copy('canMaster','canMaster',true); copy('customMempoolEnabled','mempoolCustom',true);
    if(changed.has('customMempoolEnabled') || changed.has('mempoolUrl'))out.mempoolUrl=values.customMempoolEnabled ? values.mempoolUrl : '';
  }
  if(section === 'pool') {
    ['poolMode','poolBalance','stratumDifficulty'].forEach(k=>copy(k));copy('stratumKeep','stratumKeep',true);
    const pools=[{},{}];
    const names=['URL','Port','User','Password','Protocol','TLS','EnonceSubscribe'];
    const targets=['url','port','user','password','protocol','tls','enonceSubscribe'];
    pools.forEach((pool,i)=>{
      names.forEach((name,j)=>copy((i ? 'fallbackStratum' : 'stratum')+name,targets[j],j>=5,pool));
      ['sv2AuthorityPubkey','sv2ChannelType','coinbaseVerifyMode','coinbaseMaxFee','coinbaseVerifyForce'].forEach(k=>copy(i ? 'fallback'+k[0].toUpperCase()+k.slice(1) : k,k,k==='coinbaseVerifyForce',pool));
    });
    if(pools.some(p=>Object.keys(p).length))out.pools=pools;
  }
  if(section === 'cooling') {
    copy('invertFanPolarity','invertFanPolarity',true);if(fanCount===1)copy('pidUseMax','pidUseMax',true);
    const fans=Array.from({length:Math.min(fanCount,2)},()=>({} as Record<string,any>));
    fans.forEach((fan,i)=>{
      const prefix=i ? 'fan1' : '';
      copy(i?'fan1Mode':'autofanspeed','mode',false,fan);
      copy(i?'fan1ManualSpeed':'manualFanSpeed','manualSpeed',false,fan);
      copy(i?'fan1OverheatTemp':'overheat_temp','overheatTemp',false,fan);
      const pid={};['TargetTemp','P','I','D'].forEach(k=>copy(prefix+'Pid'+k,k==='TargetTemp'?'targetTemp':k.toLowerCase(),false,pid));
      // Channel zero uses the historical lower-case pid* form controls.
      if(i===0)['TargetTemp','P','I','D'].forEach(k=>copy('pid'+k,k==='TargetTemp'?'targetTemp':k.toLowerCase(),false,pid));
      if(Object.keys(pid).length)fan.pid=pid;
    });
    if(fans.some(f=>Object.keys(f).length))out.fans=fans;
  }
  return out;
}
