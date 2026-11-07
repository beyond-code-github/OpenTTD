#!/usr/bin/env python3
"""Download the full-size files the Internet Archive holds for the #openttdcoop wiki upload folder.

Usage: fetch_coop_archive.py OUT_DIR

Lists wiki.openttdcoop.org/images/ through the Wayback CDX API, skips MediaWiki thumbnails
(/images/thumb/...), and saves each remaining file (images, savegames) to OUT_DIR under its own name.
Writes OUT_DIR/manifest.tsv (name, timestamp, original URL, status). Safe to re-run: existing files are kept.
"""

import pathlib
import sys
import time
import urllib.parse
import urllib.request

CDX = ("http://web.archive.org/cdx/search/cdx?url=wiki.openttdcoop.org/images/&matchType=prefix"
	"&filter=statuscode:200&collapse=urlkey&fl=timestamp,original&limit=20000")


def fetch(url, attempts=4):
	for attempt in range(attempts):
		try:
			with urllib.request.urlopen(url, timeout=120) as response:
				return response.read()
		except Exception as e:  # The archive is flaky; back off and retry.
			if attempt == attempts - 1:
				raise
			time.sleep(5 * (attempt + 1))


def main():
	if len(sys.argv) != 2:
		sys.exit(__doc__)
	out = pathlib.Path(sys.argv[1])
	out.mkdir(parents=True, exist_ok=True)

	rows = [line.split(" ", 1) for line in fetch(CDX).decode().splitlines() if line.strip()]
	rows = [(ts, url) for ts, url in rows if "/images/thumb/" not in url and not url.endswith("/")]

	manifest = []
	for ts, url in rows:
		name = urllib.parse.unquote(url.rsplit("/", 1)[-1])
		target = out / name
		status = "kept"
		if not target.exists():
			try:
				target.write_bytes(fetch(f"https://web.archive.org/web/{ts}id_/{url}"))
				status = "ok"
			except Exception as e:
				status = f"failed: {e}"
		manifest.append(f"{name}\t{ts}\t{url}\t{status}")
		print(f"{status:8.8} {name}", flush=True)

	(out / "manifest.tsv").write_text("\n".join(manifest) + "\n", encoding="utf-8")
	failed = sum(1 for m in manifest if "\tfailed" in m)
	print(f"{len(rows)} files, {failed} failed -> {out}")


if __name__ == "__main__":
	main()
