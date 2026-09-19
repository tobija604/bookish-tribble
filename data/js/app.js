// SMART BOX — spletna nadzorna plošča. Navadni JS, brez gradnje (build),
// brez CDN odvisnosti (naprava mora delovati tudi popolnoma brez interneta,
// glej specifikacijo, poglavje 23 — zato ničesar tukaj ne sme nalagati z
// interneta). Komunicira z REST API vdelanega strežnika pod /api/*
// (glej src/web/WebServer.h za natančno pogodbo/kontrakt).

const api = {
  async req(method, path, body) {
    const opts = { method, headers: {}, credentials: 'same-origin' };
    if (method !== 'GET') opts.headers['X-SmartBox-Request'] = '1';
    if (body !== undefined) { opts.headers['Content-Type'] = 'application/json'; opts.body = JSON.stringify(body); }
    const res = await fetch(path, opts);
    let data = {};
    try { data = await res.json(); } catch (e) { /* prazno telo je v redu */ }
    if (!res.ok) throw new Error(data.error || ('HTTP ' + res.status));
    return data;
  },
  get(p) { return this.req('GET', p); },
  post(p, b) { return this.req('POST', p, b || {}); },
  patch(p, b) { return this.req('PATCH', p, b || {}); },
  del(p) { return this.req('DELETE', p); },
};

function toast(msg, kind) {
  const el = document.getElementById('toast');
  el.textContent = msg;
  el.className = 'toast ' + (kind || '');
  el.classList.remove('hidden');
  clearTimeout(toast._t);
  toast._t = setTimeout(() => el.classList.add('hidden'), 3500);
}

function fmtTs(epoch) {
  if (!epoch) return '-';
  const d = new Date(epoch * 1000);
  return d.toLocaleString('sl-SI');
}

