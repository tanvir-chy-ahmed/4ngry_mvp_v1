#include "DrawingCanvas.h"
#include <Preferences.h>
#include <WiFi.h>
#include "features/weather.h"
#include "features/watch.h"
#include <string.h>
#include <stdlib.h>
#include <Update.h>
#include "features/extra_screens.h"
#include "core/global.h"
#include "update/ota_update.h"
#include "update/ota_ui.h"
#include "version.h"
#include <time.h>
#include <esp_system.h>

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;
extern bool webFeatureScreenActive;
extern bool webPortalSessionActive;
extern bool webSessionDismissed;

namespace DrawingCanvas
{
    namespace
    {
        // Authentication is intentionally disabled for this local-device console.
        static bool requireAuth(WebServer &s)
        {
            (void)s;
            return true;
        }
        static uint8_t bitmap[1024] = {};
        static bool routesAdded = false;
        static bool dirty = true;
        static DisplayMode displayMode = DisplayMode::BRAND;
        static String liveText;
        static String dateCounterType;
        static String dateCounterDate;
        static uint32_t lastDateCounterRenderMs = 0;
        static uint32_t lastVideoFrameMs = 0;
        static int drawPrevX = -1, drawPrevY = -1;
        static bool weatherRefreshPending = false;
        static bool manualOtaFinished = false;
        static bool manualOtaFailed = false;
        static bool manualOtaStarted = false;
        static uint8_t manualOtaHeader[24] = {};
        static size_t manualOtaHeaderBytes = 0;

