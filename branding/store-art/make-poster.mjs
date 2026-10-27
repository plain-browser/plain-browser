// Renders the Microsoft Store poster art (9:16, 1440x2160) in EN and PL, same look as make-hero.mjs.
// Needs `sharp` (npm i sharp) and Segoe UI visible to fontconfig (a fonts.conf with <dir>/mnt/c/Windows/Fonts</dir>).
// Usage: FONTCONFIG_FILE=fonts.conf node make-poster.mjs <outdir>
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

const svg = (t) => `
<svg xmlns="http://www.w3.org/2000/svg" width="1440" height="2160" viewBox="0 0 1440 2160" font-family="Segoe UI, DejaVu Sans, sans-serif">
  <defs>
    <linearGradient id="bg" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="#0B1B4D"/><stop offset=".55" stop-color="#1B49B8"/><stop offset="1" stop-color="#2D6CDF"/>
    </linearGradient>
    <radialGradient id="glow" cx=".6" cy=".5" r=".5"><stop offset="0" stop-color="#7FB0FF" stop-opacity=".55"/><stop offset="1" stop-color="#7FB0FF" stop-opacity="0"/></radialGradient>
    <filter id="sh" x="-20%" y="-20%" width="140%" height="150%"><feDropShadow dx="0" dy="30" stdDeviation="34" flood-color="#000A2E" flood-opacity=".5"/></filter>
    <filter id="sh2" x="-20%" y="-20%" width="140%" height="160%"><feDropShadow dx="0" dy="14" stdDeviation="16" flood-color="#000A2E" flood-opacity=".45"/></filter>
    <clipPath id="win"><rect x="0" y="0" width="980" height="640" rx="22"/></clipPath>
  </defs>
  <rect width="1440" height="2160" fill="url(#bg)"/>
  <rect width="1440" height="2160" fill="url(#glow)"/>
  <circle cx="1300" cy="200" r="420" fill="none" stroke="#fff" stroke-opacity=".06" stroke-width="2"/>
  <circle cx="1300" cy="200" r="300" fill="none" stroke="#fff" stroke-opacity=".06" stroke-width="2"/>

  <g transform="translate(100 130)">
    <circle cx="44" cy="44" r="44" fill="#fff"/>
    <path fill="#2D6CDF" fill-rule="evenodd" transform="scale(.34375)" d="M84 64h42a46 46 0 0 1 0 92h-14v36H84z M128 94a16 16 0 0 0 0 32a16 16 0 0 0 0-32z"/>
    <text x="114" y="60" font-size="46" font-weight="600" fill="#fff">Plain Browser</text>
  </g>

  <g transform="translate(100 290)"><rect width="${Math.round(t.badge.length * 14.6 + 70)}" height="68" rx="34" fill="#fff" fill-opacity=".12" stroke="#9CC4FF" stroke-opacity=".6"/>
    <text x="34" y="46" font-size="31" font-weight="600" fill="#CFE0FF">${t.badge}</text></g>

  <text x="100" y="570" font-size="136" font-weight="700" fill="#fff" letter-spacing="-3">${t.h1}</text>
  <text x="100" y="715" font-size="136" font-weight="700" fill="#9CC4FF" letter-spacing="-3">${t.h2}</text>
  <text x="104" y="810" font-size="40" fill="#fff" fill-opacity=".88">${t.sub}</text>

  ${(() => {
    let y = 1712, out = "";
    for (const c of t.chips) {
      const w = Math.round(c.length * 16.8 + 110);
      out += `<g transform="translate(100 ${y})"><rect width="${w}" height="68" rx="34" fill="#fff" fill-opacity=".14" stroke="#fff" stroke-opacity=".3"/>
        <circle cx="40" cy="34" r="14" fill="#4ADE80"/><path d="M33 34l5 5L47 29" fill="none" stroke="#0B1B4D" stroke-width="3.8" stroke-linecap="round" stroke-linejoin="round"/>
        <text x="72" y="45" font-size="32" font-weight="600" fill="#fff">${c}</text></g>`;
      y += 84;
    }
    return out;
  })()}

  <!-- browser window -->
  <g transform="translate(170 945) scale(1.05) rotate(-3 490 320)" filter="url(#sh)">
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
  <g transform="translate(800 855) scale(1.1) rotate(2)" filter="url(#sh2)">
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
  const f = `${OUT}/poster_1440x2160_${lang}.png`;
  await sharp(Buffer.from(svg(T[lang]))).flatten({ background: "#1B49B8" }).removeAlpha().png({ compressionLevel: 9 }).toFile(f);
  console.log("wrote", f);
}
