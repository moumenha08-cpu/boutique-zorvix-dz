// متجر Zorvix - خادم C++ (cpp-httplib + nlohmann/json)
// بناء:  g++ -std=c++17 main.cpp -o store -pthread
// تشغيل: ADMIN_PASSWORD=كلمة_سر_قوية ./store   ثم افتح http://localhost:8080
// لوحة الإدارة: http://localhost:8080/admin
#include "httplib.h"
#include "json.hpp"
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <mutex>
#include <random>
#include <regex>

using json = nlohmann::json;
static std::mutex mtx;
static json db;
static std::string adminToken, adminPass;

static void save() { std::ofstream("data.json") << db.dump(2); }
static void load() {
  std::ifstream f("data.json");
  if (f) db = json::parse(f, nullptr, false);
  if (!db.is_object()) db = json::object();
  if (!db.contains("products")) db["products"] = json::array();
  if (!db.contains("orders")) db["orders"] = json::array();
  if (!db.contains("delivery")) db["delivery"] = 600;
  if (!db.contains("nextId")) db["nextId"] = 1;
}
static bool isAdmin(const httplib::Request& r) {
  return !adminToken.empty() && r.get_header_value("X-Token") == adminToken;
}
static std::string randomToken() {
  std::random_device rd; std::mt19937_64 g(rd());
  char b[40]; snprintf(b, sizeof b, "%016llx%016llx", (unsigned long long)g(), (unsigned long long)g());
  return b;
}
static void out(httplib::Response& res, const json& j, int st = 200) {
  res.status = st; res.set_content(j.dump(), "application/json; charset=utf-8");
}

