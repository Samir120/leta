# Leta — API Guide

Companion to `02-api-spec.yaml` (the normative contract). This document explains the API by walking through the SwadeStack integration.

## 1. Run it

```bash
docker run -d --name leta \
  -p 127.0.0.1:7700:7700 \
  -v leta-data:/data \
  -e LETA_MASTER_KEY=change-me \
  ghcr.io/<owner>/leta:1
curl -s localhost:7700/health
# {"status":"available"}
```

All examples below assume `H='Authorization: Bearer change-me'`.

## 2. Mental model

- **Index** — a named collection of documents with its own settings. SwadeStack uses one: `products`.
- **Document** — a flat-ish JSON object with a primary key. Leta stores the whole object and indexes the string fields listed in `searchableAttributes`.
- **Source of truth is PostgreSQL.** Leta's index is derived and disposable; the recovery procedure for any corruption or upgrade is a full reindex.

## 3. Search document shape for SwadeStack

Do not send Sequelize model rows. Map each product to a search document that contains exactly what search and the listing card need:

```ts
// backend/src/search/mapping.ts
export const SEARCH_DOC_VERSION = 1; // bump on any change → full reindex required

export function toSearchDoc(p: ProductWithRelations): SearchDoc {
  return {
    id: p.id,
    name: p.name,
    brand: p.brand?.name ?? null,
    category: p.category?.name ?? null,
    sku: p.sku,
    specs: p.specs,                // { type: "DDR5", capacity_gb: 32, speed_mhz: 6000 }
    description: p.shortDescription,
    price: p.currentPrice,
    in_stock: p.stockQuantity > 0,
    slug: p.slug,
    image: p.primaryImageUrl,
  };
}
```

Settings for this shape (set once, idempotent on deploy):

```bash
curl -X PATCH localhost:7700/indexes/products/settings -H "$H" -H 'Content-Type: application/json' -d '{
  "searchableAttributes": ["name", "brand", "sku", "specs.type", "category", "description"],
  "displayedAttributes": ["id", "name", "brand", "price", "in_stock", "slug", "image"],
  "typoTolerance": { "disableOnAttributes": ["sku"] },
  "filterableAttributes": ["brand", "category", "specs.type", "price", "in_stock"]
}'
```

`searchableAttributes` order matters: a match in `name` outranks the same match in `description`. SKUs get no typo tolerance because a one-character difference is a different product. `filterableAttributes` is accepted in v1 and only becomes active in v2; declaring it now means v2 needs no reindex.

## 4. Getting data in

### 4.1 Full reindex (deploy, schema change, nightly safety net)

```ts
// backend/scripts/search-reindex.ts
const BATCH = 1000;
let offset = 0;
for (;;) {
  const rows = await Product.findAll({ include: [Brand, Category], limit: BATCH, offset, order: [["id", "ASC"]] });
  if (rows.length === 0) break;
  await leta.post("/indexes/products/documents", rows.map(toSearchDoc));
  offset += rows.length;
}
```

Because `POST /documents` replaces by primary key, reindexing into the live index is safe: no downtime, stale documents are overwritten. Products deleted from Postgres are handled separately (4.2) or by a periodic reconciliation that diffs IDs.

### 4.2 Incremental updates via outbox

Sequelize hooks (`afterSave`, `afterDestroy`) are the quick route but lose writes if Leta is down. The robust route is an outbox:

```sql
CREATE TABLE search_outbox (
  id         BIGSERIAL PRIMARY KEY,
  product_id INTEGER NOT NULL,
  op         TEXT NOT NULL CHECK (op IN ('upsert','delete')),
  created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
```

Insert into `search_outbox` in the same transaction as the product/price/stock change. A worker (PM2 process or interval in the API) drains it every few seconds:

```ts
const rows = await SearchOutbox.findAll({ order: [["id", "ASC"]], limit: 500 });
const upserts = rows.filter(r => r.op === "upsert").map(r => r.product_id);
const deletes = rows.filter(r => r.op === "delete").map(r => r.product_id);
if (upserts.length) {
  const products = await Product.findAll({ where: { id: upserts }, include: [Brand, Category] });
  await leta.post("/indexes/products/documents", products.map(toSearchDoc));
}
for (const id of deletes) await leta.delete(`/indexes/products/documents/${id}`);
await SearchOutbox.destroy({ where: { id: rows.map(r => r.id) } });
```

