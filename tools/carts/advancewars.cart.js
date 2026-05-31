// Sprites for advancewars.c — Advance Wars demake "FIELD COMMANDER".
// Generated as flat index arrays so we can reach the extended palette and the
// "magic" team colors 28 (body) / 29 (dark) that pal() recolors per army.
//
// Slot layout (8-col sheet):
//   1 plains  2 forest  3 mountain  4 road  5 sea  6 shoal     (terrain, opaque)
//   7 city    8 factory 9 hq                                   (buildings, magic 28/29)
//  16 infantry 17 mech 18 recon 19 tank 20 artillery           (units, magic 28/29)

const S = 16;
const OUT = 16;   // outline (brownish-black, reads near-black, never 0)

const blank = () => Array.from({ length: S }, () => new Array(S).fill(0));
function box(g, x0, y0, x1, y1, c) {
  for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++)
    if (x >= 0 && x < S && y >= 0 && y < S) g[y][x] = c;
}
function dot(g, x, y, c) { if (x >= 0 && x < S && y >= 0 && y < S) g[y][x] = c; }
const flat = g => g.flat();

// add a 1px OUT outline into transparent pixels touching the silhouette
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

// ── terrain (fully opaque — no index 0, so nothing turns transparent) ──────────
function plains() {
  const g = blank(); box(g, 0, 0, 15, 15, 27);
  for (let y = 0; y < S; y++) for (let x = 0; x < S; x++) {
    const n = (x * 7 + y * 13) % 19;
    if (n === 0) g[y][x] = 11; else if (n === 6) g[y][x] = 3;
  }
  return flat(g);
}
function forest() {
  const g = blank(); box(g, 0, 0, 15, 15, 3);
  // three little trees: canopy + trunk
  const trees = [[3, 4], [10, 3], [7, 9]];
  for (const [tx, ty] of trees) {
    box(g, tx - 1, ty, tx + 2, ty + 2, 27);
    box(g, tx, ty - 1, tx + 1, ty - 1, 11);
    dot(g, tx, ty + 3, 4); dot(g, tx + 1, ty + 3, 4);
  }
  return flat(g);
}
function mountain() {
  const g = blank(); box(g, 0, 0, 15, 15, 3);   // grass skirt
  // grey massif
  for (let y = 3; y < 16; y++) { const w = Math.min(7, y - 1); box(g, 8 - w, y, 7 + w, y, 22); }
  box(g, 5, 4, 10, 6, 5);                          // shadow flank
  box(g, 6, 3, 9, 4, 6);                           // upper rock
  box(g, 7, 3, 8, 3, 7);                           // snow cap
  return flat(g);
}
function road() {
  const g = blank(); box(g, 0, 0, 15, 15, 4);    // brown earth shoulders
  box(g, 4, 0, 11, 15, 6);                         // light-grey roadway
  for (let y = 1; y < 16; y += 3) box(g, 7, y, 8, y + 1, 23);  // center dashes
  return flat(g);
}
function sea() {
  const g = blank(); box(g, 0, 0, 15, 15, 12);
  for (let y = 0; y < S; y++) for (let x = 0; x < S; x++) {
    const n = (x * 5 + y * 9) % 11;
    if (n === 0) g[y][x] = 28; else if (n === 4) g[y][x] = 1;
  }
  box(g, 2, 3, 5, 3, 7); box(g, 9, 9, 12, 9, 7);  // foam crests
  return flat(g);
}
function shoal() {
  const g = blank(); box(g, 0, 0, 15, 15, 15);
  for (let y = 0; y < S; y++) for (let x = 0; x < S; x++)
    if ((x * 3 + y * 7) % 13 === 0) g[y][x] = 9;
  box(g, 0, 13, 15, 15, 12);                       // a strip of shallow water
  return flat(g);
}

