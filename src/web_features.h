// Local web controls for 4ngry. Included after the main firmware globals.
static char webText[97] = "";
static uint8_t webBits[1024];
static const uint16_t melody[][6] = {{1047, 1319, 1568, 0}, {784, 988, 1175, 0}, {523, 659, 784, 1047}, {880, 784, 698, 587}, {659, 880, 1047, 1319}, {523, 523, 784, 0}, {988, 1175, 1319, 1568}, {440, 660, 880, 1320}, {1200, 900, 1200, 900}, {660, 660, 990, 0}, {700, 850, 1000, 1200}, {1500, 1200, 900, 600}};
static const uint8_t melodyLen[] = {3, 3, 4, 4, 4, 3, 4, 4, 4, 3, 4, 4};
static bool melodyPlaying = false;
static uint8_t melodyNote = 0;
static uint32_t melodyAt = 0;
static uint32_t hardwareBeepUntil = 0;
static uint8_t alarmSong = 0;
static uint8_t oledBrightness = 180;
static bool customTextShown = false, customDrawingShown = false, relationshipShown = false;
static uint32_t webConfirmAt = 0, portalStopAt = 0;
static char relationshipDate[11] = "";

static bool validRelationshipDate(const String &value)
{
    if (value.length() != 10 || value[4] != '-' || value[7] != '-')
        return false;
    for (uint8_t i = 0; i < 10; i++)
        if (i != 4 && i != 7 && (value[i] < '0' || value[i] > '9'))
            return false;
    int year = value.substring(0, 4).toInt(), month = value.substring(5, 7).toInt(), day = value.substring(8, 10).toInt();
    if (year < 1970 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31)
        return false;
    struct tm check = {};
    check.tm_year = year - 1900;
    check.tm_mon = month - 1;
    check.tm_mday = day;
    check.tm_hour = 12;
    mktime(&check);
    return check.tm_year == year - 1900 && check.tm_mon == month - 1 && check.tm_mday == day;
}

static int64_t relationshipSeconds()
{
    if (!relationshipDate[0])
        return -1;
    struct tm now = {};
    if (!getLocalTime(&now, 0))
        return -2;
    struct tm began = {};
    began.tm_year = String(relationshipDate).substring(0, 4).toInt() - 1900;
    began.tm_mon = String(relationshipDate).substring(5, 7).toInt() - 1;
    began.tm_mday = String(relationshipDate).substring(8, 10).toInt();
    began.tm_hour = 0;
    time_t start = mktime(&began), current = mktime(&now);
    if (start == (time_t)-1 || current < start)
        return -3;
    return (int64_t)difftime(current, start);
}
static String relationshipElapsed()
{
    int64_t seconds = relationshipSeconds();
    if (seconds == -1)
        return "Set date in web page";
    if (seconds == -2)
        return "Clock not set";
    if (seconds < 0)
        return "Date is in future";
    return String((uint32_t)(seconds / 86400)) + " days";
}
static void clearFeatureDisplay()
{
    customTextShown = false;
    customDrawingShown = false;
    relationshipShown = false;
}