function esc(s) {
  return String(s ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
}

// ---------------------------------------------------------------------------
// Zagon: odloči, kaj prikazati (NASTAVITEV WIFI, PRIJAVA ali že prijavljen)
// ---------------------------------------------------------------------------
async function boot() {
  applyThemeFromServer();
  try {
    const net = await api.get('/api/network');
    if (!net.provisioned) { showSetup(); return; }
  } catch (e) { /* nadaljuj na prijavo */ }

  try {
    const session = await api.get('/api/auth/session');
    if (session.authenticated) { enterApp(); return; }
  } catch (e) { /* ni prijavljen */ }

  showLogin();
}

function showGatePanel(id) {
  document.getElementById('gate').classList.remove('hidden');
  document.getElementById('shell').classList.add('hidden');
  ['setupPanel', 'loginPanel', 'codePanel'].forEach(p => document.getElementById(p).classList.add('hidden'));
  document.getElementById(id).classList.remove('hidden');
}
function showSetup() { showGatePanel('setupPanel'); }
function showLogin() { showGatePanel('loginPanel'); }
function showCodeLogin() { showGatePanel('codePanel'); }

function wireGate() {
  document.getElementById('setupSubmit').onclick = async () => {
    const status = document.getElementById('setupStatus');
    status.textContent = 'Povezujem...';
    try {
      await api.post('/api/network/setup', {
        ssid: document.getElementById('setupSsid').value,
        password: document.getElementById('setupPass').value,
        deviceName: document.getElementById('setupDeviceName').value,
      });
      status.textContent = 'Povezano — stran se bo znova naložila...';
      setTimeout(() => location.reload(), 2500);
    } catch (e) { status.textContent = 'Napaka: ' + e.message; }
  };

  document.getElementById('loginSubmit').onclick = async () => {
    const status = document.getElementById('loginStatus');
    try {
      await api.post('/api/auth/login', {
        username: document.getElementById('loginUser').value,
        password: document.getElementById('loginPass').value,
      });
      enterApp();
    } catch (e) { status.textContent = 'Napačno uporabniško ime ali geslo'; }
  };

  document.getElementById('codeSubmit').onclick = async () => {
    const status = document.getElementById('codeStatus');
    try {
      await api.post('/api/auth/security-code', { code: document.getElementById('codeInput').value });
      enterApp();
    } catch (e) { status.textContent = 'NEVELJAVNA ali POTEKLA koda'; }
  };

  document.getElementById('showCodeLogin').onclick = showCodeLogin;
  document.getElementById('showUserLogin').onclick = showLogin;
  document.getElementById('logoutBtn').onclick = async () => { await api.post('/api/auth/logout'); location.reload(); };
}

// ---------------------------------------------------------------------------
// Ogrodje aplikacije
// ---------------------------------------------------------------------------
let pollTimer = null;

function enterApp() {
  document.getElementById('gate').classList.add('hidden');
  document.getElementById('shell').classList.remove('hidden');
  refreshTopbar();
  clearInterval(pollTimer);
  pollTimer = setInterval(refreshTopbar, 4000);
  setInterval(tickClock, 1000);
  router();
}

async function refreshTopbar() {
  try {
    const s = await api.get('/api/status');
    document.getElementById('lockPill').textContent = s.locked ? '🔒 ZAKLENJENO' : (s.lockState === 'ERROR' ? '⚠ NAPAKA ZAKLEPA' : '🔓 ODKLENJENO');
    document.getElementById('lockPill').style.color = s.locked ? 'var(--locked)' : 'var(--unlocked)';
    document.getElementById('lidPill').textContent = 'POKROV: ' + lidLabel(s.lid);
    document.getElementById('wifiPill').textContent = 'WiFi: ' + wifiLabel(s.wifi);
    window._lastStatus = s;
  } catch (e) { /* naprava se morda ponovno zaganja / posodablja — prehodne napake ignoriramo */ }
}

function lidLabel(v) { return { CLOSED: 'ZAPRTO', OPEN: 'ODPRTO', UNKNOWN: 'NEZNANO' }[v] || v; }
function wifiLabel(v) { return { CONNECTED: 'POVEZAN', AP_SETUP: 'NASTAVITEV', OFFLINE: 'BREZ POVEZAVE', CONNECTING: 'POVEZOVANJE' }[v] || v; }

function tickClock() {
  document.getElementById('clock').textContent = new Date().toLocaleTimeString('sl-SI', {hour: '2-digit', minute:'2-digit'});
}

function applyTheme(a) {
  const r = document.documentElement.style;
  const map = {
    colorPrimary: '--primary', colorSecondary: '--secondary', colorBackground: '--background',
    colorCard: '--card', colorText: '--text', colorMuted: '--muted', colorSuccess: '--success',
    colorWarning: '--warning', colorDanger: '--danger', colorInfo: '--info',
    colorLocked: '--locked', colorUnlocked: '--unlocked',
  };
  Object.entries(map).forEach(([k, v]) => { if (a[k]) r.setProperty(v, a[k]); });
  if (a.borderRadius !== undefined) r.setProperty('--radius', a.borderRadius + 'px');
}
async function applyThemeFromServer() {
  try { applyTheme(await api.get('/api/appearance')); } catch (e) {}
}

window.addEventListener('hashchange', router);

function setActiveNav(name) {
  document.querySelectorAll('.sidebar a').forEach(a => a.classList.toggle('active', a.dataset.nav === name));
}

async function router() {
  const route = (location.hash || '#dashboard').slice(1);
  setActiveNav(route);
  const view = document.getElementById('view');
  view.innerHTML = '<p class="muted">Nalagam...</p>';
  try {
    const renderer = views[route] || views.dashboard;
    await renderer(view);
  } catch (e) {
    view.innerHTML = `<p class="muted">Napaka: ${esc(e.message)}</p>`;
  }
}

// ---------------------------------------------------------------------------
// Izris posameznih razdelkov (glej specifikacijo, poglavje 25 — meni)
// ---------------------------------------------------------------------------
const views = {};

views.dashboard = async (view) => {
  const [status, stats] = await Promise.all([api.get('/api/status'), api.get('/api/statistics')]);
  view.innerHTML = `
    <h1>Nadzorna plošča</h1>
    <div class="grid">
      <div class="card"><h3>Ključavnica</h3><div class="value" style="color:${status.locked ? 'var(--locked)' : 'var(--unlocked)'}">${status.locked ? 'ZAKLENJENO' : lockStateLabel(status.lockState)}</div></div>
      <div class="card"><h3>Pokrov</h3><div class="value small">${esc(lidLabel(status.lid))}</div></div>
      <div class="card"><h3>WiFi</h3><div class="value small">${esc(wifiLabel(status.wifi))}<br><span class="muted">${esc(status.ip)}</span></div></div>
      <div class="card"><h3>Čas delovanja</h3><div class="value small">${Math.floor(status.uptimeSec/3600)} h ${Math.floor((status.uptimeSec%3600)/60)} min</div></div>
      ${status.batteryPercent !== undefined ? `<div class="card"><h3>Baterija</h3><div class="value small">${status.batteryPercent}% ${status.charging ? '(polnjenje)' : ''}</div></div>` : ''}
      <div class="card"><h3>Zadnji dostop</h3><div class="value small">${status.lastAccess ? esc(status.lastAccess.user || '-') + '<br><span class="muted">' + fmtTs(status.lastAccess.ts) + '</span>' : 'še ni dostopa'}</div></div>
    </div>
    <div class="grid">
      <div class="card"><h3>Odklepov danes</h3><div class="value">${stats.unlocksToday}</div></div>
      <div class="card"><h3>Odklepov ta teden</h3><div class="value">${stats.unlocksWeek}</div></div>
      <div class="card"><h3>Zavrnjeni poskusi</h3><div class="value">${stats.deniedAttempts}</div></div>
      <div class="card"><h3>Aktivni uporabniki / kartice</h3><div class="value small">${stats.activeUsers} / ${stats.activeCards}</div></div>
    </div>
    <div class="actions">
      <button class="btn success" id="dashUnlock">ODKLENI</button>
      <button class="btn danger" id="dashLock">ZAKLENI</button>
    </div>`;
  document.getElementById('dashUnlock').onclick = async () => { await api.post('/api/lock/unlock'); toast('Odklep zahtevan', 'ok'); refreshTopbar(); };
  document.getElementById('dashLock').onclick = async () => { await api.post('/api/lock/lock'); toast('Zaklep zahtevan', 'ok'); refreshTopbar(); };
};

function lockStateLabel(s) {
  return { LOCKED: 'ZAKLENJENO', UNLOCKED: 'ODKLENJENO', MOVING: 'V PREMIKU', ERROR: 'NAPAKA', UNKNOWN: 'NEZNANO' }[s] || s;
}

views.lock = async (view) => {
  const s = await api.get('/api/status');
  view.innerHTML = `
    <h1>Ključavnica</h1>
    <div class="card" style="max-width:420px">
      <h3>Trenutno stanje</h3>
      <div class="value" style="color:${s.locked ? 'var(--locked)' : 'var(--unlocked)'}">${esc(lockStateLabel(s.lockState))}</div>
      <div class="actions">
        <button class="btn success" id="unlockBtn">ODKLENI</button>
        <button class="btn danger" id="lockBtn">ZAKLENI</button>
      </div>
    </div>`;
  document.getElementById('unlockBtn').onclick = async () => { await api.post('/api/lock/unlock'); toast('V redu', 'ok'); router(); };
  document.getElementById('lockBtn').onclick = async () => { await api.post('/api/lock/lock'); toast('V redu', 'ok'); router(); };
};

views.users = async (view) => {
  const data = await api.get('/api/users');
  const rows = data.users.map(u => `
    <tr>
      <td>${esc(u.username)}</td><td>${esc(u.firstName)} ${esc(u.lastName)}</td>
      <td>${esc(u.role)}</td><td>${u.active ? '<span class="badge ok">aktiven</span>' : '<span class="badge neutral">onemogočen</span>'}</td>
      <td>${fmtTs(u.lastAccessEpoch)}</td><td>${u.accessCount}</td>
      <td><button class="btn danger" data-del="${esc(u.id)}">Izbriši</button></td>
    </tr>`).join('');
  view.innerHTML = `
    <h1>Uporabniki</h1>
    <div class="card" style="margin-bottom:20px">
      <h3>Dodaj uporabnika</h3>
      <div class="form-row"><label>Ime</label><input id="uFirst"></div>
      <div class="form-row"><label>Priimek</label><input id="uLast"></div>
      <div class="form-row"><label>Uporabniško ime</label><input id="uUsername"></div>
      <div class="form-row"><label>Vloga</label>
        <select id="uRole"><option>ADMIN</option><option>MANAGER</option><option selected>USER</option><option>GUEST</option></select>
      </div>
      <button class="btn primary" id="uAdd" style="width:auto">Dodaj</button>
    </div>
    <table><thead><tr><th>Uporabniško ime</th><th>Ime</th><th>Vloga</th><th>Status</th><th>Zadnji dostop</th><th>Št. dostopov</th><th></th></tr></thead>
    <tbody>${rows || '<tr><td colspan=7 class="muted">Še ni uporabnikov</td></tr>'}</tbody></table>`;
  document.getElementById('uAdd').onclick = async () => {
    try {
      await api.post('/api/users', {
        firstName: document.getElementById('uFirst').value, lastName: document.getElementById('uLast').value,
        username: document.getElementById('uUsername').value, role: document.getElementById('uRole').value,
      });
      toast('Uporabnik dodan', 'ok'); router();
    } catch (e) { toast(e.message, 'error'); }
  };
  view.querySelectorAll('[data-del]').forEach(btn => btn.onclick = async () => {
    await api.del('/api/users/' + btn.dataset.del); toast('Izbrisano', 'ok'); router();
  });
};

views.rfid = async (view) => {
  const [cards, users] = await Promise.all([api.get('/api/rfid/cards'), api.get('/api/users')]);
  const userName = (id) => { const u = users.users.find(x => x.id === id); return u ? u.username : '(ni dodeljeno)'; };
  const rows = cards.cards.map(c => `
    <tr>
      <td>${esc(c.uid)}</td><td>${esc(c.label) || '-'}</td><td>${esc(userName(c.userId))}</td>
      <td>${c.enabled ? '<span class="badge ok">omogočena</span>' : '<span class="badge neutral">onemogočena</span>'}</td>
      <td>${c.useCount}</td><td>${fmtTs(c.lastUsedEpoch)}</td>
      <td>
        <button class="btn" data-toggle="${esc(c.uid)}" data-en="${c.enabled ? 0 : 1}">${c.enabled ? 'Onemogoči' : 'Omogoči'}</button>
        <button class="btn danger" data-del="${esc(c.uid)}">Izbriši</button>
      </td>
    </tr>`).join('');
  view.innerHTML = `
    <h1>RFID kartice</h1>
    <div class="card" style="margin-bottom:20px;max-width:480px">
      <h3>Dodaj kartico</h3>
      <button class="btn primary" id="scanBtn" style="width:auto">PRIBLIŽAJ KARTICO</button>
      <div id="scanStatus" class="muted" style="margin-top:10px"></div>
      <div id="assignRow" class="hidden">
        <div class="form-row"><label>Dodeli uporabniku</label>
          <select id="assignUser">${users.users.map(u => `<option value="${esc(u.id)}">${esc(u.username)}</option>`).join('')}</select>
        </div>
        <div class="form-row"><label>Oznaka</label><input id="assignLabel"></div>
        <button class="btn success" id="assignSave" style="width:auto">Shrani kartico</button>
      </div>
    </div>
    <table><thead><tr><th>UID</th><th>Oznaka</th><th>Uporabnik</th><th>Status</th><th>Uporab</th><th>Zadnja uporaba</th><th></th></tr></thead>
    <tbody>${rows || '<tr><td colspan=7 class="muted">Še ni kartic</td></tr>'}</tbody></table>`;

  let pendingUid = null;
  document.getElementById('scanBtn').onclick = async () => {
    document.getElementById('scanStatus').textContent = 'PRIBLIŽAJ KARTICO — približajte kartico bralniku...';
    await api.post('/api/rfid/cards/capture');
    const poll = setInterval(async () => {
      const r = await api.get('/api/rfid/cards/capture');
      if (r.done) {
        clearInterval(poll);
        if (r.exists) { document.getElementById('scanStatus').textContent = 'Kartica je že znana: ' + r.uid; return; }
        pendingUid = r.uid;
        document.getElementById('scanStatus').textContent = 'Zaznana kartica: ' + r.uid;
        document.getElementById('assignRow').classList.remove('hidden');
      }
    }, 1000);
    setTimeout(() => clearInterval(poll), 20000);
  };
  document.getElementById('assignSave')?.addEventListener('click', async () => {
    await api.post('/api/rfid/cards', {
      uid: pendingUid, userId: document.getElementById('assignUser').value, label: document.getElementById('assignLabel').value,
    });
    toast('KARTICA DODANA', 'ok'); router();
  });
  view.querySelectorAll('[data-toggle]').forEach(btn => btn.onclick = async () => {
    await api.patch('/api/rfid/cards/' + btn.dataset.toggle, { enabled: btn.dataset.en === '1' }); router();
  });
  view.querySelectorAll('[data-del]').forEach(btn => btn.onclick = async () => {
    await api.del('/api/rfid/cards/' + btn.dataset.del); toast('Izbrisano', 'ok'); router();
  });
};

views.events = async (view) => {
  const data = await api.get('/api/events?limit=100');
  const rows = data.events.slice().reverse().map(e => `
    <tr><td>${fmtTs(e.ts)}</td><td>${esc(e.type)}</td><td>${esc(e.source)}</td>
        <td>${esc(e.user || e.rfidUid || '-')}</td><td>${esc(e.result)}</td><td>${esc(e.reason)}</td></tr>`).join('');
  view.innerHTML = `<h1>Dogodki</h1>
    <table><thead><tr><th>Čas</th><th>Vrsta</th><th>Vir</th><th>Kdo</th><th>Rezultat</th><th>Razlog</th></tr></thead>
    <tbody>${rows || '<tr><td colspan=6 class="muted">Še ni dogodkov</td></tr>'}</tbody></table>`;
};

views.statistics = async (view) => {
  const s = await api.get('/api/statistics');
  view.innerHTML = `<h1>Statistika</h1>
    <div class="grid">
      <div class="card"><h3>Odklepov danes</h3><div class="value">${s.unlocksToday}</div></div>
      <div class="card"><h3>Odklepov ta teden</h3><div class="value">${s.unlocksWeek}</div></div>
      <div class="card"><h3>Odklepov ta mesec</h3><div class="value">${s.unlocksMonth}</div></div>
      <div class="card"><h3>Zavrnjeni poskusi</h3><div class="value">${s.deniedAttempts}</div></div>
      <div class="card"><h3>Napake servo motorjev</h3><div class="value">${s.servoErrors}</div></div>
      <div class="card"><h3>Prekinitve WiFi</h3><div class="value">${s.wifiDisconnects}</div></div>
      <div class="card"><h3>Sistemske napake</h3><div class="value">${s.systemErrors}</div></div>
      <div class="card"><h3>Najpogosteje uporabljena kartica</h3><div class="value small">${esc(s.mostUsedCard || '-')}</div></div>
    </div>
    <div class="card"><h3>Odklepi (danes / teden / mesec)</h3><canvas id="chart" class="chart"></canvas></div>`;
  drawBarChart(document.getElementById('chart'), [
    { label: 'Danes', value: s.unlocksToday }, { label: 'Teden', value: s.unlocksWeek }, { label: 'Mesec', value: s.unlocksMonth },
  ]);
};

function drawBarChart(canvas, points) {
  const dpr = window.devicePixelRatio || 1;
  const w = canvas.clientWidth || 400, h = canvas.clientHeight || 160;
  canvas.width = w * dpr; canvas.height = h * dpr;
  const ctx = canvas.getContext('2d'); ctx.scale(dpr, dpr);
  const max = Math.max(1, ...points.map(p => p.value));
  const barW = w / (points.length * 2);
  ctx.font = '12px sans-serif';
  points.forEach((p, i) => {
    const x = i * (w / points.length) + (w / points.length - barW) / 2;
    const barH = (p.value / max) * (h - 30);
    ctx.fillStyle = getComputedStyle(document.documentElement).getPropertyValue('--primary') || '#2563eb';
    ctx.fillRect(x, h - 20 - barH, barW, barH);
    ctx.fillStyle = '#8890a3';
    ctx.fillText(p.label, x, h - 4);
    ctx.fillText(String(p.value), x, h - 24 - barH);
  });
}

views.network = async (view) => {
  const n = await api.get('/api/network');
  view.innerHTML = `<h1>Omrežje</h1>
    <div class="card" style="max-width:480px">
      <div class="form-row"><label>Način</label><input value="${esc(wifiLabel(n.mode))}" disabled></div>
      <div class="form-row"><label>IP naslov</label><input value="${esc(n.ip)}" disabled></div>
      <div class="form-row"><label>MAC naslov</label><input value="${esc(n.mac)}" disabled></div>
      <div class="form-row"><label>Ime gostitelja (hostname)</label><input id="nHostname" value="${esc(n.hostname)}"></div>
      <div class="form-row"><label>Ime naprave</label><input id="nDeviceName" value="${esc(n.deviceName)}"></div>
      <div class="form-row"><label>SSID (ime omrežja)</label><input id="nSsid" value="${esc(n.ssid)}"></div>
      <div class="form-row"><label>Geslo (pustite prazno, če ne spreminjate)</label><input id="nPass" type="password"></div>
      <div class="form-row"><label>Časovni pas</label><input id="nTz" value="${esc(n.timezone)}"></div>
      <button class="btn primary" id="nSave" style="width:auto">Shrani (za uveljavitev WiFi sprememb se naprava ponovno zažene)</button>
    </div>`;
  document.getElementById('nSave').onclick = async () => {
    await api.post('/api/network/setup', {
      ssid: document.getElementById('nSsid').value, password: document.getElementById('nPass').value,
      hostname: document.getElementById('nHostname').value, deviceName: document.getElementById('nDeviceName').value,
      timezone: document.getElementById('nTz').value,
    });
    toast('Shranjeno — ponovno povezovanje...', 'ok');
  };
};

views.security = async (view) => {
  const s = await api.get('/api/security');
  view.innerHTML = `<h1>Varnost</h1>
    <div class="card" style="max-width:480px">
      <div class="form-row"><label>Časovna omejitev spletne seje (sekunde)</label><input id="sTimeout" type="number" value="${s.sessionTimeoutSec}"></div>
      <div class="form-row"><label>Največje število neuspešnih poskusov pred opozorilom</label><input id="sMaxFail" type="number" value="${s.maxFailedAttempts}"></div>
      <button class="btn primary" id="sSave" style="width:auto">Shrani</button>
    </div>
    <p class="muted" style="margin-top:16px">Enkratna varnostna koda: na napravi držite GUMB 1, ko je ta spletna seja zaklenjena, nato na prijavnem zaslonu izberite "Uporabi varnostno kodo".</p>`;
  document.getElementById('sSave').onclick = async () => {
    await api.patch('/api/security', { sessionTimeoutSec: +document.getElementById('sTimeout').value, maxFailedAttempts: +document.getElementById('sMaxFail').value });
    toast('Shranjeno', 'ok');
  };
};

views.display = async (view) => {
  const d = await api.get('/api/display');
  view.innerHTML = `<h1>Zaslon</h1>
    <div class="card" style="max-width:480px">
      <div class="form-row"><label>Svetlost (%)</label><input id="dBright" type="number" min="0" max="100" value="${d.brightnessPercent}"></div>
      <div class="form-row"><label>Časovna zakasnitev zaslona (sekunde, 0 = brez)</label><input id="dTimeout" type="number" value="${d.screenTimeoutSec}"></div>
      <div class="form-row"><label>Vrtenje (stopinje)</label>
        <select id="dRot"><option value="0">0</option><option value="90">90</option><option value="180">180</option><option value="270">270</option></select>
      </div>
      <div class="form-row"><label><input type="checkbox" id="dAnim" ${d.animationsEnabled ? 'checked' : ''}> Animacije omogočene</label></div>
      <button class="btn primary" id="dSave" style="width:auto">Shrani</button>
    </div>`;
  document.getElementById('dRot').value = String(d.rotationDeg);
  document.getElementById('dSave').onclick = async () => {
    await api.patch('/api/display', {
      brightnessPercent: +document.getElementById('dBright').value, screenTimeoutSec: +document.getElementById('dTimeout').value,
      rotationDeg: +document.getElementById('dRot').value, animationsEnabled: document.getElementById('dAnim').checked,
    });
    toast('Shranjeno', 'ok');
  };
};

views.audio = async (view) => {
  const a = await api.get('/api/audio');
  const toggle = (id, label, checked) => `<div class="form-row"><label><input type="checkbox" id="${id}" ${checked ? 'checked' : ''}> ${label}</label></div>`;
  view.innerHTML = `<h1>Zvok</h1>
    <div class="card" style="max-width:480px">
      ${toggle('aEnabled', 'Zvočnik omogočen', a.enabled)}
      <div class="form-row"><label>Glasnost (%)</label><input id="aVol" type="number" min="0" max="100" value="${a.volumePercent}"></div>
      ${toggle('aSuccess', 'Zvok ob uspešnem RFID', a.soundSuccess)}
      ${toggle('aDenied', 'Zvok ob zavrnjenem RFID', a.soundDenied)}
      ${toggle('aLock', 'Zvok ob zaklepu', a.soundLock)}
      ${toggle('aUnlock', 'Zvok ob odklepu', a.soundUnlock)}
      ${toggle('aWarning', 'Opozorilni zvok', a.soundWarning)}
      ${toggle('aStartup', 'Zvok ob zagonu', a.soundStartup)}
      ${toggle('aError', 'Zvok ob napaki', a.soundError)}
      <button class="btn primary" id="aSave" style="width:auto">Shrani</button>
    </div>`;
  document.getElementById('aSave').onclick = async () => {
    await api.patch('/api/audio', {
      enabled: document.getElementById('aEnabled').checked, volumePercent: +document.getElementById('aVol').value,
      soundSuccess: document.getElementById('aSuccess').checked, soundDenied: document.getElementById('aDenied').checked,
      soundLock: document.getElementById('aLock').checked, soundUnlock: document.getElementById('aUnlock').checked,
      soundWarning: document.getElementById('aWarning').checked, soundStartup: document.getElementById('aStartup').checked,
      soundError: document.getElementById('aError').checked,
    });
    toast('Shranjeno', 'ok');
  };
};

const ACTIONS = ['SHOW_STATUS','SHOW_IP','SHOW_WIFI','GENERATE_SECURITY_CODE','UNLOCK','LOCK','OPEN_MENU',
                  'SHOW_DEVICE_INFO','REBOOT','CAMERA','CUSTOM_ACTION','WIFI_SETUP','SERVICE_MODE','DIAGNOSTICS','OPEN_QUICK_MENU'];
function actionSelect(id, current) {
  return `<select id="${id}">${ACTIONS.map(a => `<option ${a === current ? 'selected' : ''}>${a}</option>`).join('')}</select>`;
}

views.buttons = async (view) => {
  const b = await api.get('/api/buttons');
  view.innerHTML = `<h1>Tipke</h1>
    <div class="grid">
      <div class="card"><h3>Gumb 1</h3>
        <div class="form-row"><label>Kratek pritisk</label>${actionSelect('b1s', b.button1.shortAction)}</div>
        <div class="form-row"><label>Dolg pritisk</label>${actionSelect('b1l', b.button1.longAction)}</div>
        <div class="form-row"><label>Dvoklik</label>${actionSelect('b1d', b.button1.doubleAction)}</div>
        <div class="form-row"><label>Trajanje dolgega pritiska (ms)</label><input id="b1ms" type="number" value="${b.button1.longPressMs}"></div>
      </div>
      <div class="card"><h3>Gumb 2</h3>
        <div class="form-row"><label>Kratek pritisk</label>${actionSelect('b2s', b.button2.shortAction)}</div>
        <div class="form-row"><label>Dolg pritisk</label>${actionSelect('b2l', b.button2.longAction)}</div>
        <div class="form-row"><label>Dvoklik</label>${actionSelect('b2d', b.button2.doubleAction)}</div>
        <div class="form-row"><label>Trajanje dolgega pritiska (ms)</label><input id="b2ms" type="number" value="${b.button2.longPressMs}"></div>
      </div>
    </div>
    <button class="btn primary" id="bSave" style="width:auto">Shrani</button>`;
  document.getElementById('bSave').onclick = async () => {
    await api.patch('/api/buttons', {
      button1: { shortAction: document.getElementById('b1s').value, longAction: document.getElementById('b1l').value,
                 doubleAction: document.getElementById('b1d').value, longPressMs: +document.getElementById('b1ms').value },
      button2: { shortAction: document.getElementById('b2s').value, longAction: document.getElementById('b2l').value,
                 doubleAction: document.getElementById('b2d').value, longPressMs: +document.getElementById('b2ms').value },
    });
    toast('Shranjeno', 'ok');
  };
};

views.lid = async (view) => {
  const l = await api.get('/api/lid');
  view.innerHTML = `<h1>Senzor pokrova</h1>
    <div class="card" style="max-width:480px">
      <p>Trenutno stanje: <span class="badge ${l.state === 'CLOSED' ? 'ok' : 'warn'}">${esc(lidLabel(l.state))}</span></p>
      <div class="form-row"><label><input type="checkbox" id="lEnabled" ${l.enabled ? 'checked' : ''}> Omogočeno</label></div>
      <div class="form-row"><label>GPIO</label><input id="lGpio" type="number" value="${l.gpio}"></div>
      <div class="form-row"><label><input type="checkbox" id="lInv" ${l.inverted ? 'checked' : ''}> Obrnjena logika</label></div>
      <div class="form-row"><label>Debounce (ms)</label><input id="lDeb" type="number" value="${l.debounceMs}"></div>
      <div class="form-row"><label><input type="checkbox" id="lAuto" ${l.autoLock ? 'checked' : ''}> Samodejni zaklep ob zaprtju</label></div>
      <div class="form-row"><label>Zakasnitev samodejnega zaklepa (0-30 s)</label><input id="lDelay" type="number" min="0" max="30" value="${l.autoLockDelaySec}"></div>
      <button class="btn primary" id="lSave" style="width:auto">Shrani</button>
    </div>`;
  document.getElementById('lSave').onclick = async () => {
    await api.patch('/api/lid', {
      enabled: document.getElementById('lEnabled').checked, gpio: +document.getElementById('lGpio').value,
      inverted: document.getElementById('lInv').checked, debounceMs: +document.getElementById('lDeb').value,
      autoLock: document.getElementById('lAuto').checked, autoLockDelaySec: +document.getElementById('lDelay').value,
    });
    toast('Shranjeno', 'ok');
  };
};

views.servos = async (view) => {
  const s = await api.get('/api/servos');
  const servoCard = (n, cfg) => `
    <div class="card"><h3>Servo motor ${n}</h3>
      <div class="form-row"><label><input type="checkbox" id="s${n}en" ${cfg.enabled ? 'checked' : ''}> Omogočen</label></div>
      <div class="form-row"><label>GPIO</label><input id="s${n}gpio" type="number" value="${cfg.gpio}"></div>
      <div class="form-row"><label>Kot zaklenjeno</label><input id="s${n}lock" type="number" min="0" max="180" value="${cfg.lockedAngle}"></div>
      <div class="form-row"><label>Kot odklenjeno</label><input id="s${n}unlock" type="number" min="0" max="180" value="${cfg.unlockedAngle}"></div>
      <div class="form-row"><label>Hitrost (stopinj/s)</label><input id="s${n}speed" type="number" value="${cfg.speedDegPerSec}"></div>
      <div class="form-row"><label>Zakasnitev (ms)</label><input id="s${n}delay" type="number" value="${cfg.delayMs}"></div>
      <div class="form-row"><label><input type="checkbox" id="s${n}inv" ${cfg.invert ? 'checked' : ''}> Obratna smer</label></div>
    </div>`;
  view.innerHTML = `<h1>Servo motorji</h1>
    <div class="grid">${servoCard(1, s.servo1)}${servoCard(2, s.servo2)}</div>
    <div class="actions">
      <button class="btn primary" id="svSave" style="width:auto">Shrani kalibracijo</button>
      <button class="btn success" id="svTestUnlock">Testiraj ODKLEP</button>
      <button class="btn danger" id="svTestLock">Testiraj ZAKLEP</button>
    </div>`;
  const readServo = (n) => ({
    enabled: document.getElementById(`s${n}en`).checked, gpio: +document.getElementById(`s${n}gpio`).value,
    lockedAngle: +document.getElementById(`s${n}lock`).value, unlockedAngle: +document.getElementById(`s${n}unlock`).value,
    speedDegPerSec: +document.getElementById(`s${n}speed`).value, delayMs: +document.getElementById(`s${n}delay`).value,
    invert: document.getElementById(`s${n}inv`).checked,
  });
  document.getElementById('svSave').onclick = async () => {
    await api.patch('/api/servos', { servo1: readServo(1), servo2: readServo(2) });
    toast('Shranjeno', 'ok');
  };
  document.getElementById('svTestUnlock').onclick = async () => { await api.post('/api/servos/test', { action: 'unlock' }); toast('Testiram odklep', 'ok'); };
  document.getElementById('svTestLock').onclick = async () => { await api.post('/api/servos/test', { action: 'lock' }); toast('Testiram zaklep', 'ok'); };
};

views.automation = async (view) => {
  const data = await api.get('/api/automation/rules');
  const kindLabel = (k) => ['ČE DOGODEK', 'ČE URA', 'ČE N ZAVRNITEV'][k] || k;
  const actionLabel = (a) => ['ODKLENI', 'ZAKLENI', 'OPOZORILO', 'SINHRONIZIRAJ', 'OBVESTI'][a] || a;
  const rows = data.rules.map(r => `
    <tr><td>${esc(r.label)}</td><td>${kindLabel(r.conditionKind)}</td><td>POTEM ${actionLabel(r.action)}</td>
        <td><label><input type="checkbox" data-toggle="${esc(r.id)}" ${r.enabled ? 'checked' : ''}></label></td></tr>`).join('');
  view.innerHTML = `<h1>Avtomatizacija</h1>
    <p class="muted">Sistem pravil ČE / POTEM. Nova pravila lahko trenutno dodate prek uvoza varnostne kopije (JSON) — urejevalnik pravil sledi kot naslednji korak.</p>
    <table><thead><tr><th>Pravilo</th><th>Pogoj</th><th>Dejanje</th><th>Omogočeno</th></tr></thead>
    <tbody>${rows || '<tr><td colspan=4 class="muted">Ni pravil</td></tr>'}</tbody></table>`;
  view.querySelectorAll('[data-toggle]').forEach(cb => cb.onchange = async () => {
    await api.patch('/api/automation/rules/' + cb.dataset.toggle + '/enabled', { enabled: cb.checked });
    toast('Shranjeno', 'ok');
  });
};

views.backup = async (view) => {
  view.innerHTML = `<h1>Varnostna kopija</h1>
    <div class="card" style="max-width:480px">
      <h3>Izvoz</h3>
      <p class="muted">Gesla nikoli niso vključena v izvoz.</p>
      <button class="btn primary" id="bkExport" style="width:auto">Prenesi varnostno kopijo</button>
    </div>
    <div class="card" style="max-width:480px;margin-top:16px">
      <h3>Uvoz</h3>
      <input type="file" id="bkFile" accept="application/json">
      <button class="btn primary" id="bkImport" style="width:auto;margin-top:10px">Obnovi iz datoteke</button>
    </div>`;
  document.getElementById('bkExport').onclick = async () => {
    const data = await api.get('/api/backup/export');
    const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob); a.download = 'smartbox-varnostna-kopija-' + Date.now() + '.json'; a.click();
  };
  document.getElementById('bkImport').onclick = async () => {
    const file = document.getElementById('bkFile').files[0];
    if (!file) { toast('Najprej izberite datoteko', 'error'); return; }
    const text = await file.text();
    await api.post('/api/backup/import', JSON.parse(text));
    toast('Obnovljeno — za uveljavitev ponovno zaženite napravo', 'ok');
  };
};