// ── buildings (transparent bg, body = magic 28, dark = magic 29, white = 7) ─────
function city() {
  const g = blank();
  box(g, 2, 6, 7, 14, 28); box(g, 8, 8, 13, 14, 28);   // two houses
  box(g, 2, 4, 7, 5, 29);  box(g, 8, 6, 13, 7, 29);    // roofs
  box(g, 4, 8, 5, 9, 7);   box(g, 10, 10, 11, 11, 7);  // windows
  box(g, 4, 12, 5, 14, 29);                            // door
  return flat(outlined(g));
}
function factory() {
  const g = blank();
  box(g, 2, 6, 13, 14, 28);             // body
  box(g, 2, 4, 13, 5, 29);              // roof
  box(g, 10, 1, 11, 5, 29);             // chimney
  dot(g, 10, 0, 6); dot(g, 11, -1, 6);  // (smoke clipped — fine)
  box(g, 5, 9, 10, 14, 29);             // big door
  box(g, 6, 10, 9, 13, 5);             // door shadow
  box(g, 3, 7, 4, 8, 7);               // window
  return flat(outlined(g));
}
function hq() {
  const g = blank();
  box(g, 4, 8, 11, 14, 28);            // base
  box(g, 6, 3, 9, 8, 28);              // tower
  box(g, 4, 7, 11, 7, 29);             // ledge
  box(g, 7, 5, 8, 6, 7);              // tower window
  box(g, 10, 1, 10, 6, 5);            // flag pole
  box(g, 11, 1, 14, 3, 29);           // flag
  return flat(outlined(g));
}

// ── units (transparent; body magic 28, dark magic 29, grey 5, skin 15) ─────────
function infantry() {
  const g = blank();
  box(g, 5, 3, 10, 4, 28);   // helmet
  box(g, 6, 5, 9, 7, 15);    // face
  box(g, 6, 8, 9, 11, 28);   // torso
  box(g, 5, 8, 5, 10, 28); box(g, 10, 8, 10, 10, 28);  // arms
  box(g, 10, 6, 13, 7, 5);   // rifle
  box(g, 6, 12, 7, 14, 5); box(g, 8, 12, 9, 14, 5);    // legs
  return flat(outlined(g));
}
function mech() {
  const g = blank();
  box(g, 5, 3, 10, 4, 28);   // helmet
  box(g, 6, 5, 9, 7, 15);    // face
  box(g, 5, 8, 10, 12, 28);  // bulky torso
  box(g, 2, 6, 9, 8, 5);     // bazooka tube
  dot(g, 2, 5, 9);           // muzzle
  box(g, 6, 13, 7, 15, 5); box(g, 8, 13, 9, 15, 5);    // legs
  return flat(outlined(g));
}
function recon() {
  const g = blank();
  box(g, 2, 7, 13, 11, 28);  // body
  box(g, 5, 5, 10, 7, 12);   // windshield/cab
  box(g, 2, 11, 13, 11, 29); // chassis line
  box(g, 3, 12, 5, 13, 5); box(g, 10, 12, 12, 13, 5);  // wheels
  dot(g, 3, 12, OUT); dot(g, 10, 12, OUT);
  dot(g, 13, 8, 9);          // headlight
  return flat(outlined(g));
}
function tank() {
  const g = blank();
  box(g, 2, 11, 13, 13, 5);  // treads
  box(g, 3, 8, 12, 11, 28);  // hull
  box(g, 5, 5, 10, 8, 29);   // turret
  box(g, 10, 6, 15, 6, 5);   // barrel
  dot(g, 7, 6, 7);           // hatch glint
  return flat(outlined(g));
}
function arty() {
  const g = blank();
  box(g, 2, 11, 13, 13, 5);  // treads
  box(g, 3, 8, 11, 11, 28);  // body
  box(g, 4, 6, 7, 9, 29);    // cab
  // long raised barrel (diagonal)
  const bar = [[7, 8], [8, 7], [9, 6], [10, 5], [11, 4], [12, 3], [13, 2], [14, 1]];
  for (const [bx, by] of bar) { dot(g, bx, by, 5); dot(g, bx, by + 1, 5); }
  return flat(outlined(g));
}

module.exports = {
  screenW: 320, screenH: 200, scale: 4,
  cellW: 16, cellH: 16, mapW: 128, mapH: 64,
  sprites: {
    1: plains(),  2: forest(),  3: mountain(), 4: road(), 5: sea(), 6: shoal(),
    7: city(),    8: factory(), 9: hq(),
    16: infantry(), 17: mech(), 18: recon(), 19: tank(), 20: arty(),
  },
};