static String featurePage() { return F(R"HTML(<!doctype html><html><head><meta name=viewport content='width=device-width,initial-scale=1'><style>:root{--orange:#ff7a00;--gold:#ffb000;--ink:#090909;--panel:#151515;--line:#343434;--muted:#bababa}*{box-sizing:border-box}body{margin:0;background:radial-gradient(circle at top,#2c1a0a 0,#090909 46%);font:16px system-ui,-apple-system,Arial,sans-serif;color:#f5f5f5}.card{max-width:540px;margin:16px auto;padding:20px;background:var(--panel);border:1px solid var(--line);border-top:3px solid var(--orange);border-radius:14px;box-shadow:0 16px 45px #0009}h1{margin:0;color:var(--orange);letter-spacing:.06em;font-size:26px}h1+p{color:var(--muted)}.menu{display:flex;flex-wrap:wrap;gap:7px;margin:18px 0}.menu a{padding:7px 9px;border:1px solid var(--line);border-radius:6px;background:#202020;color:#eee;text-decoration:none;font-size:12px}.menu a:hover{border-color:var(--orange);color:var(--orange)}h2{font-size:17px;letter-spacing:.04em;text-transform:uppercase;border-bottom:1px solid var(--line);padding-bottom:7px;margin-top:28px;color:var(--gold)}label{display:block;margin-top:12px;font-weight:700;font-size:14px}input,select,textarea,button{width:100%;margin-top:6px;padding:11px;border-radius:7px;font:inherit}input,select,textarea{border:1px solid #444;background:#0c0c0c;color:#fff}input:focus,select:focus,textarea:focus{outline:2px solid var(--orange);border-color:var(--orange)}button{border:1px solid var(--orange);background:var(--orange);color:#111;font-weight:800;cursor:pointer}.secondary{background:#202020;color:#f5f5f5;border-color:#555}.secondary:hover{border-color:var(--orange);color:var(--orange)}.row{display:flex;gap:8px}.row>*{flex:1}.net{width:100%;text-align:left;margin-top:7px;background:#202020;color:#fff;border-color:#555}.hint{font-size:13px;color:var(--muted);line-height:1.45}.status{min-height:22px;color:var(--gold)}canvas{width:100%;height:auto;background:#fff;touch-action:none;image-rendering:pixelated;border:2px solid var(--orange);border-radius:7px}</style></head><body><main class=card><h1>MTR - 4ngry</h1><p>Local controls for your desk companion.</p><nav class=menu><a href=#connection>Connection</a><a href=#timer>Timer</a><a href=#relationship>Relationship</a><a href=#creative>Creative</a><a href=#hardware>Hardware</a><a href=#update>Update</a></nav><button class=secondary onclick=returnToFace()>Return OLED to main face</button><p id=returnmsg class=hint>Use this after showing text, a drawing, or relationship time. A quick touch-pad tap also returns to the face.</p><h2 id=connection>Wi-Fi and clock</h2><button class=secondary onclick=scan()>Find nearby Wi-Fi</button><div id=nets class=hint>Tap Find nearby Wi-Fi.</div><form id=cfg><label>Selected network<input required name=ssid id=ssid maxlength=32 autocomplete=off></label><label>Wi-Fi password<input required name=pass type=password maxlength=63 autocomplete=new-password placeholder='Enter the password for this Wi-Fi'></label><label>Time zone<input name=tz value='Asia/Dhaka' maxlength=47></label><div class=row><label>Latitude<input name=lat value='24.8949' inputmode=decimal></label><label>Longitude<input name=lon value='91.8687' inputmode=decimal></label></div><button>Save settings and restart</button></form><p id=msg class=status></p><h2 id=timer>Timer sound and OLED brightness</h2><label>Timer alarm song<select id=song></select></label><button onclick=setSong()>Save timer sound</button><label>OLED brightness <span id=bv></span><input id=bright type=range min=1 max=255 value=180 oninput='bv.textContent=this.value;setBright()'></label><h2 id=relationship>Relationship</h2><p class=hint>Save the day you first met. The OLED shows a live days, hours, minutes, and seconds counter using the companion's clock.</p><label>First-meet date<input id=relDate type=date min=1970-01-01 max=2099-12-31></label><button onclick=showRelationship()>Start live relationship counter</button><p id=relmsg class=status></p><h2 id=creative>Live text</h2><textarea id=text maxlength=96 rows=3 placeholder='Write a short message. Use plain English letters and symbols.'></textarea><button onclick=sendText()>Show text on OLED</button><h2>Draw or show a picture</h2><input id=photo type=file accept='image/*'><label>Picture style<select id=picmode><option value=dither>Photo detail (recommended)</option><option value=crisp>Logo or text (crisp)</option></select></label><button class=secondary onclick=clearCanvas()>Clear drawing</button><canvas id=cv width=128 height=64></canvas><button onclick=sendCanvas()>Show drawing on OLED</button><p class=hint>Photos use a fine black-and-white dot pattern to keep more detail. Use a close-up with a clear subject; the OLED has only 128 by 64 dots.</p><h2 id=hardware>Hardware module test</h2><p class=hint>Checks the OLED connection and touch-pad signal. It plays a short buzzer tone; listen for it. Battery and PowerBoost voltage cannot be measured because no voltage-sense wire is fitted.</p><button onclick=hardwareTest()>Run hardware test</button><div id=hw class=hint>Not tested yet.</div><h2 id=update>Firmware update</h2><p class=hint>Choose the application firmware.bin made for this ESP32-C3 project. Do not choose a merged/full-flash .bin, bootloader .bin, partition-table .bin, ZIP, or source file. Keep USB power connected until the result appears.</p><form action=/update method=post enctype=multipart/form-data><input required name=firmware type=file accept=.bin,application/octet-stream><button>Install firmware update</button></form><p class=hint>The result will say whether the file was accepted, too large, not an ESP32 app image, or interrupted. A failed update leaves the currently running firmware in place.</p></main><script>const q=s=>document.querySelector(s),cv=q('#cv'),x=cv.getContext('2d');let drawing=false,bt;for(let i=1;i<=12;i++)q('#song').innerHTML+='<option value='+(i-1)+'>Song '+i+'</option>';q('#bv').textContent=q('#bright').value;x.fillStyle='white';x.fillRect(0,0,128,64);x.strokeStyle='black';x.lineWidth=3;x.lineCap='round';async function scan(){q('#nets').textContent='Searching...';let r=await fetch('/scan'),j=await r.json();if(j.state==='starting'){setTimeout(scan,900);return}q('#nets').innerHTML=j.networks.map(n=>'<button type=button class=net data-s="'+n.s+'">'+n.s+' - '+n.r+' dBm '+(n.sec?'secured':'open')+'</button>').join('')||'No networks found.';document.querySelectorAll('.net').forEach(b=>b.onclick=()=>{q('#ssid').value=b.dataset.s;q('#msg').textContent='Network selected. Enter its password below.'})}q('#cfg').onsubmit=async e=>{e.preventDefault();q('#msg').textContent='Saving...';let r=await fetch('/save',{method:'POST',body:new FormData(e.target)});q('#msg').textContent=await r.text()};async function returnToFace(){let r=await fetch('/return-main',{method:'POST'});q('#returnmsg').textContent=await r.text()}async function setSong(){await fetch('/song',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'song='+q('#song').value});q('#msg').textContent='Timer sound saved.'}function setBright(){clearTimeout(bt);bt=setTimeout(()=>fetch('/bright',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'value='+q('#bright').value}),100)}async function sendText(){let r=await fetch('/text',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'text='+encodeURIComponent(q('#text').value)});q('#msg').textContent=await r.text()}async function showRelationship(){let date=q('#relDate').value;if(!date){q('#relmsg').textContent='Choose the day you first met.';return}q('#relmsg').textContent='Saving...';let r=await fetch('/relationship',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'date='+encodeURIComponent(date)});q('#relmsg').textContent=await r.text()}async function hardwareTest(){q('#hw').textContent='Testing connected modules...';let r=await fetch('/hardware-test',{method:'POST'}),j=await r.json();q('#hw').innerHTML='<b>OLED: '+(j.oled?'SUCCESS':'FAILED')+'</b><br>Touch pad: '+(j.touch?'TOUCHED — SUCCESS':'ready — touch the pad, then run this test again')+'<br>Buzzer: TONE PLAYED — listen for the beep.<br>Battery: MANUAL CHECK — no voltage measurement wire is installed.<br>PowerBoost: MANUAL CHECK — confirm its power LED is lit and the 5 V output is connected.'}function pos(e){let r=cv.getBoundingClientRect(),p=e;return{x:(p.clientX-r.left)*128/r.width,y:(p.clientY-r.top)*64/r.height}}function line(e){if(!drawing)return;let p=pos(e);x.lineTo(p.x,p.y);x.stroke();e.preventDefault()}cv.onpointerdown=e=>{drawing=true;let p=pos(e);x.beginPath();x.moveTo(p.x,p.y);cv.setPointerCapture(e.pointerId)};cv.onpointermove=line;cv.onpointerup=()=>drawing=false;function clearCanvas(){x.fillStyle='white';x.fillRect(0,0,128,64)}function importImage(im){let t=document.createElement('canvas'),g=t.getContext('2d');t.width=128;t.height=64;g.fillStyle='white';g.fillRect(0,0,128,64);g.imageSmoothingEnabled=true;g.imageSmoothingQuality='high';let s=Math.min(128/im.width,64/im.height),w=Math.round(im.width*s),h=Math.round(im.height*s);g.drawImage(im,Math.round((128-w)/2),Math.round((64-h)/2),w,h);let p=g.getImageData(0,0,128,64),a=p.data;if(q('#picmode').value==='crisp'){for(let i=0;i<a.length;i+=4){let v=.299*a[i]+.587*a[i+1]+.114*a[i+2],z=v<150?0:255;a[i]=a[i+1]=a[i+2]=z}}else{let v=new Float32Array(8192);for(let i=0;i<8192;i++){let j=i*4;v[i]=.299*a[j]+.587*a[j+1]+.114*a[j+2]}for(let y=0;y<64;y++)for(let xx=0;xx<128;xx++){let i=y*128+xx,old=v[i],z=old<128?0:255,e=old-z;v[i]=z;if(xx<127)v[i+1]+=e*7/16;if(y<63){if(xx>0)v[i+127]+=e*3/16;v[i+128]+=e*5/16;if(xx<127)v[i+129]+=e/16}}for(let i=0;i<8192;i++){let j=i*4,z=v[i];a[j]=a[j+1]=a[j+2]=z;a[j+3]=255}}x.putImageData(p,0,0)}q('#photo').onchange=e=>{let f=e.target.files[0];if(!f)return;let im=new Image;im.onload=()=>{importImage(im);URL.revokeObjectURL(im.src)};im.src=URL.createObjectURL(f)};fetch('/relationship').then(r=>r.json()).then(j=>{if(j.date)q('#relDate').value=j.date}).catch(()=>{});function bits(){let p=x.getImageData(0,0,128,64).data,h='';for(let y=0;y<64;y++)for(let b=0;b<16;b++){let z=0;for(let k=0;k<8;k++){let i=(y*128+b*8+k)*4;if(p[i]+p[i+1]+p[i+2]<420)z|=128>>k}h+=z.toString(16).padStart(2,'0')}return h}async function sendCanvas(){q('#msg').textContent='Sending drawing...';let r=await fetch('/drawing',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'bits='+bits()});q('#msg').textContent=await r.text()}</script></body></html>)HTML"); }
static bool parseBits(const String &s)
{
    if (s.length() != 2048)
        return false;
    for (int i = 0; i < 1024; i++)
    {
        char a = s[i * 2], b = s[i * 2 + 1];
        int x = (a >= '0' && a <= '9' ? a - '0' : a >= 'a' && a <= 'f' ? a - 'a' + 10
                                                                       : -1),
            y = (b >= '0' && b <= '9' ? b - '0' : b >= 'a' && b <= 'f' ? b - 'a' + 10
                                                                       : -1);
        if (x < 0 || y < 0)
            return false;
        webBits[i] = (x << 4) | y;
    }
    return true;
}
static void featureRoutes()
{
    nv.begin("4ngry", true);
    cp(relationshipDate, sizeof relationshipDate, nv.getString("relDate", ""));
    nv.end();
    web.on("/return-main", HTTP_POST, []
           {
    clearFeatureDisplay();
    go(PET);
    web.send(200,"text/plain","OLED returned to the main face. The setup Wi-Fi remains available."); });
    web.on("/relationship", HTTP_GET, []
           { web.send(200, "application/json", String("{\"date\":\"") + relationshipDate + "\"}"); });
    web.on("/relationship", HTTP_POST, []
           {
    String value=web.arg("date");
    if(!validRelationshipDate(value)){web.send(400,"text/plain","Use a real date between 1970 and 2099.");return;}
    cp(relationshipDate,sizeof relationshipDate,value); nv.begin("4ngry",false); nv.putString("relDate",relationshipDate); nv.end();
    relationshipShown=true; customTextShown=false; customDrawingShown=false;
    web.send(200,"text/plain",String("Showing: ")+relationshipElapsed()+". Tap the touch pad to return."); });
    web.on("/song", HTTP_POST, []
           {int n=web.arg("song").toInt();if(n<0||n>11){web.send(400,"text/plain","Invalid song.");return;}alarmSong=n;nv.begin("4ngry",false);nv.putUChar("song",n);nv.end();web.send(200,"text/plain","OK"); });
    web.on("/bright", HTTP_POST, []
           {int n=web.arg("value").toInt();if(n<1||n>255){web.send(400,"text/plain","Invalid brightness.");return;}oledBrightness=n;d.setContrast(n);nv.begin("4ngry",false);nv.putUChar("bright",n);nv.end();web.send(200,"text/plain","OK"); });
    web.on("/text", HTTP_POST, []
           {String v=web.arg("text");int j=0;for(uint i=0;i<v.length()&&j<96;i++){char z=v[i];if((z>=32&&z<=126)||z=='\n'||z=='\r')webText[j++]=z;}webText[j]=0;customTextShown=true;customDrawingShown=false;web.send(200,"text/plain","Text is now on the OLED."); });
    web.on("/drawing", HTTP_POST, []
           {if(!parseBits(web.arg("bits"))){web.send(400,"text/plain","Drawing data was incomplete. Please try again.");return;}customDrawingShown=true;customTextShown=false;web.send(200,"text/plain","Drawing is now on the OLED."); });
    web.on("/hardware-test", HTTP_POST, []
           {Wire.beginTransmission(OLED_I2C_ADDR);bool oledOk=(Wire.endTransmission()==0);bool touchActive=digitalRead(TOUCH_PIN)==HIGH;hardwareBeepUntil=millis()+350;ledcWriteTone(BUZZER_PIN,1760);String result="{\"oled\":"+String(oledOk?"true":"false")+",\"touch\":"+String(touchActive?"true":"false")+"}";web.send(200,"application/json",result); });
}
static void startAlarmSong()
{
    melodyPlaying = true;
    melodyNote = 0;
    melodyAt = 0;
}
static void updateAlarmSong()
{
    uint32_t n = millis();
    if (hardwareBeepUntil)
    {
        if (n < hardwareBeepUntil)
        {
            ledcWriteTone(BUZZER_PIN, 1760);
            return;
        }
        hardwareBeepUntil = 0;
        ledcWriteTone(BUZZER_PIN, 0);
    }
    if (!melodyPlaying)
        return;
    if (!melodyAt || n - melodyAt >= 150)
    {
        if (melodyAt)
            melodyNote++;
        if (melodyNote >= melodyLen[alarmSong])
        {
            melodyPlaying = false;
            ledcWriteTone(BUZZER_PIN, 0);
            return;
        }
        ledcWriteTone(BUZZER_PIN, melody[alarmSong][melodyNote]);
        melodyAt = n;
    }
}
static bool isFeatureDisplayActive() { return customTextShown || customDrawingShown || relationshipShown; }
static void drawWebConfirm()
{
    d.clearBuffer();
    d.setFont(u8g2_font_6x12_tf);
    d.drawStr(15, 16, "OPEN WEB PAGE?");
    d.setFont(u8g2_font_5x8_tf);
    d.drawStr(11, 30, "Keep holding to confirm");
    d.drawFrame(12, 38, 104, 12);
    uint32_t held = millis() - webConfirmAt;
    d.drawBox(14, 40, min(100, (int)(held * 100 / 3000)), 8);
    d.drawStr(17, 62, "Release now to cancel");
    d.sendBuffer();
}
static void drawFeatureDisplay()
{
    if (customTextShown)
    {
        d.clearBuffer();
        d.setFont(u8g2_font_6x12_tf);
        int y = 16, start = 0;
        for (int i = 0; i <= strlen(webText) && y < 64; i++)
            if (webText[i] == '\n' || webText[i] == 0)
            {
                char z[22];
                int n = min(21, i - start);
                memcpy(z, webText + start, n);
                z[n] = 0;
                d.drawStr(max(0, (128 - d.getStrWidth(z)) / 2), y, z);
                y += 15;
                start = i + 1;
            }
        d.sendBuffer();
    }
    else if (customDrawingShown)
    {
        d.clearBuffer();
        for (int y = 0; y < 64; y++)
            for (int x = 0; x < 128; x++)
                if (webBits[y * 16 + x / 8] & (0x80 >> (x & 7)))
                    d.drawPixel(x, y);
        d.sendBuffer();
    }
    else if (relationshipShown)
    {
        d.clearBuffer();
        int64_t total = relationshipSeconds();
        d.setFont(u8g2_font_6x12_tf);
        d.drawStr(20, 12, "TOGETHER FOR");
        if (total < 0)
        {
            String message = relationshipElapsed();
            d.setFont(u8g2_font_6x12_tf);
            d.drawStr(max(0, (128 - d.getStrWidth(message.c_str())) / 2), 37, message.c_str());
        }
        else
        {
            uint32_t days = (uint32_t)(total / 86400);
            uint32_t rest = (uint32_t)(total % 86400);
            char dayLine[18], clockLine[12];
            snprintf(dayLine, sizeof(dayLine), "%lu DAYS", (unsigned long)days);
            snprintf(clockLine, sizeof(clockLine), "%02lu:%02lu:%02lu", (unsigned long)(rest / 3600), (unsigned long)((rest / 60) % 60), (unsigned long)(rest % 60));
            d.setFont(u8g2_font_helvB12_tf);
            d.drawStr(max(0, (128 - d.getStrWidth(dayLine)) / 2), 31, dayLine);
            d.setFont(u8g2_font_logisoso16_tf);
            d.drawStr(max(0, (128 - d.getStrWidth(clockLine)) / 2), 51, clockLine);
        }
        d.setFont(u8g2_font_5x8_tf);
        d.drawStr(14, 63, "Tap touch pad to return");
        d.sendBuffer();
    }
}
