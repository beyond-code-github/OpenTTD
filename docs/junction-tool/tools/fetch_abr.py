#!/usr/bin/env python3
"""Download V453000's "Advanced Building Revue" (ABR) posts from the #openttdcoop blog via the Internet Archive.

Usage: fetch_abr.py OUT_DIR

Finds the ABR posts and blog images through the Wayback CDX API, saves each post's HTML, a plain-text
extraction of the article (OUT_DIR/NN-slug.txt, image references kept as [image: NAME]), and every
image under /files/blog/V453000/ (OUT_DIR/images/). Safe to re-run: existing files are kept.
"""

import html
import pathlib
import re
import sys
import time
import urllib.request

CDX = "http://web.archive.org/cdx/search/cdx?url={}&matchType=prefix&filter=statuscode:200&collapse=urlkey&fl=timestamp,original&limit=20000"
POST_RE = re.compile(r"^https?://blog\.openttdcoop\.org(?::80)?/(\d{4}/\d{2}/\d{2})/(advanced-building-revue-(\d+)[a-z0-9-]*)/?$")


def fetch(url, attempts=5):
	for attempt in range(attempts):
		try:
			with urllib.request.urlopen(url, timeout=120) as response:
				data = response.read()
			if b"Temporarily Offline" in data[:2000]:
				raise IOError("archive temporarily offline")
			return data
		except Exception:  # The archive is flaky; back off and retry.
			if attempt == attempts - 1:
				raise
			time.sleep(10 * (attempt + 1))


def cdx(prefix):
	rows = fetch(CDX.format(prefix)).decode().splitlines()
	return [row.split(" ", 1) for row in rows if row.strip()]


def article_text(page):
	"""Crude extraction of the post body: keep paragraphs, headings and image names."""
	match = re.search(r'<div class="(?:entry|post-content|entry-content)[^"]*">(.*?)<div class="(?:postmetadata|post-footer|navigation|entry-utility)', page, re.S)
	body = match.group(1) if match else page
	body = re.sub(r"<img[^>]*src=\"[^\"]*/([^/\"]+)\"[^>]*>", r"\n[image: \1]\n", body)
	body = re.sub(r"<(br|/p|/h\d|/li)[^>]*>", "\n", body)
	body = re.sub(r"<script.*?</script>|<style.*?</style>", "", body, flags=re.S)
	body = re.sub(r"<[^>]+>", "", body)
	lines = [html.unescape(line).strip() for line in body.splitlines()]
	return "\n".join(line for line in lines if line) + "\n"


def main():
	if len(sys.argv) != 2:
		sys.exit(__doc__)
	out = pathlib.Path(sys.argv[1])
	(out / "images").mkdir(parents=True, exist_ok=True)

	posts = {}
	for ts, url in cdx("blog.openttdcoop.org/"):
		match = POST_RE.match(url)
		if match:
			posts.setdefault(match.group(2), (ts, url, int(match.group(3))))

	for slug, (ts, url, number) in sorted(posts.items(), key=lambda item: item[1][2]):
		target = out / f"{number:02d}-{slug}.txt"
		if target.exists():
			continue
		try:
			page = fetch(f"https://web.archive.org/web/{ts}id_/{url}").decode("utf-8", errors="replace")
		except Exception as e:
			print(f"failed  {slug}: {e}", flush=True)
			continue
		(out / f"{number:02d}-{slug}.html").write_text(page, encoding="utf-8")
		target.write_text(f"Source: {url} (archived {ts})\n\n" + article_text(page), encoding="utf-8")
		print(f"post    {slug}", flush=True)

	for ts, url in cdx("blog.openttdcoop.org/files/blog/V453000/"):
		name = url.rsplit("/", 1)[-1]
		target = out / "images" / name
		if target.exists() or not re.search(r"\.(png|jpe?g|gif)$", name, re.I):
			continue
		try:
			target.write_bytes(fetch(f"https://web.archive.org/web/{ts}id_/{url}"))
			print(f"image   {name}", flush=True)
		except Exception as e:
			print(f"failed  {name}: {e}", flush=True)

	print(f"{len(posts)} posts -> {out}")


if __name__ == "__main__":
	main()
