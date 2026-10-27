// Renders the Microsoft Store hero (1920x1080 and 2400x1200) in EN and PL from an inline SVG.
// Needs `sharp` (npm i sharp) and Segoe UI visible to fontconfig (a fonts.conf with <dir>/mnt/c/Windows/Fonts</dir>).
// Usage: FONTCONFIG_FILE=fonts.conf node make-hero.mjs <outdir>
import sharp from "sharp";
import { mkdirSync } from "node:fs";

const OUT = process.argv[2] || ".";
mkdirSync(OUT, { recursive: true });

const T = {
  en: { h1: "Browse without", h2: "the noise.", sub: "Private by default. No Google, no tracking, no account.",
        badge: "Built on Chromium  \u00b7  Privacy first",
        chips: ["Chromium engine, modern web", "No Google services or account", "Ads and trackers blocked", "Fingerprints masked", "Free and open source"],
        blocked: "37 blocked", site: "This site", on: "Shields on" },
  pl: { h1: "Przeglądaj bez", h2: "zbędnego szumu.", sub: "Prywatność domyślnie. Bez Google, śledzenia i konta.",
        badge: "Oparta na Chromium  \u00b7  Prywatność przede wszystkim",
        chips: ["Silnik Chromium, nowoczesny internet", "Bez usług Google i konta", "Reklamy i trackery zablokowane", "Odciski przeglądarki ukryte", "Darmowa i open source"],
        blocked: "37 zablokowano", site: "Ta strona", on: "Tarcza włączona" },
};

