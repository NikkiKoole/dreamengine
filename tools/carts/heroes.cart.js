// Sprites for heroes.c — "HEROES OF MIGHT & MAGIC" (a tiny HoMM demake).
// Flat index arrays so we can reach the extended palette and the "magic" team
// colors that pal() recolors per owner:  28 = body/banner, 29 = dark/shadow.
// Crowd-cart trick: hero, town and every creature share these magic indices and
// get tinted blue (player) / red (CPU) / grey (neutral) at draw time.
//
// Slot layout (8-col sheet):
//   1 grass  2 forest  3 mountain  4 water  5 road            (overworld terrain, opaque)
//   6 gold pile  7 ore pile  8 chest  9 mine                  (map objects, transparent)
//  10 town/castle (magic 28/29)  11 hero (magic 28/29)
//  12 battle field (opaque)  13 battle rock (obstacle)
//  16 pikeman  17 archer  18 griffin  19 ogre                 (creatures, magic 28/29)

const S = 16;
const OUT = 16;   // outline (brownish-black, reads near-black, never index 0)

const blank = () => Array.from({ length: S }, () => new Array(S).fill(0));
function box(g, x0, y0, x1, y1, c) {
  for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++)
    if (x >= 0 && x < S && y >= 0 && y < S) g[y][x] = c;
}
function dot(g, x, y, c) { if (x >= 0 && x < S && y >= 0 && y < S) g[y][x] = c; }
const flat = g => g.flat();

// 1px OUT outline traced into transparent pixels touching the silhouette
function outlined(g) {
  const o = g.map(r => r.slice());
  for (let y = 0; y < S; y++) for (let x = 0; x < S; x++) {
    if (g[y][x] !== 0) continue;
    let touch = false;
    for (let dy = -1; dy <= 1 && !touch; dy++) for (let dx = -1; dx <= 1; dx++) {
      const ny = y + dy, nx = x + dx;
      if (nx >= 0 && nx < S && ny >= 0 && ny < S && g[ny][nx] !== 0 && g[ny][nx] !== OUT) { touch = true; break; }
    }
    if (touch) o[y][x] = OUT;
  }
  return o;
}

// ── terrain (fully opaque — no index 0) ────────────────────────────────────────
function grass() {
  const g = blank(); box(g, 0, 0, 15, 15, 27);          // medium green
  for (let y = 0; y < S; y++) for (let x = 0; x < S; x++) {
    const n = (x * 7 + y * 13) % 17;
    if (n === 0) g[y][x] = 11; else if (n === 5) g[y][x] = 3;
  }
  return flat(g);
}
function forest() {
  const g = blank(); box(g, 0, 0, 15, 15, 3);           // dark-green floor
  const trees = [[3, 5], [10, 4], [7, 10]];
  for (const [tx, ty] of trees) {
    box(g, tx - 1, ty, tx + 2, ty + 2, 27);             // canopy
    box(g, tx, ty - 1, tx + 1, ty - 1, 11);             // highlight
    dot(g, tx, ty + 3, 20); dot(g, tx + 1, ty + 3, 20); // trunk
  }
  return flat(g);
}
function mountain() {
  const g = blank(); box(g, 0, 0, 15, 15, 3);           // grass skirt
  for (let y = 3; y < 16; y++) { const w = Math.min(7, y - 1); box(g, 8 - w, y, 7 + w, y, 22); }
  box(g, 5, 5, 10, 7, 5);                               // shadow flank
  box(g, 6, 3, 9, 4, 6); box(g, 7, 2, 8, 2, 7);         // peak + snow
  return flat(g);
}
function water() {
  const g = blank(); box(g, 0, 0, 15, 15, 28);          // (magic) deep — but we want fixed blue
  box(g, 0, 0, 15, 15, 12);
  for (let y = 0; y < S; y++) for (let x = 0; x < S; x++) {
    const n = (x * 5 + y * 9) % 11;
    if (n === 0) g[y][x] = 28; else if (n === 4) g[y][x] = 1;
  }
  box(g, 2, 4, 5, 4, 7); box(g, 9, 10, 12, 10, 7);      // foam
  return flat(g);
}
function road() {
  const g = blank(); box(g, 0, 0, 15, 15, 27);          // grass shoulders
  box(g, 4, 0, 11, 15, 4);                              // brown track
  box(g, 5, 0, 10, 15, 20);                             // darker rut
  for (let y = 1; y < 16; y += 4) box(g, 7, y, 8, y + 1, 23); // dashes
  return flat(g);
}
function battlefield() {
  const g = blank(); box(g, 0, 0, 15, 15, 20);          // packed dirt
  for (let y = 0; y < S; y++) for (let x = 0; x < S; x++) {
    const n = (x * 3 + y * 7) % 13;
    if (n === 0) g[y][x] = 4; else if (n === 6) g[y][x] = 16;
  }
  return flat(g);
}

