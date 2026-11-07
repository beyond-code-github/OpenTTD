#!/usr/bin/env python3
"""Index the images used in the ABR posts and archived coop wiki pages, without viewing them.

Usage: index_images.py ABR_DIR ARC_DIR COOP_FILES_DIR OUT_MD

For every <img> in ABR_DIR/*.html and ARC_DIR/*.html, records the source page, the image name,
its alt/title text, the text just before and after it, and (if downloaded) its pixel size and
local path. Writes a Markdown index grouped by source page.
"""

import html
import pathlib
import re
import struct
import sys
import urllib.parse

CONTEXT_CHARS = 220


def image_size(path):
	"""Pixel size of a PNG, GIF or JPEG, or None."""
	try:
		data = path.read_bytes()
	except OSError:
		return None
	if data[:8] == b"\x89PNG\r\n\x1a\n":
		return struct.unpack(">II", data[16:24])
	if data[:6] in (b"GIF87a", b"GIF89a"):
		return struct.unpack("<HH", data[6:10])
	if data[:2] == b"\xff\xd8":
		i = 2
		while i + 9 < len(data):
			if data[i] != 0xFF:
				break
			marker, length = data[i + 1], struct.unpack(">H", data[i + 2:i + 4])[0]
			if 0xC0 <= marker <= 0xCF and marker not in (0xC4, 0xC8, 0xCC):
				h, w = struct.unpack(">HH", data[i + 5:i + 9])
				return w, h
			i += 2 + length
	return None


def plain(fragment):
	fragment = re.sub(r"<script.*?</script>|<style.*?</style>", " ", fragment, flags=re.S)
	fragment = re.sub(r"<[^>]+>", " ", fragment)
	return re.sub(r"\s+", " ", html.unescape(fragment)).strip()


def full_size_name(src):
	"""MediaWiki thumbnails (/thumb/a/ab/Name.png/140px-Name.png) map to Name.png."""
	path = urllib.parse.unquote(urllib.parse.urlparse(src).path)
	match = re.search(r"/thumb/[0-9a-f]/[0-9a-f]{2}/([^/]+)/", path)
	return match.group(1) if match else path.rsplit("/", 1)[-1]


def index_page(page_path, search_dirs):
	page = page_path.read_text(encoding="utf-8", errors="replace")
	entries = []
	for match in re.finditer(r"<img\b[^>]*>", page, re.I):
		tag = match.group(0)
		src = re.search(r'src="([^"]+)"', tag)
		if not src:
			continue
		name = full_size_name(src.group(1))
		if not re.search(r"\.(png|jpe?g|gif)$", name, re.I) or re.search(r"smil|icon|logo|avatar|gravatar|feed|button|wp-includes", src.group(1), re.I):
			continue
		alt = " / ".join(plain(m) for m in re.findall(r'(?:alt|title)="([^"]*)"', tag) if plain(m))
		before = plain(page[max(0, match.start() - 3000):match.start()])[-CONTEXT_CHARS:]
		after = plain(page[match.end():match.end() + 3000])[:CONTEXT_CHARS]
		local = next((d / name for d in search_dirs if (d / name).exists()), None)
		entries.append((name, alt, before, after, local))
	return entries


def main():
	if len(sys.argv) != 5:
		sys.exit(__doc__)
	abr, arc, coop, out = (pathlib.Path(a) for a in sys.argv[1:])
	search_dirs = [abr / "images", coop]
	lines = ["# Image index", "", "Built from the HTML around each image; images not viewed. Size and path shown when downloaded.", ""]
	total = 0
	seen = set()
	for page_path in sorted(abr.glob("*.html")) + sorted(arc.glob("*.html")):
		entries = [e for e in index_page(page_path, search_dirs) if (page_path.name, e[0]) not in seen]
		if not entries:
			continue
		lines += [f"## {page_path.stem}", ""]
		for name, alt, before, after, local in entries:
			seen.add((page_path.name, name))
			total += 1
			size = image_size(local) if local else None
			status = f"{size[0]}x{size[1]}, `{local}`" if size else ("downloaded" if local else "not downloaded")
			lines.append(f"- **{name}** ({status})")
			if alt:
				lines.append(f"  - Alt: {alt}")
			lines.append(f"  - Before: …{before}")
			lines.append(f"  - After: {after}…")
		lines.append("")
	lines.insert(3, f"{total} image references.")
	pathlib.Path(out).write_text("\n".join(lines) + "\n", encoding="utf-8")
	print(f"{total} image references -> {out}")


if __name__ == "__main__":
	main()