Stock and price change far more often than names; `PUT /documents` (partial update) exists so those writes can send `{id, price, in_stock}` only.

## 5. Querying

### 5.1 Backend route

The frontend never talks to Leta directly in v1. One Express route validates, forwards, and shapes the response:

```ts
router.get("/api/search", async (req, res) => {
  const q = String(req.query.q ?? "").slice(0, 200);
  const page = Math.max(1, Number(req.query.page ?? 1));
  const limit = 24;
  try {
    const r = await leta.post("/indexes/products/search", {
      q, limit, offset: (page - 1) * limit,
      attributesToHighlight: ["name"],
    });
    res.json({ hits: r.hits, total: r.estimatedTotalHits, page, tookMs: r.processingTimeMs });
  } catch (e) {
    log.warn({ err: e }, "leta unavailable, falling back to SQL");
    res.json(await sqlFallbackSearch(q, page, limit));
  }
});
```

The fallback keeps SwadeStack searchable if Leta is down; it is slower and dumber, and the warning log is the signal to look at Leta.

### 5.2 Frontend usage

Two consumers of the same route:

- **Search-as-you-type dropdown:** fire on every keystroke, debounce ~100 ms, `limit=5`, render `hits[i]._formatted.name` so the matched terms are emphasised. Cancel in-flight requests when a new keystroke arrives (AbortController) so slow responses cannot overwrite fast ones.
- **Results page:** `limit=24`, paginated, render the plain fields.

A search request/response, end to end:

```bash
curl -s localhost:7700/indexes/products/search -H "$H" -H 'Content-Type: application/json' \
  -d '{"q":"corsiar ddr5 600","limit":3,"attributesToHighlight":["name"]}'
```

```json
{
  "hits": [
    {
      "id": 4471,
      "name": "Corsair Vengeance DDR5 32GB (2x16) 6000MHz CL30",
      "brand": "Corsair", "price": 1290, "in_stock": true,
      "slug": "corsair-vengeance-ddr5-32gb-6000", "image": "/img/4471.webp",
      "_formatted": { "name": "<em>Corsair</em> Vengeance <em>DDR5</em> 32GB (2x16) <em>6000</em>MHz CL30" }
    }
  ],
  "query": "corsiar ddr5 600",
  "limit": 3, "offset": 0,
  "estimatedTotalHits": 7,
  "processingTimeMs": 1.2
}
```

What happened: `corsiar` matched `corsair` with one typo (length ≥ 5), `ddr5` matched exactly, `600` matched `6000mhz` as a prefix because it is the last term.

## 6. Errors

Every error is JSON with a stable `code` you can branch on:

```json
{ "code": "index_not_found", "message": "Index `products` not found.", "type": "not_found" }
```

| HTTP | `type` | Typical codes |
|---|---|---|
| 400 | `invalid_request` | `invalid_index_uid`, `missing_primary_key`, `unknown_field`, `invalid_search_parameter` |
| 401 | `auth` | `missing_authorization`, `invalid_api_key` |
| 404 | `not_found` | `index_not_found`, `document_not_found` |
| 409 | `invalid_request` | `index_already_exists` |
| 413 | `invalid_request` | `payload_too_large` |
| 500 | `internal` | `internal` (details in server log, correlated by `X-Request-Id`) |

## 7. Operating it

- `GET /health` for PM2/Docker/uptime checks; 503 while restoring on startup.
- `GET /metrics` for Prometheus; the interesting series are `leta_search_duration_seconds` (histogram) and `leta_index_documents`.
- `POST /snapshots` before an upgrade; then `docker pull` + restart. On-disk format is versioned and forward-compatible within v1.
- If anything looks wrong: stop, wipe `/data`, start, run the reindex script. That is the whole disaster-recovery plan, and it is intentional.

## 8. What changes in v2

Same endpoints, three new request fields on search: `filter` (`"brand = Corsair AND price < 2000 AND in_stock = true"`), `facets` (`["brand","specs.type"]` → counts in `facetDistribution`), and `sort` (`["price:asc"]`). Because `filterableAttributes` is declared in v1, enabling them is a server upgrade, not a reindex.