const svg = (t, W = 1920, H = 1080) => `
<svg xmlns="http://www.w3.org/2000/svg" width="${W}" height="${H}" viewBox="0 0 1920 1080" preserveAspectRatio="xMidYMid meet" font-family="Segoe UI, DejaVu Sans, sans-serif">
  <defs>
    <linearGradient id="bg" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="#0B1B4D"/><stop offset=".55" stop-color="#1B49B8"/><stop offset="1" stop-color="#2D6CDF"/>
    </linearGradient>
    <radialGradient id="glow" cx=".78" cy=".45" r=".5"><stop offset="0" stop-color="#7FB0FF" stop-opacity=".55"/><stop offset="1" stop-color="#7FB0FF" stop-opacity="0"/></radialGradient>
    <filter id="sh" x="-20%" y="-20%" width="140%" height="150%"><feDropShadow dx="0" dy="30" stdDeviation="34" flood-color="#000A2E" flood-opacity=".5"/></filter>
    <filter id="sh2" x="-20%" y="-20%" width="140%" height="160%"><feDropShadow dx="0" dy="14" stdDeviation="16" flood-color="#000A2E" flood-opacity=".45"/></filter>
    <clipPath id="win"><rect x="0" y="0" width="980" height="640" rx="22"/></clipPath>
  </defs>
  <rect x="-400" y="-120" width="2720" height="1320" fill="url(#bg)"/>
  <rect x="-400" y="-120" width="2720" height="1320" fill="url(#glow)"/>
  <circle cx="1740" cy="140" r="360" fill="none" stroke="#fff" stroke-opacity=".06" stroke-width="2"/>
  <circle cx="1740" cy="140" r="260" fill="none" stroke="#fff" stroke-opacity=".06" stroke-width="2"/>

  <!-- logo + wordmark -->
  <g transform="translate(110 96)">
    <circle cx="36" cy="36" r="36" fill="#fff"/>
    <path fill="#2D6CDF" fill-rule="evenodd" transform="scale(.28125)" d="M84 64h42a46 46 0 0 1 0 92h-14v36H84z M128 94a16 16 0 0 0 0 32a16 16 0 0 0 0-32z"/>
    <text x="94" y="49" font-size="38" font-weight="600" fill="#fff">Plain Browser</text>
  </g>

  <!-- badge -->
  <g transform="translate(110 214)"><rect width="${Math.round(t.badge.length * 11.6 + 60)}" height="52" rx="26" fill="#fff" fill-opacity=".12" stroke="#9CC4FF" stroke-opacity=".6"/>
    <text x="28" y="35" font-size="24" font-weight="600" fill="#CFE0FF">${t.badge}</text></g>

  <!-- headline -->
  <text x="110" y="400" font-size="100" font-weight="700" fill="#fff" letter-spacing="-2">${t.h1}</text>
  <text x="110" y="510" font-size="100" font-weight="700" fill="#9CC4FF" letter-spacing="-2">${t.h2}</text>
  <text x="114" y="590" font-size="32" fill="#fff" fill-opacity=".88">${t.sub}</text>

  <!-- chips -->
  ${(() => {
    let y = 650, out = "";
    for (const c of t.chips) {
      const w = Math.round(c.length * 12.6 + 84);
      out += `<g transform="translate(110 ${y})"><rect width="${w}" height="56" rx="28" fill="#fff" fill-opacity=".14" stroke="#fff" stroke-opacity=".3"/>
        <circle cx="32" cy="28" r="11" fill="#4ADE80"/><path d="M26.5 28l4 4L37.5 24.5" fill="none" stroke="#0B1B4D" stroke-width="3.2" stroke-linecap="round" stroke-linejoin="round"/>
        <text x="56" y="37" font-size="24" font-weight="600" fill="#fff">${c}</text></g>`;
      y += 68;
    }
    return out;
  })()}

  <!-- browser window -->
  <g transform="translate(1010 300) scale(.84) rotate(-3 490 320)" filter="url(#sh)">
    <rect width="980" height="640" rx="22" fill="#F5F7FB"/>
    <g clip-path="url(#win)">
      <rect width="980" height="74" fill="#E3E9F4"/>
      <rect x="22" y="14" width="250" height="60" rx="14" fill="#F5F7FB"/>
      <circle cx="50" cy="44" r="9" fill="#2D6CDF"/><rect x="70" y="38" width="120" height="12" rx="6" fill="#B7C2D9"/>
      <rect x="0" y="74" width="980" height="64" fill="#F5F7FB"/>
      <rect x="150" y="86" width="560" height="40" rx="20" fill="#E3E9F4"/>
      <text x="176" y="113" font-size="20" fill="#5C6A88">plain-browser.app</text>
      <circle cx="50" cy="106" r="11" fill="#C5CEE2"/><circle cx="94" cy="106" r="11" fill="#C5CEE2"/>
      <!-- page: news layout, ads removed -->
      <rect x="40" y="170" width="560" height="26" rx="6" fill="#1B2A52"/>
      <rect x="40" y="212" width="420" height="26" rx="6" fill="#1B2A52"/>
      <rect x="40" y="268" width="560" height="180" rx="14" fill="#C9DAFB"/>
      <path d="M40 420l120-90 90 60 110-100 240 130v28H40z" fill="#9CC0F7"/><circle cx="500" cy="318" r="26" fill="#fff"/>
      ${[0,1,2,3,4].map(i => `<rect x="40" y="${478 + i*26}" width="${[540,520,548,480,300][i]}" height="12" rx="6" fill="#C5CEE2"/>`).join("")}
      <!-- ad slots, struck out -->
      <g>
        <rect x="650" y="170" width="290" height="250" rx="14" fill="none" stroke="#2D6CDF" stroke-width="3" stroke-dasharray="12 10"/>
        <text x="795" y="304" font-size="26" font-weight="600" fill="#2D6CDF" text-anchor="middle">AD</text>
        <text x="795" y="338" font-size="20" fill="#5C6A88" text-anchor="middle">${t.blocked}</text>
        <rect x="650" y="446" width="290" height="150" rx="14" fill="none" stroke="#2D6CDF" stroke-width="3" stroke-dasharray="12 10"/>
        <text x="795" y="530" font-size="26" font-weight="600" fill="#2D6CDF" text-anchor="middle">AD</text>
      </g>
    </g>
  </g>

  <!-- shield bubble -->
  <g transform="translate(1380 170) rotate(2)" filter="url(#sh2)">
    <rect width="500" height="266" rx="22" fill="#fff"/>
    <path d="M62 52l38 14v32c0 26-16 42-38 52c-22-10-38-26-38-52V66z" transform="translate(12 6)" fill="#2D6CDF"/>
    <path d="M62 96l9 9 18-20" transform="translate(12 6)" fill="none" stroke="#fff" stroke-width="6" stroke-linecap="round" stroke-linejoin="round"/>
    <text x="128" y="84" font-size="30" font-weight="700" fill="#0B1B4D">${t.on}</text>
    <text x="128" y="120" font-size="22" fill="#5C6A88">${t.site}</text>
    <rect x="400" y="62" width="76" height="42" rx="21" fill="#2D6CDF"/><circle cx="455" cy="83" r="16" fill="#fff"/>
    <line x1="28" y1="156" x2="472" y2="156" stroke="#E3E9F4" stroke-width="2"/>
    <text x="28" y="220" font-size="58" font-weight="700" fill="#2D6CDF">37</text>
    <text x="104" y="220" font-size="24" fill="#5C6A88">${t.blocked.replace(/^\d+\s*/, "")}</text>
  </g>
</svg>`;

for (const lang of ["en", "pl"]) {
  for (const [W, H] of [[1920, 1080], [2400, 1200]]) {
    const f = `${OUT}/hero_${W}x${H}_${lang}.png`;
    await sharp(Buffer.from(svg(T[lang], W, H))).flatten({ background: "#1B49B8" }).removeAlpha().png({ compressionLevel: 9 }).toFile(f);
    console.log("wrote", f);
  }
}