views.diagnostics = async (view) => {
  const data = await api.get('/api/diagnostics');
  const badge = (s) => s === 'OK' ? 'ok' : s === 'WARN' ? 'warn' : s === 'FAIL' ? 'fail' : 'neutral';
  const statusLabel = (s) => ({ OK: 'V REDU', WARN: 'OPOZORILO', FAIL: 'NAPAKA', NOT_PRESENT: 'NI PRISOTNO' }[s] || s);
  const rows = data.results.map(r => `<tr><td>${esc(r.component)}</td><td><span class="badge ${badge(r.status)}">${esc(statusLabel(r.status))}</span></td><td>${esc(r.detail)}</td></tr>`).join('');
  view.innerHTML = `<h1>Diagnostika</h1>
    <table><thead><tr><th>Komponenta</th><th>Status</th><th>Podrobnosti</th></tr></thead><tbody>${rows}</tbody></table>`;
};

views.firmware = async (view) => {
  const [fw, dev] = await Promise.all([api.get('/api/firmware/check'), api.get('/api/device')]);
  view.innerHTML = `<h1>Firmware</h1>
    <div class="card" style="max-width:480px">
      <div class="form-row"><label>Trenutna različica</label><input value="${esc(dev.firmware)}" disabled></div>
      <div class="form-row"><label>URL za posodobitev / firmware datoteko</label><input id="fwUrl" placeholder="https://.../firmware.bin"></div>
      <div class="form-row"><label>SHA-256 (neobvezno, a priporočeno)</label><input id="fwSha"></div>
      <div class="actions">
        <button class="btn primary" id="fwUpdate">POSODOBI</button>
        <button class="btn" id="fwReboot">PONOVNI ZAGON</button>
      </div>
    </div>`;
  document.getElementById('fwUpdate').onclick = async () => {
    if (!confirm('Namestim to firmware različico zdaj? Naprava se bo ponovno zagnala.')) return;
    await api.post('/api/firmware/update', { url: document.getElementById('fwUrl').value, sha256: document.getElementById('fwSha').value });
    toast('Posodobitev se je začela — naprava se bo ponovno zagnala', 'ok');
  };
  document.getElementById('fwReboot').onclick = async () => {
    if (!confirm('Ponovno zaženem napravo zdaj?')) return;
    await api.post('/api/system/reboot'); toast('Ponovni zagon...', 'ok');
  };
};