static const char* STORE_HTML = R"HTML(<!doctype html><html lang="ar" dir="rtl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>ZORVIX STREETWEAR DZ</title>
<style>
*{box-sizing:border-box}body{margin:0;font-family:system-ui,Tahoma,sans-serif;color:#eaf2ff;min-height:100vh;
background:radial-gradient(circle at 20% 0%,rgba(80,140,255,.35),transparent 45%),radial-gradient(circle at 90% 30%,rgba(0,200,255,.18),transparent 40%),linear-gradient(135deg,#010616,#06174a 50%,#020a26);background-attachment:fixed}
header{display:flex;justify-content:space-between;align-items:center;padding:14px 18px;position:sticky;top:0;background:rgba(2,10,38,.75);backdrop-filter:blur(10px);border-bottom:1px solid rgba(120,170,255,.25);z-index:5}
header b{letter-spacing:4px;font-size:20px}header small{display:block;letter-spacing:3px;color:#7fb0ff;font-size:11px}
.btn{background:linear-gradient(135deg,#1c4fd8,#0ea5e9);color:#fff;border:0;border-radius:99px;padding:10px 20px;font-size:15px;cursor:pointer;box-shadow:0 0 14px rgba(14,165,233,.5)}
main{max-width:900px;margin:auto;padding:16px}
.glass,.card{background:rgba(255,255,255,.07);border:1px solid rgba(140,185,255,.25);border-radius:20px;padding:16px;backdrop-filter:blur(8px);box-shadow:0 8px 30px rgba(0,20,90,.5), inset 0 1px 0 rgba(255,255,255,.15)}
#grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(200px,1fr));gap:14px;margin:16px 0}
.card img{width:100%;border-radius:14px;aspect-ratio:1;object-fit:cover}.d{color:#9db8e8;font-size:13px}
input,select,textarea{width:100%;padding:12px;margin:5px 0 12px;border-radius:12px;border:1px solid #3b63b8;background:rgba(2,10,40,.7);color:#fff;font-size:15px}
label{font-size:14px;color:#b9d0ff}.row{display:flex;justify-content:space-between;align-items:center;gap:8px;padding:8px 0;border-bottom:1px solid rgba(255,255,255,.1)}
.tot{font-size:18px;font-weight:bold;color:#5eead4}#cart{display:none}.q button{width:30px;height:30px;border-radius:50%;border:0;background:#1c4fd8;color:#fff}
</style></head><body>
<header><div><b>ZORVIX</b><small>STREETWEAR · DZ</small></div><button class="btn" onclick="toggleCart()">🛍 السلة (<span id="cnt">0</span>)</button></header>
<main>
<h2>القطع المتوفرة</h2><div id="grid"></div>
<section id="cart"><div class="glass"><h2>السلة تاعك</h2><div id="items"></div>
<div class="row"><span>مجموع المنتجات</span><span id="sub">0</span></div>
<div class="row"><span>سعر التوصيل</span><span id="del">0</span></div>
<div class="row tot"><span>السعر الإجمالي</span><span id="tot">0</span></div></div><br>
<div class="glass"><h2>معلومات التوصيل</h2>
<label>الاسم الكامل *</label><input id="name" placeholder="كيفاش نناديولك؟">
<label>رقم الهاتف *</label><input id="phone" inputmode="numeric" placeholder="05 / 06 / 07 ...">
<label>الولاية *</label><select id="wil"><option value="">اختار الولاية</option></select>
<label>البلدية *</label><input id="com" placeholder="البلدية">
<label>العنوان بالتفصيل (اختياري)</label><input id="addr" placeholder="الحي، رقم العمارة...">
<label>ملاحظة (اختياري)</label><textarea id="note" rows="3" placeholder="مقاس أو تفاصيل أخرى..."></textarea>
<button class="btn" style="width:100%" onclick="order()">أكد الطلب</button>
<p class="d">توصيل للدار · الدفع عند الاستلام</p><p id="msg"></p></div></section>
</main>
<script>
const $=s=>document.querySelector(s);
function el(t,c,x){const e=document.createElement(t);if(c)e.className=c;if(x!==undefined)e.textContent=x;return e}
const W=("أدرار,الشلف,الأغواط,أم البواقي,باتنة,بجاية,بسكرة,بشار,البليدة,البويرة,تمنراست,تبسة,تلمسان,تيارت,تيزي وزو,الجزائر,الجلفة,جيجل,سطيف,سعيدة,سكيكدة,سيدي بلعباس,عنابة,قالمة,قسنطينة,المدية,مستغانم,المسيلة,معسكر,ورقلة,وهران,البيض,إليزي,برج بوعريريج,بومرداس,الطارف,تندوف,تيسمسيلت,الوادي,خنشلة,سوق أهراس,تيبازة,ميلة,عين الدفلى,النعامة,عين تموشنت,غرداية,غليزان,تيميمون,برج باجي مختار,أولاد جلال,بني عباس,عين صالح,عين قزام,توقرت,جانت,المغير,المنيعة,أفلو,البيض سيدي الشيخ,العريشة,القنطرة,بريكة,بوسعادة,بئر العاتر,قصر البخاري,قصر الشلالة,عين وسارة,مسعد").split(",");
W.forEach((w,i)=>{const o=el('option','',(i+1)+' - '+w);o.value=w;$('#wil').append(o)});
let P=[],D=0,cart=JSON.parse(localStorage.getItem('cart')||'{}');
const money=n=>n.toLocaleString('en')+' دج';
function toggleCart(){const c=$('#cart');c.style.display=c.style.display==='block'?'none':'block'}
function saveCart(){localStorage.setItem('cart',JSON.stringify(cart));drawCart()}
async function init(){const j=await (await fetch('/api/products')).json();P=j.products;D=j.delivery;
 const g=$('#grid');g.replaceChildren();if(!P.length)g.append(el('p','d','ماكانش قطع متوفرة حاليا، نوجدولكم دروبات جديدة.'));
 P.forEach(p=>{const c=el('div','card');if(p.image){const i=el('img');i.src=p.image;c.append(i)}
  c.append(el('h3','',p.name),el('p','d',p.desc||''),el('b','',money(p.price)),el('br'));
  const b=el('button','btn','أضف للسلة');b.style.marginTop='8px';b.onclick=()=>{cart[p.id]=(cart[p.id]||0)+1;saveCart()};c.append(b);g.append(c)});drawCart()}
function totals(){let s=0;for(const id in cart){const p=P.find(x=>x.id==id);if(p)s+=p.price*cart[id]}return[s,s>0?D:0]}
function drawCart(){const box=$('#items');box.replaceChildren();let n=0;
 for(const id in cart){const p=P.find(x=>x.id==id);if(!p){delete cart[id];continue}n+=cart[id];
  const r=el('div','row');r.append(el('span','',p.name+' — '+money(p.price)));
  const q=el('span','q');const m=el('button','','−'),a=el('button','','+');
  m.onclick=()=>{if(--cart[id]<=0)delete cart[id];saveCart()};a.onclick=()=>{cart[id]++;saveCart()};
  q.append(m,' '+cart[id]+' ',a);r.append(q);box.append(r)}
 if(!n)box.append(el('p','d','السلة مازالها فارغة'));
 const[s,d]=totals();$('#cnt').textContent=n;$('#sub').textContent=money(s);$('#del').textContent=money(d);$('#tot').textContent=money(s+d)}
async function order(){const items=Object.keys(cart).map(id=>({id:+id,qty:cart[id]}));
 const body={name:$('#name').value,phone:$('#phone').value,wilaya:$('#wil').value,commune:$('#com').value,address:$('#addr').value,note:$('#note').value,items};
 const r=await fetch('/api/orders',{method:'POST',body:JSON.stringify(body)});const j=await r.json();
 $('#msg').textContent=j.ok?('تم الطلب ✅ الإجمالي: '+money(j.total)):('⚠️ '+j.error);
 if(j.ok){cart={};saveCart()}}
init();
</script></body></html>)HTML";

static const char* ADMIN_HTML = R"HTML(<!doctype html><html lang="ar" dir="rtl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>لوحة الإدارة</title>
<style>*{box-sizing:border-box}body{margin:0;padding:16px;font-family:system-ui,Tahoma,sans-serif;color:#eaf2ff;min-height:100vh;background:linear-gradient(135deg,#010616,#06174a 50%,#020a26)}
.box{max-width:800px;margin:0 auto 16px;background:rgba(255,255,255,.07);border:1px solid rgba(140,185,255,.25);border-radius:18px;padding:16px}
input{width:100%;padding:11px;margin:4px 0 10px;border-radius:10px;border:1px solid #3b63b8;background:rgba(2,10,40,.7);color:#fff}
button{background:linear-gradient(135deg,#1c4fd8,#0ea5e9);color:#fff;border:0;border-radius:99px;padding:9px 16px;cursor:pointer}
.row{display:flex;gap:8px;align-items:center;justify-content:space-between;padding:8px 0;border-bottom:1px solid rgba(255,255,255,.1);flex-wrap:wrap}.row input{width:110px;margin:0}
.red{background:#b91c1c}</style></head><body>
<div class="box" id="login"><h2>دخول المدير</h2><input id="pw" type="password" placeholder="كلمة السر"><button onclick="login()">دخول</button><p id="err"></p></div>
<div id="panel" style="display:none">
<div class="box"><h3>سعر التوصيل</h3><input id="dv" type="number"><button onclick="setDel()">حفظ</button></div>
<div class="box"><h3>إضافة منتج</h3><input id="pn" placeholder="الاسم"><input id="pp" type="number" placeholder="السعر (دج)">
<input id="pi" placeholder="رابط الصورة"><input id="pd" placeholder="وصف قصير"><button onclick="addP()">إضافة</button></div>
<div class="box"><h3>المنتجات</h3><div id="plist"></div></div>
<div class="box"><h3>الطلبات</h3><div id="olist"></div></div></div>
<script>
const $=s=>document.querySelector(s);let T='';
function el(t,c,x){const e=document.createElement(t);if(c)e.className=c;if(x!==undefined)e.textContent=x;return e}
async function api(u,b){const r=await fetch(u,{method:'POST',headers:{'X-Token':T},body:JSON.stringify(b||{})});return r.json()}
async function login(){const j=await api('/api/admin/login',{password:$('#pw').value});
 if(j.token){T=j.token;$('#login').style.display='none';$('#panel').style.display='block';load()}else $('#err').textContent='كلمة السر خاطئة'}
async function load(){const j=await api('/api/admin/data');$('#dv').value=j.delivery;
 const pl=$('#plist');pl.replaceChildren();j.products.forEach(p=>{const r=el('div','row');r.append(el('span','',p.name));
  const i=el('input');i.type='number';i.value=p.price;const s=el('button','','حفظ السعر');s.onclick=async()=>{await api('/api/admin/update',{id:p.id,price:+i.value});load()};
  const d=el('button','red','حذف');d.onclick=async()=>{if(confirm('حذف؟')){await api('/api/admin/delete',{id:p.id});load()}};r.append(i,s,d);pl.append(r)});
 const ol=$('#olist');ol.replaceChildren();j.orders.slice().reverse().forEach(o=>{const r=el('div','row');
  r.append(el('span','',`#${o.id} | ${o.name} | ${o.phone} | ${o.wilaya} - ${o.commune} | ${o.address||''} | ${o.items.map(x=>x.name+'×'+x.qty).join('، ')} | الإجمالي: ${o.total} دج | ${o.note||''}`));ol.append(r)})}
async function setDel(){await api('/api/admin/delivery',{delivery:+$('#dv').value});load()}
async function addP(){const j=await api('/api/admin/add',{name:$('#pn').value,price:+$('#pp').value,image:$('#pi').value,desc:$('#pd').value});
 if(j.ok){['pn','pp','pi','pd'].forEach(x=>$('#'+x).value='');load()}else alert(j.error)}
</script></body></html>)HTML";

int main() {
  const char* pw = std::getenv("ADMIN_PASSWORD");
  adminPass = pw ? pw : "change_me";
  load();
  httplib::Server svr;

  svr.Get("/", [](const httplib::Request&, httplib::Response& res) { res.set_content(STORE_HTML, "text/html; charset=utf-8"); });
  svr.Get("/admin", [](const httplib::Request&, httplib::Response& res) { res.set_content(ADMIN_HTML, "text/html; charset=utf-8"); });

  // ---- عام: المنتجات + سعر التوصيل ----
  svr.Get("/api/products", [](const httplib::Request&, httplib::Response& res) {
    std::lock_guard<std::mutex> l(mtx);
    out(res, {{"products", db["products"]}, {"delivery", db["delivery"]}});
  });

  // ---- عام: إنشاء طلب (السعر يُحسب في الخادم وليس من المتصفح) ----
  svr.Post("/api/orders", [](const httplib::Request& req, httplib::Response& res) {
    json b = json::parse(req.body, nullptr, false);
    if (!b.is_object()) return out(res, {{"ok", false}, {"error", "طلب غير صالح"}}, 400);
    std::lock_guard<std::mutex> l(mtx);
    std::string name = b.value("name", ""), phone = b.value("phone", ""), wil = b.value("wilaya", ""), com = b.value("commune", "");
    if (name.empty() || wil.empty() || com.empty()) return out(res, {{"ok", false}, {"error", "عمر الخانات الإجبارية"}});
    if (!std::regex_match(phone, std::regex("0[567][0-9]{8}"))) return out(res, {{"ok", false}, {"error", "رقم الهاتف غير صحيح"}});
    long sub = 0; json items = json::array();
    if (b.contains("items") && b["items"].is_array())
      for (auto& it : b["items"])
        for (auto& p : db["products"])
          if (p["id"] == it.value("id", -1)) {
            int q = it.value("qty", 0);
            if (q < 1 || q > 50) continue;
            sub += p["price"].get<long>() * q;
            items.push_back({{"name", p["name"]}, {"qty", q}, {"price", p["price"]}});
          }
    if (items.empty()) return out(res, {{"ok", false}, {"error", "السلة فارغة"}});
    long delivery = db["delivery"].get<long>(), total = sub + delivery;
    int id = db["orders"].size() + 1;
    db["orders"].push_back({{"id", id}, {"name", name}, {"phone", phone}, {"wilaya", wil}, {"commune", com},
                            {"address", b.value("address", "")}, {"note", b.value("note", "")}, {"items", items},
                            {"subtotal", sub}, {"delivery", delivery}, {"total", total}, {"time", (long)std::time(nullptr)}});
    save();
    out(res, {{"ok", true}, {"total", total}});
  });

  // ---- المدير فقط ----
  svr.Post("/api/admin/login", [](const httplib::Request& req, httplib::Response& res) {
    json b = json::parse(req.body, nullptr, false);
    if (b.is_object() && b.value("password", "") == adminPass) {
      std::lock_guard<std::mutex> l(mtx); adminToken = randomToken();
      return out(res, {{"token", adminToken}});
    }
    out(res, {{"error", "denied"}}, 401);
  });
  svr.Post("/api/admin/data", [](const httplib::Request& req, httplib::Response& res) {
    if (!isAdmin(req)) return out(res, {{"error", "denied"}}, 401);
    std::lock_guard<std::mutex> l(mtx); out(res, db);
  });
  svr.Post("/api/admin/add", [](const httplib::Request& req, httplib::Response& res) {
    if (!isAdmin(req)) return out(res, {{"error", "denied"}}, 401);
    json b = json::parse(req.body, nullptr, false);
    if (!b.is_object() || b.value("name", "").empty() || b.value("price", 0) <= 0) return out(res, {{"ok", false}, {"error", "الاسم والسعر مطلوبان"}});
    std::lock_guard<std::mutex> l(mtx);
    int id = db["nextId"]; db["nextId"] = id + 1;
    db["products"].push_back({{"id", id}, {"name", b["name"]}, {"price", b["price"]}, {"image", b.value("image", "")}, {"desc", b.value("desc", "")}});
    save(); out(res, {{"ok", true}});
  });
  svr.Post("/api/admin/update", [](const httplib::Request& req, httplib::Response& res) {
    if (!isAdmin(req)) return out(res, {{"error", "denied"}}, 401);
    json b = json::parse(req.body, nullptr, false);
    if (!b.is_object()) return out(res, {{"ok", false}}, 400);
    std::lock_guard<std::mutex> l(mtx);
    for (auto& p : db["products"]) if (p["id"] == b.value("id", -1) && b.value("price", 0) > 0) p["price"] = b["price"];
    save(); out(res, {{"ok", true}});
  });
  svr.Post("/api/admin/delete", [](const httplib::Request& req, httplib::Response& res) {
    if (!isAdmin(req)) return out(res, {{"error", "denied"}}, 401);
    json b = json::parse(req.body, nullptr, false);
    if (!b.is_object()) return out(res, {{"ok", false}}, 400);
    std::lock_guard<std::mutex> l(mtx);
    json np = json::array();
    for (auto& p : db["products"]) if (p["id"] != b.value("id", -1)) np.push_back(p);
    db["products"] = np; save(); out(res, {{"ok", true}});
  });
  svr.Post("/api/admin/delivery", [](const httplib::Request& req, httplib::Response& res) {
    if (!isAdmin(req)) return out(res, {{"error", "denied"}}, 401);
    json b = json::parse(req.body, nullptr, false);
    if (!b.is_object() || b.value("delivery", -1) < 0) return out(res, {{"ok", false}}, 400);
    std::lock_guard<std::mutex> l(mtx);
    db["delivery"] = b["delivery"]; save(); out(res, {{"ok", true}});
  });

  printf("Store: http://localhost:8080   Admin: http://localhost:8080/admin\n");
  svr.listen("0.0.0.0", 8080);
}
