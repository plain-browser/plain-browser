// Copyright Plain Browser contributors. MPL-2.0.
// Minimal zip reader and writer: stored and deflated entries, no zip64, no encryption. Enough to open an
// extension's release zip and write it again with one file changed. No dependencies.
import { deflateRawSync, inflateRawSync, crc32 as zlibCrc32 } from "node:zlib";

const LOCAL_SIG = 0x04034b50;
const CENTRAL_SIG = 0x02014b50;
const END_SIG = 0x06054b50;
// Generous bounds for an extension (uBlock Origin is ~4 MB unpacked). A bigger zip is refused, not read.
export const MAX_ENTRIES = 20000;
export const MAX_UNPACKED = 256 * 1024 * 1024;

// zlib.crc32 exists since Node 22.2; the fallback keeps older runtimes working.
let table;
export function crc32(buf) {
  if (zlibCrc32) return zlibCrc32(buf) >>> 0;
  if (!table) {
    table = new Uint32Array(256);
    for (let n = 0; n < 256; n++) {
      let c = n;
      for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
      table[n] = c >>> 0;
    }
  }
  let c = 0xffffffff;
  for (const b of buf) c = table[(c ^ b) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}

// Why `name` must not be unpacked, or null when it is a plain relative path.
export function unsafeName(name) {
  if (name === "" || name.includes("\0")) return "empty name or NUL";
  if (name.includes("\\")) return "backslash";
  if (name.startsWith("/") || /^[A-Za-z]:/.test(name)) return "absolute path";
  if (name.split("/").some((part) => part === "..")) return "'..' segment";
  return null;
}

// Entries in central directory order: {name, data (uncompressed Buffer), time, date (DOS), isDir}.
// Each entry's CRC-32 is checked.
export function readZip(buf) {
  let end = -1;
  // The end record is 22 bytes plus a comment of up to 64 KiB.
  for (let i = buf.length - 22; i >= Math.max(0, buf.length - 22 - 0xffff); i--) {
    if (buf.readUInt32LE(i) === END_SIG) {
      end = i;
      break;
    }
  }
  if (end < 0) throw new Error("zip: no end of central directory");
  const count = buf.readUInt16LE(end + 10);
  const cdSize = buf.readUInt32LE(end + 12);
  const cdOffset = buf.readUInt32LE(end + 16);
  if (count === 0xffff || cdOffset === 0xffffffff) throw new Error("zip: zip64 is not supported");
  if (buf.readUInt16LE(end + 4) !== 0 || buf.readUInt16LE(end + 6) !== 0) throw new Error("zip: multi-disk");
  if (count > MAX_ENTRIES) throw new Error(`zip: ${count} entries`);
  if (cdOffset + cdSize > end) throw new Error("zip: central directory out of bounds");

  const entries = [];
  const seen = new Set();
  let unpacked = 0;
  let p = cdOffset;
  for (let i = 0; i < count; i++) {
    if (buf.readUInt32LE(p) !== CENTRAL_SIG) throw new Error("zip: bad central directory entry");
    const flags = buf.readUInt16LE(p + 8);
    const method = buf.readUInt16LE(p + 10);
    const time = buf.readUInt16LE(p + 12);
    const date = buf.readUInt16LE(p + 14);
    const crc = buf.readUInt32LE(p + 16);
    const csize = buf.readUInt32LE(p + 20);
    const usize = buf.readUInt32LE(p + 24);
    const nameLen = buf.readUInt16LE(p + 28);
    const extraLen = buf.readUInt16LE(p + 30);
    const commentLen = buf.readUInt16LE(p + 32);
    const localOffset = buf.readUInt32LE(p + 42);
    const name = buf.subarray(p + 46, p + 46 + nameLen).toString("utf8");
    p += 46 + nameLen + extraLen + commentLen;

    if (flags & 1) throw new Error(`zip: ${name} is encrypted`);
    const bad = unsafeName(name);
    if (bad) throw new Error(`zip: refusing entry ${JSON.stringify(name)}: ${bad}`);
    if (seen.has(name)) throw new Error(`zip: duplicate entry ${name}`);
    seen.add(name);
    unpacked += usize;
    if (unpacked > MAX_UNPACKED) throw new Error("zip: unpacks to too much data");

    if (buf.readUInt32LE(localOffset) !== LOCAL_SIG) throw new Error(`zip: bad local header for ${name}`);
    const start = localOffset + 30 + buf.readUInt16LE(localOffset + 26) + buf.readUInt16LE(localOffset + 28);
    if (start + csize > buf.length) throw new Error(`zip: ${name} out of bounds`);
    const raw = buf.subarray(start, start + csize);
    let data;
    if (method === 0) data = Buffer.from(raw);
    else if (method === 8) data = inflateRawSync(raw, { maxOutputLength: Math.max(usize, 1) });
    else throw new Error(`zip: ${name} uses compression method ${method}`);
    if (data.length !== usize || crc32(data) !== crc) throw new Error(`zip: ${name} is corrupt (size or CRC)`);
    entries.push({ name, data, time, date, isDir: name.endsWith("/") });
  }
  return entries;
}

// Writes `entries` ({name, data, time?, date?}) in order. Directories (name ending in "/") are stored, files
// deflated unless that does not help. Same input, same bytes.
export function writeZip(entries) {
  const locals = [];
  const centrals = [];
  let offset = 0;
  for (const e of entries) {
    const bad = unsafeName(e.name);
    if (bad) throw new Error(`zip: refusing entry ${JSON.stringify(e.name)}: ${bad}`);
    const name = Buffer.from(e.name, "utf8");
    const data = e.data ?? Buffer.alloc(0);
    const crc = crc32(data);
    let method = 0;
    let body = data;
    if (data.length > 0) {
      const deflated = deflateRawSync(data, { level: 9 });
      if (deflated.length < data.length) {
        method = 8;
        body = deflated;
      }
    }
    // Bit 11: the name is UTF-8.
    const flags = /^[\x20-\x7e]*$/.test(e.name) ? 0 : 0x800;
    const time = e.time ?? 0;
    const date = e.date ?? 0x21; // 1980-01-01
    const local = Buffer.alloc(30);
    local.writeUInt32LE(LOCAL_SIG, 0);
    local.writeUInt16LE(20, 4); // version needed: 2.0
    local.writeUInt16LE(flags, 6);
    local.writeUInt16LE(method, 8);
    local.writeUInt16LE(time, 10);
    local.writeUInt16LE(date, 12);
    local.writeUInt32LE(crc, 14);
    local.writeUInt32LE(body.length, 18);
    local.writeUInt32LE(data.length, 22);
    local.writeUInt16LE(name.length, 26);
    local.writeUInt16LE(0, 28);
    const central = Buffer.alloc(46);
    central.writeUInt32LE(CENTRAL_SIG, 0);
    central.writeUInt16LE(0x0314, 4); // made by: Unix, 2.0
    central.writeUInt16LE(20, 6);
    central.writeUInt16LE(flags, 8);
    central.writeUInt16LE(method, 10);
    central.writeUInt16LE(time, 12);
    central.writeUInt16LE(date, 14);
    central.writeUInt32LE(crc, 16);
    central.writeUInt32LE(body.length, 20);
    central.writeUInt32LE(data.length, 24);
    central.writeUInt16LE(name.length, 28);
    // Unix mode in the high half of the external attributes: rw-r--r--, or a directory rwxr-xr-x.
    const dir = e.name.endsWith("/");
    central.writeUInt32LE((((dir ? 0o40755 : 0o100644) << 16) | (dir ? 0x10 : 0)) >>> 0, 38);
    central.writeUInt32LE(offset, 42);
    locals.push(local, name, body);
    centrals.push(central, name);
    offset += local.length + name.length + body.length;
  }
  const cd = Buffer.concat(centrals);
  const end = Buffer.alloc(22);
  end.writeUInt32LE(END_SIG, 0);
  end.writeUInt16LE(entries.length, 8);
  end.writeUInt16LE(entries.length, 10);
  end.writeUInt32LE(cd.length, 12);
  end.writeUInt32LE(offset, 16);
  if (entries.length > 0xffff || offset > 0xffffffff) throw new Error("zip: too big without zip64");
  return Buffer.concat([...locals, cd, end]);
}
