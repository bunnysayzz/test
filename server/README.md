# Broker reference server

Node 18+.

```
BROKER_SECRET=$(openssl rand -hex 32) UPSTREAM_KEY=sk-... \
  LICENSE_KEYS=AAAA-BBBB ADMIN_TOKEN=secret node broker.js
```

Client config:

```
mode=broker
endpoint=https://broker.example/ask
license_url=https://broker.example/activate
```

Rotate keys: `curl -X POST -H "x-admin-token: $ADMIN_TOKEN" :8787/rotate`

Revoke: same with `-d '{"hwid":"..."}'` to `/revoke`.
