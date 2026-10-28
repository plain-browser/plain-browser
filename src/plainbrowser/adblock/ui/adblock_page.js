// Copyright Plain Browser contributors. MPL-2.0.
// chrome://adblock: the state of the ad blocker, from AdblockPageHandler
// (adblock_ui.cc) through chrome.send. No innerHTML (the page runs under
// Trusted Types): every text goes through textContent.
import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import './strings.m.js';

const $ = (id) => document.getElementById(id);
const t = (id) => loadTimeData.getString(id);

function formatTime(ms) {
  return new Intl.DateTimeFormat(t('language') || undefined,
                                 {dateStyle: 'medium', timeStyle: 'short'})
      .format(new Date(ms));
}

function setState(el, on) {
  el.textContent = t(on ? 'on' : 'off');
  el.className = 'value ' + (on ? 'on' : 'off');
}

function row(label, sub, value, valueClass) {
  const r = document.createElement('div');
  r.className = 'row';
  const l = document.createElement('div');
  l.className = 'label';
  l.textContent = label;
  if (sub) {
    const s = document.createElement('div');
    s.className = 'sub';
    s.textContent = sub;
    l.appendChild(s);
  }
  const v = document.createElement('div');
  v.className = 'value ' + (valueClass || '');
  v.textContent = value;
  r.append(l, v);
  return r;
}

function render(s) {
  setState($('blocking'), s.blockingEnabled);
  $('blocked').textContent = s.blockedTotal.toLocaleString();

  const engine = $('engine');
  if (s.engineGaveUp) {
    engine.textContent = t('engineStopped');
    engine.className = 'value warn';
  } else {
    engine.textContent = t(s.engineRunning ? 'engineRunning' : 'engineIdle');
    engine.className = 'value' + (s.engineRunning ? ' on' : '');
  }
  $('engineStarts').textContent =
      loadTimeData.getStringF('engineStarts', s.engineStarts);

  const lists = $('lists');
  lists.replaceChildren(...s.lists.map((l) => {
    const sub = l.downloaded ?
        t('listDownloaded') + (l.sha256 ? ' · ' + l.sha256.slice(0, 12) : '') :
        t('listBundled');
    const size = l.size ? Math.round(l.size / 1024).toLocaleString() + ' KB' : '';
    return row(l.name, sub, size, 'mono');
  }));

  const updates = $('updates');
  const lines = [];
  if (!s.updatesEnabled) {
    lines.push(t('updatesOff'));
  } else {
    lines.push(s.lastSuccess ?
                   loadTimeData.getStringF('lastCheck', formatTime(s.lastSuccess)) :
                   t('neverChecked'));
    if (s.lastCheck > s.lastSuccess + 1000) {
      lines.push(t('lastFailed'));
    }
  }
  updates.textContent = lines.join('\n');
  updates.style.whiteSpace = 'pre-line';
  const check = $('checkNow');
  check.disabled = !s.updatesEnabled || s.checking;
  check.textContent = t(s.checking ? 'checking' : 'checkNow');
}

async function refresh() {
  render(await sendWithPromise('getStatus'));
}

function init() {
  document.documentElement.lang = t('language');
  document.title = t('title');
  for (const [id, key] of [['title', 'title'], ['blockingLabel', 'blocking'],
                           ['blockedLabel', 'blocked'], ['engineLabel', 'engine'],
                           ['listsLabel', 'lists'], ['updatesLabel', 'updates'],
                           ['settings', 'settings']]) {
    $(id).textContent = t(key);
  }
  $('checkNow').addEventListener('click', async () => {
    await sendWithPromise('checkNow');
    refresh();
  });
  refresh();
  // The counter and a running check change while the page is open.
  setInterval(refresh, 2000);
}

init();
