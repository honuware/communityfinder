import { inject, Injectable } from '@angular/core';
import { HONUWARE_API_BASE, PhotoUrlBuilder } from '@honuware/ui/access';
import type { HonuwareAccessMocks } from '@honuware/ui/testing';
import { MOCK_GALLERY, MOCK_PHOTO_OWNERS } from './events-mock-data';

// CM3's offline placeholder art: in the `local` (mock) build there is no
// server to scale a photo, so every events photo address becomes a picture
// drawn here instead. Generated shapes and color fields, never people
// (the photo policy holds even for demo art), and the same picture for the
// same row every time, so a thumbnail and its full view match.

// The tables whose photos the demo draws. The events pages only ask for a
// photo the payload says exists, and the editor's photo control only after
// the photo store says so, so these are drawn whatever the id (a gallery
// photo added offline included). A venue is drawn only when the demo gives
// it a photo, since the guide's venue page asks for every venue's. Others
// (a person's avatar) keep the real address, which fails offline as it
// always has.
const DEMO_TABLES = ['events', 'series', 'event_gallery_photos'];

function isDemoPhoto(tableName: string, id: number): boolean {
  return (
    DEMO_TABLES.includes(tableName) ||
    (tableName === 'venues' && MOCK_PHOTO_OWNERS.venues.includes(id))
  );
}

// The site's plum, coral, teal, gold and ink, plus a cream for light fields.
const PALETTE = ['#6b2d5c', '#ff6f59', '#2a9d8f', '#f4a261', '#264653', '#fdf6ec'];

// A small deterministic generator (mulberry32), seeded by the row.
function seededRandom(seed: number): () => number {
  let state = seed >>> 0;
  return () => {
    state = (state + 0x6d2b79f5) >>> 0;
    let t = state;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4_294_967_296;
  };
}

function seedFor(table: string, id: number): number {
  let hash = 2_166_136_261;
  for (const character of `${table}/${id}`) {
    hash = Math.imul(hash ^ character.charCodeAt(0), 16_777_619);
  }
  return hash;
}

// An SVG data: address, `width` × `height`, for the row's picture.
export function demoPhotoArt(table: string, id: number, width: number, height: number): string {
  const random = seededRandom(seedFor(table, id));
  const pick = () => PALETTE[Math.floor(random() * PALETTE.length)];
  const from = pick();
  let to = pick();
  if (to === from) {
    to = PALETTE[(PALETTE.indexOf(from) + 2) % PALETTE.length];
  }
  const shapes: string[] = [];
  const shapeCount = 3 + Math.floor(random() * 3);
  for (let index = 0; index < shapeCount; index++) {
    const x = Math.round(random() * width);
    const y = Math.round(random() * height);
    const size = Math.round((0.15 + random() * 0.35) * Math.min(width, height));
    const opacity = (0.35 + random() * 0.45).toFixed(2);
    shapes.push(
      random() < 0.5
        ? `<circle cx="${x}" cy="${y}" r="${size}" fill="${pick()}" fill-opacity="${opacity}"/>`
        : `<rect x="${x - size}" y="${y - size / 2}" width="${size * 2}" height="${size}" ` +
            `rx="${Math.round(size / 4)}" fill="${pick()}" fill-opacity="${opacity}" ` +
            `transform="rotate(${Math.round(random() * 90 - 45)} ${x} ${y})"/>`,
    );
  }
  const svg =
    `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" ` +
    `viewBox="0 0 ${width} ${height}">` +
    `<defs><linearGradient id="g" x1="0" y1="0" x2="1" y2="1">` +
    `<stop offset="0" stop-color="${from}"/><stop offset="1" stop-color="${to}"/>` +
    `</linearGradient></defs>` +
    `<rect width="${width}" height="${height}" fill="url(#g)"/>${shapes.join('')}</svg>`;
  // The trailing "#" makes anything appended a fragment: hw-photo-upload adds
  // "?t=<now>" after an upload, which would otherwise land inside the SVG.
  return `data:image/svg+xml,${encodeURIComponent(svg)}#`;
}

// The `local` build's PhotoUrlBuilder: demo art for the events tables, the
// real address for everything else.
@Injectable()
export class DemoPhotoUrlBuilder extends PhotoUrlBuilder {
  constructor() {
    super(inject(HONUWARE_API_BASE));
  }

  override scaledPhotoUrl(
    tableName: string,
    tableItemId: number | string,
    width: number,
    height: number,
  ): string {
    return isDemoPhoto(tableName, Number(tableItemId))
      ? demoPhotoArt(tableName, Number(tableItemId), width, height)
      : super.scaledPhotoUrl(tableName, tableItemId, width, height);
  }
}

// Tells the offline photo store which demo rows have a photo, so the curator
// editor's photo controls agree with the public pages (Leather Night has its
// own photo; its gallery's rows have theirs).
export function seedDemoPhotos(mocks: Pick<HonuwareAccessMocks, 'photo'>): void {
  const owners: Array<[string, number]> = [
    ...MOCK_PHOTO_OWNERS.events.map((id): [string, number] => ['events', id]),
    ...MOCK_PHOTO_OWNERS.series.map((id): [string, number] => ['series', id]),
    ...MOCK_PHOTO_OWNERS.venues.map((id): [string, number] => ['venues', id]),
    ...MOCK_GALLERY.map((row): [string, number] => ['event_gallery_photos', row.id]),
  ];
  for (const [table, id] of owners) {
    mocks.photo.uploadPhoto(table, id, new ArrayBuffer(0), 'svg').subscribe();
  }
}