// ── map objects (transparent bg) ───────────────────────────────────────────────
function goldPile() {
  const g = blank();
  box(g, 4, 10, 11, 13, 9);                             // base coins
  box(g, 5, 7, 9, 10, 10); box(g, 8, 8, 11, 11, 10);
  dot(g, 6, 7, 23); dot(g, 9, 9, 23);                   // glints
  box(g, 6, 4, 8, 6, 10); dot(g, 7, 4, 23);             // top coin
  return flat(outlined(g));
}
function orePile() {
  const g = blank();
  box(g, 3, 9, 12, 13, 5);                              // rocks
  box(g, 5, 6, 9, 10, 6); box(g, 8, 8, 11, 11, 6);
  dot(g, 6, 6, 7); dot(g, 9, 8, 7);                     // facets
  box(g, 6, 11, 9, 13, 25);                             // ore glow (orange seam)
  return flat(outlined(g));
}
function chest() {
  const g = blank();
  box(g, 2, 7, 13, 14, 4);                              // body
  box(g, 2, 4, 13, 7, 20);                              // lid
  box(g, 2, 7, 13, 7, 10);                              // gold band
  box(g, 7, 8, 8, 11, 10);                              // lock
  dot(g, 4, 5, 23); dot(g, 11, 5, 23);                  // highlights
  return flat(outlined(g));
}
function mine() {
  const g = blank();
  box(g, 1, 6, 14, 14, 5);                              // mountain face
  box(g, 4, 8, 11, 14, 16);                             // cave mouth (dark)
  box(g, 5, 9, 10, 14, 0);
  box(g, 9, 1, 9, 6, 6);                                // flagpole
  box(g, 10, 1, 14, 4, 28);                             // banner (magic → owner color)
  box(g, 6, 12, 7, 13, 25);                             // ore cart spark
  return flat(outlined(g));
}

// ── town / castle (transparent; magic 28 walls, 29 roofs, 7 windows) ────────────
function town() {
  const g = blank();
  box(g, 1, 9, 14, 15, 28);                             // curtain wall
  box(g, 1, 7, 3, 9, 28);  box(g, 12, 7, 14, 9, 28);    // corner towers
  box(g, 6, 5, 9, 9, 28);                               // keep
  box(g, 1, 6, 3, 6, 29);  box(g, 12, 6, 14, 6, 29);    // tower roofs
  box(g, 6, 3, 9, 4, 29);                               // keep roof (peak)
  box(g, 7, 1, 8, 2, 29);
  // crenellations
  for (let x = 1; x <= 14; x += 2) dot(g, x, 9, 29);
  box(g, 6, 11, 9, 15, 16);                             // gate
  box(g, 7, 12, 8, 15, 25);                             // gate glow
  box(g, 2, 7, 2, 7, 7); box(g, 13, 7, 13, 7, 7);       // tower windows
  box(g, 7, 6, 8, 7, 7);                                // keep window
  return flat(outlined(g));
}

// ── hero on the overworld (magic 28 cloak, 29 trim) ─────────────────────────────
function hero() {
  const g = blank();
  box(g, 6, 2, 9, 4, 29);                               // helm/hat
  box(g, 6, 5, 9, 7, 15);                               // face
  box(g, 5, 8, 10, 12, 28);                             // cloak/body
  box(g, 4, 8, 4, 11, 28); box(g, 11, 8, 11, 11, 28);   // arms
  box(g, 12, 3, 13, 9, 28);                             // banner cloth
  box(g, 11, 2, 11, 12, 5);                             // banner pole
  box(g, 6, 13, 7, 15, 5); box(g, 8, 13, 9, 15, 5);     // legs
  return flat(outlined(g));
}