views.appearance = async (view) => {
  const a = await api.get('/api/appearance');
  const colorRow = (id, label, val) => `<div class="form-row"><label>${label}</label><input id="${id}" type="color" value="${val}"></div>`;
  view.innerHTML = `<h1>Izgled</h1>
    <div class="grid">
      <div class="card">
        <h3>Tema</h3>
        <div class="form-row"><label>Način</label>
          <select id="apTheme"><option value="light" ${a.theme==='light'?'selected':''}>Svetla</option><option value="dark" ${a.theme==='dark'?'selected':''}>Temna</option>
            <option value="auto" ${a.theme==='auto'?'selected':''}>Samodejno</option><option value="custom" ${a.theme==='custom'?'selected':''}>Po meri</option></select>
        </div>
        <div class="form-row"><label>Zaokroženost robov (px)</label><input id="apRadius" type="number" value="${a.borderRadius}"></div>
        <div class="form-row"><label><input type="checkbox" id="apAnim" ${a.animationsEnabled?'checked':''}> Animacije</label></div>
      </div>
      <div class="card">
        <h3>Barve</h3>
        ${colorRow('cPrimary','Osnovna',a.colorPrimary)}${colorRow('cSecondary','Sekundarna',a.colorSecondary)}
        ${colorRow('cBg','Ozadje',a.colorBackground)}${colorRow('cCard','Kartica',a.colorCard)}
        ${colorRow('cText','Besedilo',a.colorText)}${colorRow('cSuccess','Uspeh',a.colorSuccess)}
        ${colorRow('cWarn','Opozorilo',a.colorWarning)}${colorRow('cDanger','Nevarnost',a.colorDanger)}
      </div>
    </div>
    <button class="btn primary" id="apSave" style="width:auto">Shrani in uveljavi</button>`;
  document.getElementById('apSave').onclick = async () => {
    const payload = {
      theme: document.getElementById('apTheme').value, borderRadius: +document.getElementById('apRadius').value,
      animationsEnabled: document.getElementById('apAnim').checked,
      colorPrimary: document.getElementById('cPrimary').value, colorSecondary: document.getElementById('cSecondary').value,
      colorBackground: document.getElementById('cBg').value, colorCard: document.getElementById('cCard').value,
      colorText: document.getElementById('cText').value, colorSuccess: document.getElementById('cSuccess').value,
      colorWarning: document.getElementById('cWarn').value, colorDanger: document.getElementById('cDanger').value,
    };
    await api.patch('/api/appearance', payload);
    applyTheme(payload);
    toast('Uveljavljeno', 'ok');
  };
};

