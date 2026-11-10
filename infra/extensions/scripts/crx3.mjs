// Copyright Plain Browser contributors. MPL-2.0.
// CRX3 packer and verifier (Chromium components/crx_file/crx3.proto). No dependencies.
//
//   "Cr24" | uint32LE 3 | uint32LE header length | CrxFileHeader | zip
//
//   CrxFileHeader { repeated AsymmetricKeyProof sha256_with_rsa = 2; bytes signed_header_data = 10000; }
//   AsymmetricKeyProof { bytes public_key = 1 (SPKI DER); bytes signature = 2; }
//   SignedData { bytes crx_id = 1 (first 16 bytes of SHA-256 of the SPKI DER); }
//
// The signature is RSASSA-PKCS1-v1_5 with SHA-256 over
//   "CRX3 SignedData\x00" | uint32LE len(signed_header_data) | signed_header_data | zip
import { createHash, createPublicKey, sign, verify } from "node:crypto";

const MAGIC = Buffer.from("Cr24");
const SIGN_PREFIX = Buffer.from("CRX3 SignedData\x00", "binary");
// Chromium refuses headers bigger than this (crx_verifier.cc kMaxHeaderSize is 256 KiB); ours are ~600 bytes.
const MAX_HEADER = 256 * 1024;

function varint(n) {
  const out = [];
  while (n > 0x7f) {
    out.push((n & 0x7f) | 0x80);
    n >>>= 7;
  }
  out.push(n);
  return Buffer.from(out);
}

// A length-delimited protobuf field (wire type 2).
function field(number, bytes) {
  return Buffer.concat([varint((number << 3) | 2), varint(bytes.length), bytes]);
}

// Fields of one protobuf message as [number, value]; value is a Buffer for wire type 2, a number otherwise.
function parseFields(buf) {
  const fields = [];
  let p = 0;
  const readVarint = () => {
    let result = 0;
    let shift = 0;
    for (;;) {
      if (p >= buf.length || shift > 35) throw new Error("crx: bad varint");
      const b = buf[p++];
      result += (b & 0x7f) * 2 ** shift;
      if (!(b & 0x80)) return result;
      shift += 7;
    }
  };
  while (p < buf.length) {
    const tag = readVarint();
    const number = Math.floor(tag / 8);
    const wire = tag % 8;
    if (wire === 0) fields.push([number, readVarint()]);
    else if (wire === 2) {
      const len = readVarint();
      if (p + len > buf.length) throw new Error("crx: field out of bounds");
      fields.push([number, buf.subarray(p, p + len)]);
      p += len;
    } else if (wire === 5) {
      p += 4;
    } else if (wire === 1) {
      p += 8;
    } else throw new Error(`crx: unsupported wire type ${wire}`);
  }
  if (p !== buf.length) throw new Error("crx: truncated field");
  return fields;
}

// Chromium's ID: the first 16 bytes of SHA-256 of `bytes` (an SPKI DER key for a CRX), each hex nibble 0-f
// written as a-p.
export function extensionId(bytes) {
  const hex = createHash("sha256").update(bytes).digest("hex").slice(0, 32);
  return [...hex].map((c) => String.fromCharCode(97 + parseInt(c, 16))).join("");
}

function crxId(spkiDer) {
  return createHash("sha256").update(spkiDer).digest().subarray(0, 16);
}

function signedMessage(signedHeaderData, zip) {
  const len = Buffer.alloc(4);
  len.writeUInt32LE(signedHeaderData.length);
  return Buffer.concat([SIGN_PREFIX, len, signedHeaderData, zip]);
}

// `privateKey`: a KeyObject or PEM text of an RSA private key. Returns the CRX bytes.
export function packCrx3(zip, privateKey) {
  const spki = createPublicKey(privateKey).export({ type: "spki", format: "der" });
  const signedHeaderData = field(1, crxId(spki));
  const signature = sign("sha256", signedMessage(signedHeaderData, zip), privateKey);
  const header = Buffer.concat([
    field(2, Buffer.concat([field(1, spki), field(2, signature)])),
    field(10000, signedHeaderData),
  ]);
  const prefix = Buffer.alloc(12);
  MAGIC.copy(prefix, 0);
  prefix.writeUInt32LE(3, 4);
  prefix.writeUInt32LE(header.length, 8);
  return Buffer.concat([prefix, header, zip]);
}

// Checks a CRX3 the way Chromium does for RSA proofs: every proof's signature is valid, and one of the keys
// hashes to the crx_id in the signed header. Throws on anything else. Returns {id, publicKey (SPKI DER), zip}.
export function verifyCrx3(buf) {
  if (buf.length < 12 || !buf.subarray(0, 4).equals(MAGIC)) throw new Error("crx: no Cr24 magic");
  if (buf.readUInt32LE(4) !== 3) throw new Error(`crx: version ${buf.readUInt32LE(4)}, not 3`);
  const headerLen = buf.readUInt32LE(8);
  if (headerLen > MAX_HEADER || 12 + headerLen > buf.length) throw new Error("crx: bad header length");
  const header = buf.subarray(12, 12 + headerLen);
  const zip = buf.subarray(12 + headerLen);

  const proofs = [];
  let signedHeaderData = null;
  for (const [number, value] of parseFields(header)) {
    if (number === 2 && Buffer.isBuffer(value)) {
      const proof = {};
      for (const [n, v] of parseFields(value)) {
        if (n === 1) proof.publicKey = v;
        if (n === 2) proof.signature = v;
      }
      if (!proof.publicKey || !proof.signature) throw new Error("crx: incomplete key proof");
      proofs.push(proof);
    } else if (number === 3) {
      throw new Error("crx: ECDSA proofs are not supported here");
    } else if (number === 10000 && Buffer.isBuffer(value)) {
      signedHeaderData = value;
    }
  }
  if (!signedHeaderData) throw new Error("crx: no signed header data");
  if (proofs.length === 0) throw new Error("crx: no RSA proof");
  const id = parseFields(signedHeaderData).find(([n]) => n === 1)?.[1];
  if (!Buffer.isBuffer(id) || id.length !== 16) throw new Error("crx: no crx_id");

  const message = signedMessage(signedHeaderData, zip);
  let developerKey = null;
  for (const { publicKey, signature } of proofs) {
    const key = createPublicKey({ key: publicKey, format: "der", type: "spki" });
    if (key.asymmetricKeyType !== "rsa") throw new Error("crx: proof key is not RSA");
    if (!verify("sha256", message, key, signature)) throw new Error("crx: bad signature");
    if (crxId(publicKey).equals(id)) developerKey = publicKey;
  }
  if (!developerKey) throw new Error("crx: crx_id matches no key");
  return { id: extensionId(developerKey), publicKey: Buffer.from(developerKey), zip: Buffer.from(zip) };
}