// ── battle obstacle rock (opaque-ish, no team color) ────────────────────────────
function rock() {
  const g = blank();
  box(g, 2, 6, 13, 14, 5);
  box(g, 4, 4, 11, 8, 6);
  dot(g, 5, 5, 7); dot(g, 9, 6, 7);
  box(g, 3, 13, 12, 14, 16);
  return flat(outlined(g));
}

// ── creatures (transparent; magic 28 body, 29 dark, 15 skin, 5/6 metal) ─────────
function pikeman() {
  const g = blank();
  box(g, 5, 4, 9, 5, 29);                               // helm
  box(g, 6, 6, 8, 8, 15);                               // face
  box(g, 5, 9, 9, 13, 28);                              // tunic
  box(g, 4, 9, 4, 12, 28); box(g, 10, 9, 10, 12, 28);   // arms
  box(g, 12, 0, 12, 14, 6);                             // pike shaft
  box(g, 11, 0, 13, 1, 7);                              // pike head
  box(g, 5, 14, 6, 15, 5); box(g, 8, 14, 9, 15, 5);     // legs
  box(g, 4, 10, 11, 11, 29);                            // belt
  return flat(outlined(g));
}
function archer() {
  const g = blank();
  box(g, 5, 3, 9, 4, 29);                               // hood
  box(g, 6, 5, 8, 7, 15);                               // face
  box(g, 5, 8, 9, 13, 28);                              // tunic
  box(g, 4, 8, 4, 11, 28);                              // draw arm
  // bow (curved)
  const bow = [[11, 4], [12, 5], [12, 6], [13, 7], [13, 8], [12, 9], [12, 10], [11, 11]];
  for (const [bx, by] of bow) dot(g, bx, by, 4);
  for (let y = 4; y <= 11; y++) dot(g, 10, y, 6);       // bowstring
  box(g, 5, 7, 11, 7, 23);                              // arrow
  box(g, 5, 14, 6, 15, 5); box(g, 8, 14, 9, 15, 5);     // legs
  return flat(outlined(g));
}
function griffin() {
  const g = blank();
  // wings spread
  box(g, 0, 3, 4, 9, 6);  box(g, 11, 3, 15, 9, 6);
  box(g, 1, 4, 3, 8, 7);  box(g, 12, 4, 14, 8, 7);
  box(g, 5, 6, 10, 12, 25);                             // tawny body
  box(g, 6, 3, 9, 6, 23);                               // eagle head (pale)
  box(g, 9, 4, 11, 5, 9);                               // beak
  dot(g, 8, 4, 0);                                      // eye
  box(g, 5, 12, 6, 15, 25); box(g, 9, 12, 10, 15, 25);  // legs
  dot(g, 5, 15, 10); dot(g, 10, 15, 10);                // talons
  box(g, 6, 9, 9, 11, 28);                              // (magic) saddle/banner accent
  return flat(outlined(g));
}
function ogre() {
  const g = blank();
  box(g, 4, 4, 11, 12, 27);                             // big green body (will read as brute)
  box(g, 5, 2, 10, 5, 27);                              // head
  dot(g, 6, 3, 8); dot(g, 9, 3, 8);                     // red eyes
  box(g, 6, 4, 9, 4, 16);                               // brow
  box(g, 6, 5, 9, 5, 7);                                // tusks/teeth
  box(g, 3, 6, 4, 10, 27); box(g, 11, 6, 12, 10, 27);   // arms
  box(g, 1, 8, 4, 11, 5);                               // club shaft
  box(g, 0, 6, 3, 10, 6);                               // club head
  box(g, 5, 12, 7, 15, 27); box(g, 8, 12, 10, 15, 27);  // legs
  box(g, 5, 9, 10, 11, 28);                             // (magic) loincloth → team tint
  return flat(outlined(g));
}

module.exports = {
  screenW: 320, screenH: 200, scale: 4,
  cellW: 16, cellH: 16, mapW: 128, mapH: 64,
  sprites: {
    1: grass(), 2: forest(), 3: mountain(), 4: water(), 5: road(),
    6: goldPile(), 7: orePile(), 8: chest(), 9: mine(),
    10: town(), 11: hero(),
    12: battlefield(), 13: rock(),
    16: pikeman(), 17: archer(), 18: griffin(), 19: ogre(),
  },
};
