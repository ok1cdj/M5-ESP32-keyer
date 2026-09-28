// HTTP transport: the v1 API (/sendmorse and / with parameters) plus /cmd
// with the text protocol, and a small built-in page.
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include "core/commands.h"
#include "element_generator.h"
#include "core/keyer_task.h"
#include "settings.h"
#include "transport/transport.h"
#include "transport/wifi_station.h"

namespace transport {

namespace {

const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CW keyer</title>
<style>
body{font-family:system-ui,sans-serif;max-width:32rem;margin:1rem auto;padding:0 1rem;background:#fff;color:#111}
@media(prefers-color-scheme:dark){body{background:#111;color:#eee}input,button{background:#222;color:#eee;border-color:#555}}
input,button{font-size:1.1rem;padding:.5rem;border:1px solid #999;border-radius:.4rem;box-sizing:border-box}
#msg{width:100%;margin-bottom:.5rem}#wpm{width:5rem}
.row{display:flex;gap:.5rem;align-items:center;flex-wrap:wrap}
#stop{background:#c00;color:#fff;border-color:#c00}#st{margin-top:1rem;font-family:monospace}
</style></head><body>
<h1>CW keyer</h1>
<input id="msg" placeholder="CQ CQ DE ..." autofocus>
<div class="row"><button id="send">Send</button><button id="stop">STOP</button>
<label>WPM <input id="wpm" type="number" min="5" max="50"></label><button id="set">Set</button></div>
<div id="st"></div>
<script>
const key=new URLSearchParams(location.search).get('apikey')||'';
async function cmd(c){const r=await fetch('/cmd?apikey='+encodeURIComponent(key)+'&c='+encodeURIComponent(c));return r.text();}
async function status(){const s=await cmd('STATUS');document.getElementById('st').textContent=s;
 const m=s.match(/WPM (\d+)/);const w=document.getElementById('wpm');if(m&&document.activeElement!==w)w.value=m[1];}
document.getElementById('send').onclick=async()=>{const m=document.getElementById('msg');if(m.value){await cmd('SEND '+m.value);m.value='';status();}};
document.getElementById('msg').onkeydown=e=>{if(e.key==='Enter')document.getElementById('send').click();};
document.getElementById('stop').onclick=async()=>{await cmd('STOP');status();};
document.getElementById('set').onclick=async()=>{document.getElementById('st').textContent=await cmd('WPM '+document.getElementById('wpm').value);};
status();setInterval(status,2000);
</script></body></html>)HTML";

bool authorized(AsyncWebServerRequest* req) {
    std::string key = settings::apiKey();
    if (key.empty()) return true;
    return req->hasParam("apikey") && req->getParam("apikey")->value() == key.c_str();
}

// v1 semantics: speed applies immediately, a message replaces what is queued.
void applyV1Params(AsyncWebServerRequest* req) {
    if (req->hasParam("speed")) {
        long speed = req->getParam("speed")->value().toInt();
        if (speed >= keyer::WPM_MIN && speed <= keyer::WPM_MAX) keyer_task::postWpm(uint8_t(speed));
    }
    if (req->hasParam("message")) {
        const String& msg = req->getParam("message")->value();
        keyer_task::postStop();
        keyer_task::postText(msg.c_str(), msg.length());
    }
}

class Http : public Transport {
public:
    void begin() override {
        wifi_station::begin("http", "tcp", 80);
        server_ = new AsyncWebServer(80);

        server_->on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
            if (!authorized(req)) return req->send(401, "text/plain", "Unauthorized");
            applyV1Params(req);
            req->send(200, "text/html", PAGE);
        });

        server_->on("/sendmorse", HTTP_GET, [](AsyncWebServerRequest* req) {
            if (!authorized(req)) return req->send(401, "text/plain", "Unauthorized");
            applyV1Params(req);
            req->send(200, "text/plain", "OK");
        });

        server_->on("/cmd", HTTP_ANY, [](AsyncWebServerRequest* req) {
            if (!authorized(req)) return req->send(401, "text/plain", "Unauthorized");
            const AsyncWebParameter* p = req->getParam("c");
            if (!p) p = req->getParam("c", true);
            if (!p) return req->send(400, "text/plain", "ERR arg");
            std::string line = p->value().c_str();
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
            std::string resp = commands::execute(line, proto::Mode::Http);
            req->send(200, "text/plain", (resp + "\n").c_str());
        });

        server_->onNotFound([](AsyncWebServerRequest* req) { req->send(404, "text/plain", "Not found"); });
        server_->begin();
    }

    // No channel for asynchronous messages over HTTP; they go to Serial only.
    void notify(const char*) override {}

    std::string address() const override { return wifi_station::address(); }

private:
    AsyncWebServer* server_ = nullptr;
};

}  // namespace

Transport& http() {
    static Http instance;
    return instance;
}

}  // namespace transport