        inline int pixelIndex(int x, int y) { return y * 128 + x; }
        void setPixel(int x, int y, bool on)
        {
            if (x < 0 || x >= 128 || y < 0 || y >= 64)
                return;
            int i = pixelIndex(x, y);
            uint8_t mask = uint8_t(1U << (i & 7));
            if (on)
                bitmap[i >> 3] |= mask;
            else
                bitmap[i >> 3] &= uint8_t(~mask);
        }
        void drawLine(int x0, int y0, int x1, int y1)
        {
            int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
            int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
            for (;;)
            {
                setPixel(x0, y0, true);
                if (x0 == x1 && y0 == y1)
                    break;
                int e2 = 2 * err;
                if (e2 >= dy)
                {
                    err += dy;
                    x0 += sx;
                }
                if (e2 <= dx)
                {
                    err += dx;
                    y0 += sy;
                }
            }
        }
        String page()
        {
            return R"HTML(<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no"><title>4ngryROBO Control</title>
<style>
:root{color-scheme:dark;--orange:#ff7900;--bg:#08090b;--panel:#15171b;--line:#343840;--muted:#a7abb4}*{box-sizing:border-box}body{margin:0;background:var(--bg);color:#f7f7f8;font:15px/1.45 system-ui,Arial,sans-serif}header{padding:22px 16px 16px;border-bottom:1px solid var(--line);background:linear-gradient(120deg,#18130f,#090a0c)}header b{color:var(--orange);font-size:24px;letter-spacing:1px}header p{margin:4px 0 0;color:var(--muted)}main{max-width:780px;margin:auto;padding:14px;display:grid;gap:14px}.panel{background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:15px;min-width:0}.panel h2{font-size:16px;margin:0 0 10px}.accent{color:var(--orange)}p,small{color:var(--muted)}.canvas-wrap{border:1px solid var(--orange);border-radius:8px;overflow:hidden;background:#000;touch-action:none}canvas{display:block;width:100%;height:auto;aspect-ratio:128/56;image-rendering:pixelated;touch-action:none}.row{display:flex;gap:8px;flex-wrap:wrap;margin-top:10px}.row>*{flex:1;min-width:120px}button,.btn{border:0;border-radius:8px;padding:11px 13px;background:var(--orange);color:#111;font-weight:750;cursor:pointer;text-align:center;text-decoration:none}button.secondary,.btn.secondary{background:#282b31;color:#f6f6f6;border:1px solid #454952}button.danger{background:#6b211a;color:white}input,textarea,select{width:100%;background:#0d0f12;color:#f7f7f8;border:1px solid #454952;border-radius:8px;padding:11px;font:inherit;margin:5px 0 10px}textarea{min-height:90px;resize:vertical}label{display:block;color:#e7e7e9;font-size:13px;margin-top:6px}.status{color:var(--orange);min-height:20px;font-size:13px;overflow-wrap:anywhere}.hint{font-size:12px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:10px}.hide{display:none}video{width:100%;max-height:240px;background:#000;border-radius:8px}hr{border:0;border-top:1px solid var(--line);margin:14px 0}
</style></head><body><header><b>4NGRY<span style="color:#fff">ROBO</span></b><p>Local device console · monochrome OLED · <span class="accent">offline-first</span></p></header><main>
<section class="panel"><h2><span class="accent">01 /</span> Live canvas</h2><p>Draw on the 128 × 56 usable OLED area. Points are batched to reduce Wi-Fi request latency.</p><div class="canvas-wrap"><canvas id="c" width="128" height="56"></canvas></div><div class="row"><button id="clear">Clear Display</button><a class="btn secondary" href="/">Wi-Fi setup</a></div><div id="status" class="status">Connecting…</div></section>
<section class="panel"><h2><span class="accent">02 /</span> Live text</h2><label for="txt">Display text</label><textarea id="txt" maxlength="240" placeholder="Type here. Enter creates a new line; Shift+Enter also creates a new line."></textarea><div class="row"><button id="sendText">Show text</button><button id="clearText" class="secondary">Clear text</button></div><small class="hint">Text updates while typing. The OLED can display up to five short lines; longer text is clipped safely.</small></section>
<section class="panel"><h2><span class="accent">03 /</span> Image & low-load video</h2><label for="imageFile">Upload image (converted in browser to 128 × 56 monochrome)</label><input id="imageFile" type="file" accept="image/*"><div class="row"><button id="sendImage">Show image on OLED</button></div><hr><label for="videoFile">Select a video (processed locally in your browser)</label><input id="videoFile" type="file" accept="video/*"><video id="video" controls playsinline class="hide"></video><div class="row"><button id="startVideo">Start low-load playback</button><button id="stopVideo" class="secondary">Stop video</button></div><small class="hint">Video is not uploaded or stored on the ESP32. Your browser converts frames to monochrome and sends at most 2 frames/second to reduce CPU, memory, and Wi-Fi load.</small></section>
<section class="panel"><h2><span class="accent">04 /</span> Clock & weather settings</h2><div class="grid"><div><label for="country">Country</label><select id="country"><option value="BD">Bangladesh</option><option value="IN">India</option><option value="GB">United Kingdom</option><option value="US">United States</option><option value="FR">France</option><option value="DE">Germany</option><option value="JP">Japan</option><option value="AU">Australia</option><option value="CA">Canada</option><option value="SG">Singapore</option><option value="AE">United Arab Emirates</option><option value="SA">Saudi Arabia</option><option value="IT">Italy</option><option value="ES">Spain</option><option value="NL">Netherlands</option></select></div><div><label for="district">District / city</label><select id="district"><option>Sylhet</option><option>Bagerhat</option><option>Bandarban</option><option>Barguna</option><option>Barishal</option><option>Bhola</option><option>Bogura</option><option>Brahmanbaria</option><option>Chandpur</option><option>Chapainawabganj</option><option>Chattogram</option><option>Chuadanga</option><option>Cox's Bazar</option><option>Cumilla</option><option>Dhaka</option><option>Dinajpur</option><option>Faridpur</option><option>Feni</option><option>Gaibandha</option><option>Gazipur</option><option>Gopalganj</option><option>Habiganj</option><option>Jamalpur</option><option>Jashore</option><option>Jhalokati</option><option>Jhenaidah</option><option>Joypurhat</option><option>Khagrachhari</option><option>Khulna</option><option>Kishoreganj</option><option>Kurigram</option><option>Kushtia</option><option>Lakshmipur</option><option>Lalmonirhat</option><option>Madaripur</option><option>Magura</option><option>Manikganj</option><option>Meherpur</option><option>Moulvibazar</option><option>Munshiganj</option><option>Mymensingh</option><option>Naogaon</option><option>Narail</option><option>Narayanganj</option><option>Narsingdi</option><option>Natore</option><option>Netrokona</option><option>Nilphamari</option><option>Noakhali</option><option>Pabna</option><option>Panchagarh</option><option>Patuakhali</option><option>Pirojpur</option><option>Rajbari</option><option>Rajshahi</option><option>Rangamati</option><option>Rangpur</option><option>Satkhira</option><option>Shariatpur</option><option>Sherpur</option><option>Sirajganj</option><option>Sunamganj</option><option>Tangail</option><option>Thakurgaon</option><option>Other city…</option></select><input id="city" placeholder="Enter city/district" class="hide" maxlength="48"></div><div><label for="gmt">GMT / UTC offset</label><select id="gmt"><option value="-12">UTC−12:00</option><option value="-11">UTC−11:00</option><option value="-10">UTC−10:00</option><option value="-9.5">UTC−09:30</option><option value="-9">UTC−09:00</option><option value="-8">UTC−08:00</option><option value="-7">UTC−07:00</option><option value="-6">UTC−06:00</option><option value="-5">UTC−05:00</option><option value="-4">UTC−04:00</option><option value="-3.5">UTC−03:30</option><option value="-3">UTC−03:00</option><option value="-2">UTC−02:00</option><option value="-1">UTC−01:00</option><option value="0">UTC±00:00</option><option value="1">UTC+01:00</option><option value="2">UTC+02:00</option><option value="3">UTC+03:00</option><option value="4">UTC+04:00</option><option value="5">UTC+05:00</option><option value="5.5">UTC+05:30</option><option value="5.75">UTC+05:45</option><option value="6" selected>UTC+06:00</option><option value="7">UTC+07:00</option><option value="8">UTC+08:00</option><option value="9">UTC+09:00</option><option value="9.5">UTC+09:30</option><option value="10">UTC+10:00</option><option value="10.5">UTC+10:30</option><option value="11">UTC+11:00</option><option value="12">UTC+12:00</option><option value="12.75">UTC+12:45</option><option value="13">UTC+13:00</option><option value="14">UTC+14:00</option></select></div><div><label for="format">Time display</label><select id="format"><option value="24">24-hour</option><option value="12">12-hour</option></select></div></div><label for="apikey">OpenWeather API key</label><input id="apikey" type="password" maxlength="80" placeholder="Paste your API key"><div class="row"><button id="saveSettings">Save settings</button><button id="reloadWeather" class="secondary">Refresh weather</button></div><small class="hint">Weather requires a valid OpenWeather API key and internet access through the connected Wi-Fi. Settings are stored in device preferences.</small></section>
<section class="panel"><h2><span class="accent">05 /</span> Device</h2><div class="row"><button id="restart" class="danger">Restart 4ngryROBO</button><a class="btn secondary" href="/">Setup portal</a></div><small class="hint">Restart disconnects this page temporarily while the ESP32 reboots.</small></section>
<section class="panel"><h2><span class="accent">06 /</span> Wi-Fi, sound & OLED</h2>
<label for="wifiSsid">Wi-Fi SSID</label><input id="wifiSsid" name="ssid" maxlength="32" placeholder="Select or enter Wi-Fi name">
<label for="wifiPass">Wi-Fi password</label><input id="wifiPass" name="password" type="password" maxlength="64" placeholder="Wi-Fi password">
<div class="row"><button id="scanWifi" class="secondary">Scan nearby Wi-Fi</button><button id="saveWifi">Save Wi-Fi</button></div><div id="wifiResults" class="status"></div>
<label for="brightness">OLED brightness <span id="brightValue">180</span>/255</label><input id="brightness" type="range" min="0" max="255" value="180">
<label><input id="buzzerEnabled" type="checkbox" checked style="width:auto;margin-right:8px">Enable buzzer (saved on device)</label><label for="buzzerVolume">Buzzer volume <span id="volumeValue">60</span>%</label><input id="buzzerVolume" type="range" min="0" max="100" value="60">
<div class="row"><button data-sound="0" class="soundDemo">Demo 1 · Chime</button><button data-sound="1" class="soundDemo secondary">Demo 2 · Beep</button><button data-sound="2" class="soundDemo secondary">Demo 3 · Fanfare</button></div>
</section>
<section class="panel"><h2><span class="accent">07 /</span> Anniversary & birthday</h2>
<label for="anniversary">Anniversary date</label><input id="anniversary" type="date">
<label for="birthday">Birthday date</label><input id="birthday" type="date">
<div class="row"><button id="saveDates">Save dates</button><button id="showAnniversary" class="secondary">Show anniversary</button><button id="showBirthday" class="secondary">Show birthday</button></div><div id="dateStatus" class="status"></div>
<p class="hint">The browser updates the counters live. Use Show birthday or Show anniversary to open a compact, live-updating counter on the OLED; tap once on the device to return.</p><div id="anniversaryCount" class="hint"></div><div id="birthdayCount" class="hint"></div>
</section>
<section class="panel"><h2><span class="accent">08 /</span> Firmware update</h2>
<p>Check the configured release source or upload a firmware app .bin file built for this exact board and partition layout.</p>
<div class="row"><button id="checkUpdate">Check GitHub update</button><a class="btn secondary" href="/update" target="_blank" rel="noopener">Open ElegantOTA</a><button id="restart2" class="secondary">Restart device</button></div>
<p class="hint">ElegantOTA opens a separate upload page at /update. The GitHub button above still uses the existing GitHub OTA implementation.</p>
<label for="firmwareBin">Validated manual firmware .bin</label><input id="firmwareBin" type="file" accept=".bin,application/octet-stream"><button id="uploadBin" class="danger">Upload & install firmware</button>
<p class="hint">Do not power off during flashing. An incompatible or incomplete binary can make the device unbootable.</p>
</section>
<footer style="text-align:center;color:#777;padding:12px 0 24px">© 4ngryROBO. All rights reserved.<br><b style="color:#ff7900">4ngry developed by Tanvir.</b></footer>
</main>
<script>
const $=id=>document.getElementById(id),c=$('c'),ctx=c.getContext('2d',{willReadFrequently:true}),status=$('status');ctx.fillStyle='#000';ctx.fillRect(0,0,c.width,c.height);ctx.strokeStyle='#fff';ctx.lineWidth=2;ctx.lineCap='round';ctx.lineJoin='round';let drawing=false,last=null,points=[],flushTimer=0,flushBusy=false,textTimer=0,videoTimer=0,videoSending=false;function msg(s){status.textContent=s}function pos(e){let r=c.getBoundingClientRect();return{x:Math.max(0,Math.min(127,Math.floor((e.clientX-r.left)*128/r.width))),y:Math.max(0,Math.min(55,Math.floor((e.clientY-r.top)*56/r.height)))}}function queuePoint(p){points.push(p);if(!flushTimer)flushTimer=setTimeout(flushPoints,28)}async function flushPoints(){flushTimer=0;if(flushBusy||!points.length)return;flushBusy=true;let batch=points.splice(0,80).map(p=>p.x+','+p.y+','+(p.down===0?0:1)).join(';');try{let r=await fetch('/api/drawbatch',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'points='+encodeURIComponent(batch),cache:'no-store'});if(!r.ok)throw Error();msg('Canvas live · '+points.length+' queued')}catch(e){msg('Drawing connection failed; reconnect to 4ngryROBO Wi-Fi')}finally{flushBusy=false;if(points.length&&!flushTimer)flushTimer=setTimeout(flushPoints,8)}}c.addEventListener('pointerdown',e=>{e.preventDefault();drawing=true;last=pos(e);c.setPointerCapture(e.pointerId);ctx.beginPath();ctx.moveTo(last.x,last.y);ctx.lineTo(last.x,last.y);ctx.stroke();queuePoint({...last,down:1})});c.addEventListener('pointermove',e=>{if(!drawing)return;e.preventDefault();let p=pos(e);ctx.beginPath();ctx.moveTo(last.x,last.y);ctx.lineTo(p.x,p.y);ctx.stroke();queuePoint({...p,down:1});last=p});function stopDraw(){if(!drawing)return;drawing=false;if(last)queuePoint({...last,down:0});last=null}c.addEventListener('pointerup',stopDraw);c.addEventListener('pointercancel',stopDraw);$('clear').onclick=async()=>{ctx.fillStyle='#000';ctx.fillRect(0,0,128,56);try{while(flushBusy)await new Promise(r=>setTimeout(r,8));points=[];await fetch('/api/clear',{method:'POST'});msg('Canvas cleared')}catch(e){msg('Clear failed')}};
async function sendText(){clearTimeout(textTimer);try{let r=await fetch('/api/text',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({text:$('txt').value})});if(!r.ok)throw Error();msg('Live text updated')}catch(e){msg('Text update failed')}}$('txt').addEventListener('input',()=>{clearTimeout(textTimer);textTimer=setTimeout(sendText,90)});$('sendText').onclick=sendText;$('clearText').onclick=()=>{$('txt').value='';sendText()};
function packFrame(sourceCanvas){let g=sourceCanvas.getContext('2d',{willReadFrequently:true}),d=g.getImageData(0,0,128,56).data,b=new Uint8Array(1024);for(let y=0;y<56;y++)for(let x=0;x<128;x++){let i=(y*128+x)*4,lum=(d[i]*0.299+d[i+1]*0.587+d[i+2]*0.114);if(lum>128){let k=y*128+x;b[k>>3]|=1<<(k&7)}}return b}async function sendFrame(bytes,endpoint='/api/image'){let binary='';for(let i=0;i<bytes.length;i++)binary+=String.fromCharCode(bytes[i]);let data=btoa(binary);let r=await fetch(endpoint,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({data})});if(!r.ok)throw Error('frame rejected')}$('sendImage').onclick=async()=>{let f=$('imageFile').files[0];if(!f)return msg('Choose an image first');try{let bmp=await createImageBitmap(f),t=document.createElement('canvas');t.width=128;t.height=56;let g=t.getContext('2d');g.fillStyle='#000';g.fillRect(0,0,128,56);let scale=Math.min(128/bmp.width,56/bmp.height),w=bmp.width*scale,h=bmp.height*scale;g.drawImage(bmp,(128-w)/2,(56-h)/2,w,h);await sendFrame(packFrame(t));msg('Image displayed on OLED');bmp.close()}catch(e){msg('Image upload failed')}};
$('videoFile').onchange=()=>{let f=$('videoFile').files[0];if(!f)return;let v=$('video');v.src=URL.createObjectURL(f);v.classList.remove('hide');v.load()};$('startVideo').onclick=()=>{let v=$('video');if(!v.src)return msg('Choose a video first');v.play().catch(()=>msg('Press play on the video preview first'));if(videoTimer)clearInterval(videoTimer);videoTimer=setInterval(async()=>{if(v.paused||v.ended||videoSending)return;videoSending=true;let t=document.createElement('canvas');t.width=128;t.height=56;let g=t.getContext('2d',{willReadFrequently:true});g.fillStyle='#000';g.fillRect(0,0,128,56);let scale=Math.min(128/v.videoWidth,56/v.videoHeight);if(!isFinite(scale)||scale<=0){videoSending=false;return}let w=v.videoWidth*scale,h=v.videoHeight*scale;g.drawImage(v,(128-w)/2,(56-h)/2,w,h);try{await sendFrame(packFrame(t),'/api/frame')}catch(e){msg('Video frame dropped; playback will continue locally')}finally{videoSending=false}},500);msg('Low-load video streaming: 2 FPS')};function stopVideo(){if(videoTimer)clearInterval(videoTimer);videoTimer=0;$('video').pause();msg('Video streaming stopped')} $('stopVideo').onclick=stopVideo;
$('country').onchange=()=>{let bd=$('country').value==='BD';$('district').classList.toggle('hide',!bd);$('city').classList.toggle('hide',bd&&$('district').value!=='Other city…');if(!bd)$('city').placeholder='Enter city for selected country'};$('district').onchange=()=>{$('city').classList.toggle('hide',$('district').value!=='Other city…')};$('saveSettings').onclick=async()=>{let city=$('country').value==='BD'&&$('district').value!=='Other city…' ? $('district').value : $('city').value.trim();if(!city)return msg('Enter or select a city/district');let data={country:$('country').value,city,gmt:$('gmt').value,format:$('format').value,apikey:$('apikey').value.trim()};try{let r=await fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)});if(!r.ok)throw Error();msg('Settings saved. Clock and weather settings will apply on refresh/re-sync.')}catch(e){msg('Could not save settings')}};$('reloadWeather').onclick=async()=>{try{let r=await fetch('/api/weather-refresh',{method:'POST'});msg(r.ok?'Weather refresh requested':'Weather refresh failed')}catch(e){msg('Weather refresh failed')}};$('restart').onclick=async()=>{if(!confirm('Restart 4ngryROBO now?'))return;try{await fetch('/api/restart',{method:'POST'});msg('Restart requested; reconnect after reboot')}catch(e){msg('Restarting…')}};function loadConsoleSettings(){fetch('/api/settings').then(r=>r.text()).then(s=>{let a=s.split('|');if(a.length<4)return;$('country').value=a[0];$('gmt').value=a[2];$('format').value=a[3];if(a[0]==='BD'){if([...$('district').options].some(o=>o.value===a[1]||o.text===a[1]))$('district').value=a[1];else{$('district').value='Other city…';$('city').value=a[1];$('city').classList.remove('hide')}}else{$('city').value=a[1];$('city').classList.remove('hide');$('district').classList.add('hide')}}).catch(()=>{})}function loadDeviceSettings(){fetch('/api/device').then(r=>r.json()).then(v=>{$('brightness').value=v.brightness;$('brightValue').textContent=v.brightness;$('buzzerEnabled').checked=v.buzzer;$('buzzerVolume').value=v.volume;$('volumeValue').textContent=v.volume;$('anniversary').value=v.anniversary||'';$('birthday').value=v.birthday||'';renderDateCounts()}).catch(()=>{})}
let deviceTimer=0;async function devicePost(path,body){let r=await fetch(path,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(body||{})});if(!r.ok)throw Error(await r.text());return r}
$('brightness').oninput=()=>{$('brightValue').textContent=$('brightness').value;clearTimeout(deviceTimer);deviceTimer=setTimeout(()=>devicePost('/api/device',{brightness:$('brightness').value,buzzer:$('buzzerEnabled').checked?'1':'0'}).catch(()=>msg('Device settings failed')),70)};
$('buzzerEnabled').onchange=()=>devicePost('/api/device',{brightness:$('brightness').value,buzzer:$('buzzerEnabled').checked?'1':'0',volume:$('buzzerVolume').value}).then(()=>msg('Buzzer setting saved')).catch(()=>msg('Could not save buzzer setting'));
$('buzzerVolume').oninput=()=>{$('volumeValue').textContent=$('buzzerVolume').value;clearTimeout(deviceTimer);deviceTimer=setTimeout(()=>devicePost('/api/device',{brightness:$('brightness').value,buzzer:$('buzzerEnabled').checked?'1':'0',volume:$('buzzerVolume').value}).catch(()=>msg('Volume update failed')),80)};
document.querySelectorAll('.soundDemo').forEach(b=>b.onclick=async()=>{try{await fetch('/api/buzzer-demo?pattern='+b.dataset.sound);msg('Playing buzzer demo')}catch(e){msg('Buzzer demo failed')}});
$('scanWifi').onclick=async()=>{const out=$('wifiResults');out.textContent='Scanning nearby Wi-Fi…';try{let r=await fetch('/api/scan');if(!r.ok)throw Error();let rows=(await r.text()).split('\n').filter(Boolean);out.innerHTML='';if(!rows.length){out.textContent='No networks found';return}rows.forEach(line=>{let parts=line.split('\t'),name=parts[0];let b=document.createElement('button');b.className='secondary';b.style.margin='3px';b.textContent=name+' ('+(parts[1]||'?')+' dBm)';b.onclick=()=>{$('wifiSsid').value=name;msg('Selected SSID: '+name)};out.appendChild(b)})}catch(e){out.textContent='Scan failed. Wi-Fi scanning may be unavailable while the AP is active.'}};
$('saveWifi').onclick=async()=>{if(!$('wifiSsid').value.trim())return msg('Enter/select an SSID');try{let r=await fetch('/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({ssid:$('wifiSsid').value,password:$('wifiPass').value})});if(!r.ok)throw Error();msg('Wi-Fi credentials saved; device will reconnect')}catch(e){msg('Could not save Wi-Fi credentials')}};
$('saveDates').onclick=async()=>{try{await devicePost('/api/dates',{anniversary:$('anniversary').value,birthday:$('birthday').value});$('dateStatus').textContent='Dates saved to device preferences';renderDateCounts()}catch(e){$('dateStatus').textContent='Could not save dates'}};
function countTo(id,date){if(!date)return 'Not set';let t=new Date(date+'T00:00:00'),now=new Date();if(isNaN(t))return 'Invalid date';if(id==='Birthday'){t.setFullYear(now.getFullYear());if(t<=now)t.setFullYear(now.getFullYear()+1);let seconds=Math.max(0,Math.floor((t-now)/1000)),days=Math.floor(seconds/86400),years=t.getFullYear()-now.getFullYear(),hours=Math.floor((seconds%86400)/3600),secs=seconds%60;return `Birthday in: ${years} years · ${days} days · ${hours} hours · ${secs} seconds`}let seconds=Math.max(0,Math.floor((now-t)/1000)),days=Math.floor(seconds/86400),years=Math.floor(days/365.2425),hours=Math.floor((seconds%86400)/3600),secs=seconds%60;return `${id}: ${years} years · ${days} days · ${hours} hours · ${secs} seconds`}
function renderDateCounts(){$('anniversaryCount').textContent=countTo('Anniversary',$('anniversary').value);$('birthdayCount').textContent=countTo('Birthday',$('birthday').value)}setInterval(renderDateCounts,1000);$('showAnniversary').onclick=()=>fetch('/api/show-date?type=anniversary').then(r=>msg(r.ok?'Anniversary message sent to OLED':'Could not show anniversary')).catch(()=>msg('Request failed'));$('showBirthday').onclick=()=>fetch('/api/show-date?type=birthday').then(r=>msg(r.ok?'Birthday message sent to OLED':'Could not show birthday')).catch(()=>msg('Request failed'));
$('checkUpdate').onclick=async()=>{if(!confirm('Check for and automatically install a newer firmware if available?'))return;msg('Checking update…');try{let r=await fetch('/api/check-update',{method:'POST'});msg(r.ok?'Update check started. Follow OLED progress; do not power off.':'Update check failed')}catch(e){msg('Update may be running; watch OLED and serial log')}};
$('uploadBin').onclick=async()=>{let f=$('firmwareBin').files[0];if(!f)return msg('Select a .bin file first');if(!f.name.toLowerCase().endsWith('.bin'))return msg('Only .bin files are accepted');if(!confirm('Install '+f.name+'? Ensure it was built for this exact ESP32 model and uses an OTA-capable partition table.'))return;let data=new FormData();data.append('firmware',f);msg('Uploading firmware… do not close this page');try{let r=await fetch('/api/manual-ota',{method:'POST',body:data});if(!r.ok)throw Error(await r.text());msg('Firmware accepted; device will restart')}catch(e){msg('Upload failed: '+e.message)}};
$('restart2').onclick=async()=>{if(confirm('Restart device now?')){try{await fetch('/api/restart',{method:'POST'});msg('Restarting…')}catch(e){msg('Restarting…')}}};
loadDeviceSettings();loadConsoleSettings();fetch('/api/ping').then(r=>{if(r.ok)msg('4ngryROBO ready')}).catch(()=>msg('Device not responding'));

</script></body></html>)HTML";
        }
    }

    bool pixelAt(int x, int y)
    {
        if (x < 0 || x >= 128 || y < 0 || y >= 64)
            return false;
        int i = pixelIndex(x, y);
        return (bitmap[i >> 3] & uint8_t(1U << (i & 7))) != 0;
    }
    void clear()
    {
        memset(bitmap, 0, sizeof(bitmap));
        drawPrevX = -1;
        drawPrevY = -1;
        dirty = true;
    }
    void setBrandMode()
    {
        displayMode = DisplayMode::BRAND;
        drawPrevX = -1;
        drawPrevY = -1;
        dirty = true;
    }
    void setCanvasMode()
    {
        displayMode = DisplayMode::CANVAS;
        dirty = true;
    }
    void setLiveText(const String &text)
    {
        liveText = text;
        if (liveText.length() > 240)
            liveText.remove(240);
        displayMode = DisplayMode::TEXT;
        drawPrevX = -1;
        drawPrevY = -1;
        dirty = true;
    }
    void setDateCounter(const String &type, const String &date)
    {
        dateCounterType = (type == "birthday") ? "birthday" : "anniversary";
        dateCounterDate = date;
        displayMode = DisplayMode::DATE_COUNTER;
        drawPrevX = -1;
        drawPrevY = -1;
        dirty = true;
        lastDateCounterRenderMs = 0;
    }

    DisplayMode getDisplayMode() { return displayMode; }
    String getLiveText() { return liveText; }
    bool needsRender()
    {
        if (displayMode == DisplayMode::DATE_COUNTER && millis() - lastDateCounterRenderMs >= 1000UL)
            dirty = true;
        return dirty;
    }
    void markRendered()
    {
        dirty = false;
        if (displayMode == DisplayMode::DATE_COUNTER)
            lastDateCounterRenderMs = millis();
    }

    static bool parseStoredDate(const String &date, int &year, int &month, int &day)
    {
        if (date.length() != 10 || date[4] != '-' || date[7] != '-') return false;
        if (sscanf(date.c_str(), "%4d-%2d-%2d", &year, &month, &day) != 3) return false;
        if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31) return false;
        struct tm check = {};
        check.tm_year = year - 1900;
        check.tm_mon = month - 1;
        check.tm_mday = day;
        check.tm_isdst = -1;
        time_t epoch = mktime(&check);
        if (epoch == (time_t)-1) return false;
        return check.tm_year == year - 1900 && check.tm_mon == month - 1 && check.tm_mday == day;
    }

    void renderToOLED(U8G2 &display)
    {
        display.clearBuffer();
        display.setDrawColor(1);

        if (displayMode == DisplayMode::DATE_COUNTER)
        {
            const bool birthday = dateCounterType == "birthday";
            display.setFont(u8g2_font_6x10_tf);
            const char *title = birthday ? "BIRTHDAY COUNTDOWN" : "ANNIVERSARY TIMER";
            display.drawStr(max(0, (128 - display.getStrWidth(title)) / 2), 9, title);
            display.drawHLine(4, 13, 120);

            int eventYear = 0, eventMonth = 0, eventDay = 0;
            if (!parseStoredDate(dateCounterDate, eventYear, eventMonth, eventDay))
            {
                display.setFont(u8g2_font_6x10_tf);
                display.drawStr(15, 31, "Date is invalid");
                display.drawStr(8, 45, "Update it in web console");
            }
            else
            {
                time_t nowEpoch = time(nullptr);
                if (nowEpoch < 1700000000)
                {
                    display.drawStr(11, 31, "Waiting for clock sync");
                    display.drawStr(12, 45, "Connect to internet");
                }
                else
                {
                    struct tm nowTm = {};
                    localtime_r(&nowEpoch, &nowTm);
                    char primary[24] = {};
                    char secondary[24] = {};

                    if (birthday)
                    {
                        struct tm next = {};
                        next.tm_year = nowTm.tm_year;
                        next.tm_mon = eventMonth - 1;
                        next.tm_mday = eventDay;
                        next.tm_isdst = -1;
                        time_t target = mktime(&next);
                        if (target <= nowEpoch)
                        {
                            next.tm_year += 1;
                            next.tm_isdst = -1;
                            target = mktime(&next);
                        }
                        uint32_t remaining = target > nowEpoch ? (uint32_t)(target - nowEpoch) : 0;
                        uint32_t days = remaining / 86400UL;
                        uint32_t rem = remaining % 86400UL;
                        snprintf(primary, sizeof(primary), "%lu DAYS", (unsigned long)days);
                        snprintf(secondary, sizeof(secondary), "%02lu:%02lu:%02lu LEFT",
                                 (unsigned long)(rem / 3600UL),
                                 (unsigned long)((rem / 60UL) % 60UL),
                                 (unsigned long)(rem % 60UL));
                    }
                    else
                    {
                        struct tm start = {};
                        start.tm_year = eventYear - 1900;
                        start.tm_mon = eventMonth - 1;
                        start.tm_mday = eventDay;
                        start.tm_isdst = -1;
                        time_t anniversaryStart = mktime(&start);
                        if (anniversaryStart > nowEpoch)
                        {
                            uint32_t remaining = (uint32_t)(anniversaryStart - nowEpoch);
                            snprintf(primary, sizeof(primary), "%lu DAYS LEFT", (unsigned long)(remaining / 86400UL));
                            uint32_t rem = remaining % 86400UL;
                            snprintf(secondary, sizeof(secondary), "%02lu:%02lu:%02lu",
                                     (unsigned long)(rem / 3600UL),
                                     (unsigned long)((rem / 60UL) % 60UL),
                                     (unsigned long)(rem % 60UL));
                        }
                        else
                        {
                            int years = nowTm.tm_year - (eventYear - 1900);
                            struct tm last = start;
                            last.tm_year += years;
                            last.tm_isdst = -1;
                            time_t lastAnniversary = mktime(&last);
                            if (lastAnniversary > nowEpoch)
                            {
                                --years;
                                last = start;
                                last.tm_year += years;
                                last.tm_isdst = -1;
                                lastAnniversary = mktime(&last);
                            }
                            uint32_t elapsed = (uint32_t)(nowEpoch - lastAnniversary);
                            uint32_t days = elapsed / 86400UL;
                            uint32_t rem = elapsed % 86400UL;
                            snprintf(primary, sizeof(primary), "%dY %luD", years, (unsigned long)days);
                            snprintf(secondary, sizeof(secondary), "%02lu:%02lu:%02lu ELAPSED",
                                     (unsigned long)(rem / 3600UL),
                                     (unsigned long)((rem / 60UL) % 60UL),
                                     (unsigned long)(rem % 60UL));
                        }
                    }

                    display.setFont(u8g2_font_10x20_tf);
                    int pw = display.getStrWidth(primary);
                    display.drawStr(max(0, (128 - pw) / 2), 36, primary);
                    display.setFont(u8g2_font_5x8_tf);
                    int sw = display.getStrWidth(secondary);
                    display.drawStr(max(0, (128 - sw) / 2), 49, secondary);
                    char dateLabel[24];
                    snprintf(dateLabel, sizeof(dateLabel), "%04d-%02d-%02d", eventYear, eventMonth, eventDay);
                    int dw = display.getStrWidth(dateLabel);
                    display.drawStr(max(0, (128 - dw) / 2), 56, dateLabel);
                }
            }
            display.setDrawColor(0);
            display.drawBox(0, 57, 128, 7);
            display.setDrawColor(1);
            display.setFont(u8g2_font_5x8_tf);
            const char *footer = "Tap once to back";
            display.drawStr(max(0, (128 - display.getStrWidth(footer)) / 2), 64, footer);
            display.sendBuffer();
            markRendered();
            return;
        }

        const DisplayMode mode = displayMode;
        if (mode == DisplayMode::CANVAS)
        {
            for (int y = 0; y < 56; ++y)
                for (int x = 0; x < 128; ++x)
                    if (pixelAt(x, y)) display.drawPixel(x, y);
        }
        else if (mode == DisplayMode::TEXT)
        {
            display.setFont(u8g2_font_6x10_tf);
            String source = getLiveText();
            String lines[5];
            int lineCount = 0;
            int start = 0;
            while (start <= source.length() && lineCount < 5)
            {
                int nl = source.indexOf('\n', start);
                String part = nl < 0 ? source.substring(start) : source.substring(start, nl);
                part.replace("\r", "");
                while (part.length() && lineCount < 5)
                {
                    int fit = part.length();
                    while (fit > 0 && display.getStrWidth(part.substring(0, fit).c_str()) > 124) --fit;
                    if (fit == 0) fit = 1;
                    lines[lineCount++] = part.substring(0, fit);
                    part.remove(0, fit);
                    while (part.startsWith(" ")) part.remove(0, 1);
                }
                if (nl < 0) break;
                start = nl + 1;
            }
            for (int i = 0; i < lineCount; ++i)
            {
                int w = display.getStrWidth(lines[i].c_str());
                display.drawStr(max(0, (128 - w) / 2), 10 + i * 9, lines[i].c_str());
            }
        }
        else
        {
            display.setFont(u8g2_font_10x20_tf);
            const char *brand = "4ngry";
            display.drawStr((128 - display.getStrWidth(brand)) / 2, 35, brand);
        }
        display.setDrawColor(0);
        display.drawBox(0, 56, 128, 8);
        display.setDrawColor(1);
        display.setFont(u8g2_font_5x8_tf);
        const char *footer = "Tap once to back";
        display.drawStr(max(0, (128 - display.getStrWidth(footer)) / 2), 64, footer);
        display.sendBuffer();
        markRendered();
    }
    void servicePendingTasks()
    {
        if (!weatherRefreshPending || WiFi.status() != WL_CONNECTED)
            return;
        weatherRefreshPending = false;
        getWeatherInfo();
    }

    bool authorizeRequest(WebServer &server) { return requireAuth(server); }

    void registerRoutes(WebServer &server)
    {
        if (routesAdded)
            return;
        routesAdded = true;
        server.on("/canvas", HTTP_GET, [&server]()
                  { server.send(200, "text/html; charset=utf-8", page()); });
        server.on("/api/ping", HTTP_GET, [&server]()
                  {if(!requireAuth(server))return;server.send(200,"text/plain","OK"); });
        server.on("/api/clear", HTTP_POST, [&server]()
                  {if(!requireAuth(server))return;clear();setCanvasMode();server.send(200,"text/plain","CLEARED"); });
        server.on("/api/text", HTTP_POST, [&server]()
                  {if(!requireAuth(server))return;if(!server.hasArg("text")){server.send(400,"text/plain","Missing text");return;}setLiveText(server.arg("text"));server.send(200,"text/plain","TEXT UPDATED"); });
        server.on("/api/drawbatch", HTTP_POST, [&server]()
                  {
        if(!requireAuth(server))return;
        if(!server.hasArg("points")){server.send(400,"text/plain","Missing points");return;}
        String data=server.arg("points"); int start=0; bool any=false;
        while(start<data.length()) {int end=data.indexOf(';',start);if(end<0)end=data.length();String p=data.substring(start,end);int comma=p.indexOf(',');if(comma>0){int comma2=p.indexOf(',',comma+1);int x=p.substring(0,comma).toInt(),y=(comma2<0?p.substring(comma+1):p.substring(comma+1,comma2)).toInt();int down=(comma2<0?1:p.substring(comma2+1).toInt());if(down==0){drawPrevX=-1;drawPrevY=-1;}else if(x>=0&&x<128&&y>=0&&y<56){if(drawPrevX>=0)drawLine(drawPrevX,drawPrevY,x,y);else setPixel(x,y,true);drawPrevX=x;drawPrevY=y;any=true;}}start=end+1;}
        if(any){setCanvasMode();dirty=true;}server.send(200,"text/plain","OK"); });
        auto imageHandler = [&server](bool isVideo)
        {
            if (!requireAuth(server))
                return;
            if (!server.hasArg("data"))
            {
                server.send(400, "text/plain", "Missing bitmap data");
                return;
            }
            if (isVideo)
            {
                uint32_t now = millis();
                if (now - lastVideoFrameMs < 450)
                {
                    server.send(429, "text/plain", "Frame rate limited");
                    return;
                }
                lastVideoFrameMs = now;
            }
            String encoded = server.arg("data");
            static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            uint32_t accumulator = 0;
            int bits = -8;
            size_t out = 0;
            memset(bitmap, 0, sizeof(bitmap));
            for (size_t i = 0; i < encoded.length(); ++i)
            {
                char c = encoded[i];
                if (c == '=')
                    break;
                const char *q = strchr(alphabet, c);
                if (!q)
                    continue;
                accumulator = (accumulator << 6) | (uint32_t)(q - alphabet);
                bits += 6;
                if (bits >= 0)
                {
                    if (out >= sizeof(bitmap))
                    {
                        server.send(413, "text/plain", "Bitmap too large");
                        return;
                    }
                    bitmap[out++] = (uint8_t)((accumulator >> bits) & 0xFF);
                    bits -= 8;
                }
            }
            if (out != sizeof(bitmap))
            {
                server.send(400, "text/plain", "Invalid bitmap size");
                return;
            }
            drawPrevX = -1;
            drawPrevY = -1;
            setCanvasMode();
            dirty = true;
            server.send(200, "text/plain", "FRAME OK");
        };
        server.on("/api/image", HTTP_POST, [imageHandler]()
                  { imageHandler(false); });
        server.on("/api/frame", HTTP_POST, [imageHandler]()
                  { imageHandler(true); });
        server.on("/api/settings", HTTP_GET, [&server]()
                  {
        if(!requireAuth(server))return;
        Preferences prefs; String country="BD", city="Sylhet"; float gmt=6.0f; bool format24=true;
        if(prefs.begin("4ngryROBO",true)){country=prefs.getString("weather_country","BD");city=prefs.getString("weather_city","Sylhet");gmt=prefs.getFloat("gmt_offset",6.0f);format24=prefs.getBool("clock_24h",true);prefs.end();}
        String response=country+"|"+city+"|"+String(gmt,2)+"|"+(format24?"24":"12");
        server.send(200,"text/plain; charset=utf-8",response); });
        server.on("/api/settings", HTTP_POST, [&server]()
                  {
        if(!requireAuth(server))return;
        String raw=server.arg("plain"); if(raw.length()==0){server.send(400,"text/plain","Missing JSON");return;}
        // Small, bounded JSON parser without adding another library.
        auto value=[&raw](const char* key)->String{String token=String("\"")+key+"\"";int p=raw.indexOf(token);if(p<0)return "";p=raw.indexOf(':',p+token.length());if(p<0)return "";p++;while(p<raw.length()&&raw[p]==' ')p++;if(p>=raw.length()||raw[p]!='\"')return "";int e=raw.indexOf('\"',p+1);if(e<0)return "";return raw.substring(p+1,e);};
        String country=value("country"),city=value("city"),gmt=value("gmt"),format=value("format"),apikey=value("apikey");
        if(city.length()==0||city.length()>48||country.length()>8||apikey.length()>80){server.send(400,"text/plain","Invalid settings");return;}
        Preferences prefs;if(!prefs.begin("4ngryROBO",false)){server.send(500,"text/plain","Preferences unavailable");return;}
        prefs.putString("weather_country",country);prefs.putString("weather_city",city);prefs.putString("weather_key",apikey);prefs.putFloat("gmt_offset",gmt.toFloat());prefs.putBool("clock_24h",format!="12");prefs.end();
        weatherRefreshPending = true;
        applyClockSettings(gmt.toFloat(), format != "12");
        server.send(200,"application/json","{\"ok\":true}"); });
        server.on("/api/weather-refresh", HTTP_POST, [&server]()
                  {if(!requireAuth(server))return;weatherRefreshPending=true;server.send(202,"text/plain","Weather refresh queued; it will run when STA internet is connected."); });
        server.on("/api/device", HTTP_GET, [&server]()
                  {
        if(!requireAuth(server))return;
        Preferences p;uint8_t bright=180,vol=60;bool buz=true;String ann="",birth="";
        if(p.begin("4ngryROBO",true)){bright=p.getUChar("oled_bright",180);buz=p.getBool("buzzer_on",true);vol=p.getUChar("buzzer_vol",60);ann=p.getString("anniversary","");birth=p.getString("birthday","");p.end();}
        String json="{\"brightness\":"+String(bright)+",\"volume\":"+String(vol)+",\"buzzer\":"+(buz?"true":"false")+",\"anniversary\":\""+ann+"\",\"birthday\":\""+birth+"\"}";
        server.send(200,"application/json",json); });
        server.on("/api/device", HTTP_POST, [&server]()
                  {
        if(!requireAuth(server))return;
        int b=constrain(server.arg("brightness").toInt(),0,255);int v=constrain(server.arg("volume").toInt(),0,100);bool enabled=server.arg("buzzer")=="1";
        ExtraScreens::setBuzzerEnabled(enabled);ExtraScreens::setBrightness((uint8_t)b,u8g2);ExtraScreens::setBuzzerVolume((uint8_t)v);
        server.send(200,"text/plain","SAVED"); });
        server.on("/api/buzzer-demo", HTTP_GET, [&server]()
                  {if(!requireAuth(server))return;ExtraScreens::playBuzzerDemo((uint8_t)constrain(server.arg("pattern").toInt(),0,2));server.send(200,"text/plain","OK"); });
        server.on("/api/dates", HTTP_POST, [&server]()
                  {
        if(!requireAuth(server))return;String a=server.arg("anniversary"),b=server.arg("birthday");
        auto validDate=[](const String &v){return v.isEmpty() || (v.length()==10 && v[4]=='-' && v[7]=='-' && v.substring(0,4).toInt()>=1970 && v.substring(5,7).toInt()>=1 && v.substring(5,7).toInt()<=12 && v.substring(8,10).toInt()>=1 && v.substring(8,10).toInt()<=31);};
        if(!validDate(a)||!validDate(b)){server.send(400,"text/plain","Invalid date; use YYYY-MM-DD");return;}
        Preferences p;if(!p.begin("4ngryROBO",false)){server.send(500,"text/plain","Preferences unavailable");return;}
        p.putString("anniversary",a);p.putString("birthday",b);p.end();server.send(200,"text/plain","SAVED"); });
        server.on("/api/show-date", HTTP_GET, [&server]()
                  {
        if(!requireAuth(server))return;String type=server.arg("type");Preferences p;String date="";
        if(p.begin("4ngryROBO",true)){date=p.getString(type=="birthday"?"birthday":"anniversary","");p.end();}
        if(date.isEmpty()){server.send(404,"text/plain","Date not set");return;}
        if(type!="birthday" && type!="anniversary"){server.send(400,"text/plain","Invalid date type");return;}
        setDateCounter(type,date);webFeatureScreenActive=true;webPortalSessionActive=(Global::getWiFiState()==Global::WIFI_SETUP_PORTAL);webSessionDismissed=false;server.send(200,"text/plain","OK"); });
        server.on("/api/scan", HTTP_GET, [&server]()
                  {
        if(!requireAuth(server))return;
        int n=WiFi.scanNetworks(false,true);String out="";
        for(int i=0;i<n;++i){String name=WiFi.SSID(i);name.replace("\t"," ");name.replace("\n"," ");if(name.length())out+=name+"\t"+String(WiFi.RSSI(i))+"\n";}
        WiFi.scanDelete();server.send(200,"text/plain; charset=utf-8",out); });
        server.on("/api/check-update", HTTP_POST, [&server]()
                  {if(!requireAuth(server))return;server.send(202,"text/plain","Update check starting; watch OLED");delay(100);checkfirmwareUpdate(); });
        server.on("/api/manual-ota", HTTP_POST, [&server]()
                  {
        if(!requireAuth(server))return;
        bool ok=manualOtaFinished && !manualOtaFailed && manualOtaStarted && !Update.hasError() && Update.isFinished();
        server.send(ok?200:500,"text/plain",ok?"Firmware validated and staged; restarting":"Firmware rejected or upload failed. Check serial log.");
        if(ok){delay(350);ESP.restart();} }, [&server]()
                  {
        if(!requireAuth(server))return;
        HTTPUpload& upload=server.upload();
        if(upload.status==UPLOAD_FILE_START){
            manualOtaFinished=false; manualOtaFailed=false; manualOtaStarted=false; manualOtaHeaderBytes=0;
            memset(manualOtaHeader,0,sizeof(manualOtaHeader));
            String filename=upload.filename; filename.toLowerCase();
            if(!filename.endsWith(".bin")){manualOtaFailed=true;Serial.println("[OTA] Manual upload rejected: only .bin app images are accepted.");otaUpdateFailedScreen("Select app .bin only");return;}
            Serial.printf("[OTA] Manual upload: %s\n",upload.filename.c_str());
        } else if(upload.status==UPLOAD_FILE_WRITE){
            if(manualOtaFailed)return;
            size_t offset=0;
            if(!manualOtaStarted){
                size_t need=sizeof(manualOtaHeader)-manualOtaHeaderBytes;
                size_t take=upload.currentSize<need?upload.currentSize:need;
                memcpy(manualOtaHeader+manualOtaHeaderBytes,upload.buf,take);
                manualOtaHeaderBytes+=take;
                offset=take;
                if(manualOtaHeaderBytes<sizeof(manualOtaHeader))return;

                String headerError;
                if(!validateFirmwareImageHeader(manualOtaHeader,sizeof(manualOtaHeader),headerError)){
                    manualOtaFailed=true;
                    Serial.printf("[OTA] Manual image rejected before flash write: %s\n",headerError.c_str());
                    otaUpdateFailedScreen("Wrong board .bin");
                    return;
                }
                Serial.println("[OTA] Manual image header matches this chip; starting flash write.");
                if(!Update.begin(UPDATE_SIZE_UNKNOWN)){
                    manualOtaFailed=true;Update.printError(Serial);otaUpdateFailedScreen("Flash begin failed");return;
                }
                if(Update.write(manualOtaHeader,sizeof(manualOtaHeader))!=sizeof(manualOtaHeader)){
                    manualOtaFailed=true;Update.printError(Serial);Update.abort();otaUpdateFailedScreen("Flash write failed");return;
                }
                manualOtaStarted=true;
            }
            if(offset<upload.currentSize){
                size_t remaining=upload.currentSize-offset;
                if(Update.write(upload.buf+offset,remaining)!=remaining){manualOtaFailed=true;Update.printError(Serial);Update.abort();otaUpdateFailedScreen("Flash write failed");return;}
            }
            uint32_t requestLength=(uint32_t)server.header("Content-Length").toInt();
            int progress=requestLength?constrain((int)(upload.totalSize*100UL/requestLength),0,99):0;
            otaUpdateScreen(progress);
        } else if(upload.status==UPLOAD_FILE_END){
            if(manualOtaFailed || !manualOtaStarted || manualOtaHeaderBytes!=sizeof(manualOtaHeader) || upload.totalSize<=sizeof(manualOtaHeader)){
                manualOtaFailed=true;Serial.println("[OTA] Manual upload ended before a valid firmware image was received.");otaUpdateFailedScreen("Incomplete upload");if(manualOtaStarted)Update.abort();return;
            }
            if(Update.end(true) && Update.isFinished()){
                manualOtaFinished=true;otaUpdateScreen(100);Serial.printf("[OTA] Manual image validated and staged: %u bytes\n",upload.totalSize);
            }else{manualOtaFailed=true;Update.printError(Serial);Serial.println("[OTA] Manual image finalization failed; device will not restart.");otaUpdateFailedScreen("Image validation failed");}
        } else if(upload.status==UPLOAD_FILE_ABORTED){manualOtaFailed=true;if(manualOtaStarted)Update.abort();Serial.println("[OTA] Manual upload aborted.");otaUpdateFailedScreen("Upload aborted");} });
        server.on("/api/restart", HTTP_POST, [&server]()
                  {if(!requireAuth(server))return;server.send(200,"text/plain","Restarting");delay(150);ESP.restart(); });
    }
}