views.system = async (view) => {
  view.innerHTML = `<h1>Sistem</h1>
    <div class="card" style="max-width:480px">
      <h3>Ponovni zagon</h3>
      <button class="btn" id="sysReboot">PONOVNI ZAGON</button>
    </div>
    <div class="card" style="max-width:480px;margin-top:16px">
      <h3 style="color:var(--danger)">Ponastavitev na tovarniške nastavitve</h3>
      <p class="muted">Izbriše vse uporabnike, kartice in nastavitve. Tega dejanja ni mogoče razveljaviti.</p>
      <label><input type="checkbox" id="sysConfirm"> Razumem, da bo to izbrisalo vse</label>
      <div class="actions"><button class="btn danger" id="sysReset">TOVARNIŠKA PONASTAVITEV</button></div>
    </div>`;
  document.getElementById('sysReboot').onclick = async () => { if (confirm('Ponovno zaženem napravo zdaj?')) { await api.post('/api/system/reboot'); toast('Ponovni zagon...', 'ok'); } };
  document.getElementById('sysReset').onclick = async () => {
    if (!document.getElementById('sysConfirm').checked) { toast('Najprej potrdite s kljukico', 'error'); return; }
    if (!confirm('Tega dejanja ni mogoče razveljaviti. Nadaljujem?')) return;
    await api.post('/api/system/factory-reset', { confirm: true });
    toast('Ponastavljam napravo...', 'ok');
  };
};

views.about = async (view) => {
  const d = await api.get('/api/device');
  view.innerHTML = `<h1>O napravi</h1>
    <div class="card" style="max-width:480px">
      <table>
        <tr><td class="muted">Model</td><td>${esc(d.model)}</td></tr>
        <tr><td class="muted">MCU</td><td>${esc(d.mcu)}</td></tr>
        <tr><td class="muted">Firmware</td><td>${esc(d.firmware)}</td></tr>
        <tr><td class="muted">Gradnja (build)</td><td>${esc(d.buildTime)} (${esc(d.gitHash)})</td></tr>
        <tr><td class="muted">Flash</td><td>${d.flashMb} MB</td></tr>
        <tr><td class="muted">PSRAM</td><td>${d.psramMb} MB</td></tr>
        <tr><td class="muted">WiFi</td><td>${esc(wifiLabel(d.wifi))}</td></tr>
        <tr><td class="muted">IP</td><td>${esc(d.ip)}</td></tr>
        <tr><td class="muted">MAC</td><td>${esc(d.mac)}</td></tr>
        <tr><td class="muted">Čas delovanja</td><td>${d.uptimeSec} s</td></tr>
      </table>
    </div>`;
};

// ---------------------------------------------------------------------------
wireGate();
boot();
