#!/usr/bin/env node
// Reference broker.
//   POST /activate {hwid,key}            -> {token,expires_in}
//   POST /ask      {hwid,api_token,prompt,image(s),stream} -> {answer} | SSE
import http from "node:http";
import crypto from "node:crypto";

const PORT = Number(process.env.PORT || 8787);
const UPSTREAM = process.env.UPSTREAM_URL || "https://api.openai.com/v1/chat/completions";
const UPSTREAM_KEY = process.env.UPSTREAM_KEY || "";
const MODEL = process.env.MODEL || "gpt-4o-mini";
const TOKEN_TTL = Number(process.env.TOKEN_TTL || 86400);
const ADMIN_TOKEN = process.env.ADMIN_TOKEN || "";

const LICENSES = new Map(
  (process.env.LICENSE_KEYS || "DEMO-KEY-1").split(",").map((k) => [k.trim(), true])
);
const REVOKED = new Set();
const KEY_SECRETS = new Map();

for (const pair of (process.env.BROKER_KEYS || "").split(",").filter(Boolean)) {
  const [kid, secret] = pair.split(":");
  KEY_SECRETS.set(kid, secret);
}
if (!KEY_SECRETS.size) {
  KEY_SECRETS.set("k1", process.env.BROKER_SECRET || crypto.randomBytes(32).toString("hex"));
}
let CURRENT_KID = process.env.BROKER_CURRENT_KID || [...KEY_SECRETS.keys()].at(-1);

function signToken(hwid) {
  const kid = CURRENT_KID;
  const payload = Buffer.from(
    JSON.stringify({
      kid,
      hwid,
      exp: Math.floor(Date.now() / 1000) + TOKEN_TTL,
    })
  ).toString("base64url");
  const mac = crypto.createHmac("sha256", KEY_SECRETS.get(kid)).update(payload).digest("base64url");
  return payload + "." + mac;
}

function verifyToken(token, hwid) {
  const [payload, mac] = String(token || "").split(".");
  if (!payload || !mac) return false;
  let data;
  try {
    data = JSON.parse(Buffer.from(payload, "base64url").toString());
  } catch {
    return false;
  }
  const secret = KEY_SECRETS.get(data.kid);
  if (!secret) return false;
  const expected = crypto.createHmac("sha256", secret).update(payload).digest("base64url");
  if (expected.length !== mac.length) return false;
  if (!crypto.timingSafeEqual(Buffer.from(expected), Buffer.from(mac))) return false;
  return data.hwid === hwid && data.exp > Math.floor(Date.now() / 1000);
}

function json(res, code, body) {
  const text = JSON.stringify(body);
  res.writeHead(code, {
    "content-type": "application/json",
    "content-length": Buffer.byteLength(text),
  });
  res.end(text);
}

async function readBody(req) {
  const chunks = [];
  let size = 0;
  for await (const chunk of req) {
    size += chunk.length;
    if (size > 32 * 1024 * 1024) throw new Error("body too large");
    chunks.push(chunk);
  }
  return JSON.parse(Buffer.concat(chunks).toString("utf8") || "{}");
}

function admin(req) {
  return ADMIN_TOKEN && req.headers["x-admin-token"] === ADMIN_TOKEN;
}

async function activate(req, res) {
  const { hwid, key } = await readBody(req);
  if (!hwid) return json(res, 400, { message: "hwid required" });
  if (REVOKED.has(key)) return json(res, 403, { message: "revoked" });
  if (!LICENSES.has(key)) return json(res, 403, { message: "unknown key" });
  json(res, 200, { token: signToken(hwid), expires_in: TOKEN_TTL });
}

async function ask(req, res) {
  const body = await readBody(req);
  const { hwid, api_token, prompt, image, images, stream } = body;
  if (REVOKED.has(hwid)) return json(res, 403, { message: "revoked" });
  if (!verifyToken(api_token, hwid)) return json(res, 401, { message: "invalid token" });

  const list = Array.isArray(images) && images.length ? images : image ? [image] : [];
  if (!list.length) return json(res, 400, { message: "image required" });

  const upstream = await fetch(UPSTREAM, {
    method: "POST",
    headers: {
      "content-type": "application/json",
      authorization: "Bearer " + UPSTREAM_KEY,
    },
    body: JSON.stringify({
      model: MODEL,
      max_tokens: 1200,
      stream: Boolean(stream),
      messages: [
        {
          role: "user",
          content: [
            { type: "text", text: prompt || "Explain the screenshot." },
            ...list.map((url) => ({ type: "image_url", image_url: { url } })),
          ],
        },
      ],
    }),
  });

  if (!upstream.ok) {
    const text = await upstream.text();
    return json(res, 502, { message: "upstream " + upstream.status + ": " + text.slice(0, 400) });
  }

  if (!stream) {
    const data = await upstream.json();
    return json(res, 200, { answer: data.choices?.[0]?.message?.content ?? "" });
  }

  res.writeHead(200, {
    "content-type": "text/event-stream",
    "cache-control": "no-cache",
    connection: "keep-alive",
  });

  const reader = upstream.body.getReader();
  const decoder = new TextDecoder();
  let buffer = "";
  for (;;) {
    const { value, done } = await reader.read();
    if (done) break;
    buffer += decoder.decode(value, { stream: true });
    const lines = buffer.split("\n");
    buffer = lines.pop() ?? "";
    for (const line of lines) {
      const trimmed = line.trim();
      if (!trimmed.startsWith("data:")) continue;
      const payload = trimmed.slice(5).trim();
      if (payload === "[DONE]") continue;
      try {
        const delta = JSON.parse(payload).choices?.[0]?.delta?.content ?? "";
        if (delta) {
          res.write("data: " + JSON.stringify({ choices: [{ delta: { content: delta } }] }) + "\n\n");
        }
      } catch {
        /* skip malformed */
      }
    }
  }
  res.write("data: [DONE]\n\n");
  res.end();
}

async function revoke(req, res) {
  if (!admin(req)) return json(res, 401, { message: "admin token required" });
  const { hwid, key } = await readBody(req);
  if (hwid) REVOKED.add(hwid);
  if (key) {
    REVOKED.add(key);
    LICENSES.delete(key);
  }
  json(res, 200, { revoked: REVOKED.size });
}

async function rotate(req, res) {
  if (!admin(req)) return json(res, 401, { message: "admin token required" });
  const kid = "k" + (KEY_SECRETS.size + 1);
  KEY_SECRETS.set(kid, crypto.randomBytes(32).toString("hex"));
  CURRENT_KID = kid;
  json(res, 200, { current: CURRENT_KID });
}

http
  .createServer(async (req, res) => {
    try {
      if (req.method === "POST" && req.url === "/activate") return await activate(req, res);
      if (req.method === "POST" && req.url === "/ask") return await ask(req, res);
      if (req.method === "POST" && req.url === "/revoke") return await revoke(req, res);
      if (req.method === "POST" && req.url === "/rotate") return await rotate(req, res);
      if (req.url === "/health") return json(res, 200, { ok: true });
      json(res, 404, { message: "not found" });
    } catch (error) {
      json(res, 500, { message: String(error) });
    }
  })
  .listen(PORT, () => {
    console.log("broker on :" + PORT + " upstream=" + UPSTREAM);
  });
