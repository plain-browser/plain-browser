// Copyright Plain Browser contributors. MPL-2.0.
// chrome://recommended-extensions: the curated extensions and their state,
// from RecommendedExtensionsHandler (recommended_extensions_ui.cc) through
// chrome.send. No innerHTML (the page runs under Trusted Types): every text
// goes through textContent.
import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import './strings.m.js';

const $ = (id) => document.getElementById(id);
const t = (id) => loadTimeData.getString(id);

// "Keep both" hides the offer until the page is opened again.
let keepBoth = false;
let timer = 0;

function el(tag, className, text) {
  const e = document.createElement(tag);
  if (className) {
    e.className = className;
  }
  if (text !== undefined) {
    e.textContent = text;
  }
  return e;
}

function button(text, primary, onClick) {
  const b = el('button', primary ? 'primary' : '', text);
  b.addEventListener('click', onClick);
  return b;
}

async function install(slug, migrate) {
  await sendWithPromise(migrate ? 'migrate' : 'install', slug);
  refresh();
}

function entry(x) {
  const r = el('div', 'ext');
  const head = el('div', 'head');
  const left = el('div');
  left.append(el('div', 'name', x.name));
  const tags = el('div', 'tags');
  if (x.manifestVersion === 3) {
    tags.append(el('span', 'tag', t('manifestV3')));
  }
  if (x.unmaintained) {
    tags.append(el('span', 'tag', t('unmaintained')));
  }
  if (x.version) {
    tags.append(el('span', 'tag', x.version));
  }
  if (tags.childElementCount) {
    left.append(tags);
  }
  head.append(left);

  let action;
  if (x.installing) {
    action = el('button', '', t('installing'));
    action.disabled = true;
  } else if (x.state === 'enabled' || x.state === 'disabled') {
    action = el('span', x.state === 'enabled' ? 'on' : 'off', t(x.state));
  } else if (x.state === 'store') {
    action = button(t('switch'), true, () => install(x.slug, true));
  } else {
    action = button(t('install'), true, () => install(x.slug, false));
  }
  head.append(action);
  r.append(head);

  r.append(el('div', 'desc', x.description));
  if (x.state === 'store') {
    r.append(el('div', 'desc', t('fromStore') + ' ' + t('switchDesc')));
  }
  if (x.error) {
    r.append(el('div', 'error', x.error));
  }
  const links = el('div', 'links');
  const source = el('a', '', t('source'));
  source.href = x.homepage;
  source.target = '_blank';
  links.append(source);
  r.append(links);
  return r;
}

function render(s) {
  $('list').replaceChildren(...s.extensions.map(entry));
  const showBanner = !keepBoth && s.builtInBlocking && s.activeBlocker;
  $('banner').hidden = !showBanner;
  if (showBanner) {
    $('bannerText').textContent =
        loadTimeData.getStringF('bothBlockers', s.activeBlocker);
  }
  $('intro').textContent = t('intro') +
      (s.builtInBlocking ? '' : ' ' + t('builtInOff'));
  // Poll quickly while an install runs, slowly otherwise (the user may
  // install, enable or remove extensions in another tab).
  clearTimeout(timer);
  timer = setTimeout(refresh,
                     s.extensions.some((x) => x.installing) ? 700 : 3000);
}

async function refresh() {
  render(await sendWithPromise('getState'));
}

function init() {
  document.documentElement.lang = t('language');
  document.title = t('title');
  for (const [id, key] of [['title', 'title'], ['turnOff', 'turnOff'],
                           ['keepBoth', 'keepBoth'], ['other', 'other'],
                           ['manage', 'manage']]) {
    $(id).textContent = t(key);
  }
  $('turnOff').addEventListener('click', async () => {
    await sendWithPromise('turnOffBuiltInBlocking');
    refresh();
  });
  $('keepBoth').addEventListener('click', () => {
    keepBoth = true;
    $('banner').hidden = true;
  });
  refresh();
}

init();
