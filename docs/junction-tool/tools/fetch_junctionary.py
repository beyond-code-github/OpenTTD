#!/usr/bin/env python3
"""Mirror the OpenTTD wiki Junctionary (pages and images) from the OpenTTD/wiki-data repository.

Usage: fetch_junctionary.py OUT_DIR

Writes OUT_DIR/pages/*.mediawiki, OUT_DIR/images/*, and OUT_DIR/index.md listing each page
with its categories, the images it uses and the first lines of its text.
"""

import json
import pathlib
import re
import sys
import urllib.parse
import urllib.request

REPO = "OpenTTD/wiki-data"
BRANCH = "main"
PAGE_PREFIX = "Page/en/Community/Junctionary/"
FILE_PREFIX = "File/en/Community/Junctionary/"


def fetch(url):
	with urllib.request.urlopen(url, timeout=60) as response:
		return response.read()


def raw_url(path):
	return f"https://raw.githubusercontent.com/{REPO}/{BRANCH}/{urllib.parse.quote(path)}"


def summarise(text):
	"""First few lines of prose, with markup stripped."""
	lines = []
	for line in text.splitlines():
		line = line.strip()
		if not line or line.startswith(("{{", "[[Category", "[[File", "[[Image", "|", "{|", "|}", "==")):
			continue
		line = re.sub(r"\[\[(?:[^|\]]*\|)?([^\]]*)\]\]", r"\1", line)
		line = re.sub(r"'{2,}", "", line)
		lines.append(line)
		if len(lines) == 3:
			break
	return " ".join(lines)


def main():
	if len(sys.argv) != 2:
		sys.exit(__doc__)
	out = pathlib.Path(sys.argv[1])
	(out / "pages").mkdir(parents=True, exist_ok=True)
	(out / "images").mkdir(parents=True, exist_ok=True)

	tree = json.loads(fetch(f"https://api.github.com/repos/{REPO}/git/trees/{BRANCH}?recursive=1"))
	if tree.get("truncated"):
		print("warning: tree listing truncated", file=sys.stderr)
	paths = [entry["path"] for entry in tree["tree"] if entry["type"] == "blob"]

	pages = sorted(p for p in paths if p.startswith(PAGE_PREFIX) and p.endswith(".mediawiki"))
	images = sorted(p for p in paths if p.startswith(FILE_PREFIX) and not p.endswith(".mediawiki"))

	for path in images:
		target = out / "images" / path[len(FILE_PREFIX):]
		if not target.exists():
			target.write_bytes(fetch(raw_url(path)))

	index = ["# Junctionary mirror", "", f"Source: https://github.com/{REPO} ({len(pages)} pages, {len(images)} images).", ""]
	for path in pages:
		name = path[len(PAGE_PREFIX):-len(".mediawiki")]
		text = fetch(raw_url(path)).decode("utf-8", errors="replace")
		(out / "pages" / f"{name}.mediawiki").write_text(text, encoding="utf-8")
		categories = re.findall(r"\[\[Category:([^\]|]+)", text)
		used = re.findall(r"\[\[(?:File|Image):(?:en/Community/Junctionary/)?([^\]|]+)", text)
		index.append(f"## {name}")
		index.append(f"- Page: https://wiki.openttd.org/en/Community/Junctionary/{urllib.parse.quote(name)}")
		if categories:
			index.append(f"- Categories: {', '.join(c.strip() for c in categories)}")
		if used:
			index.append(f"- Images: {', '.join(u.strip() for u in used)}")
		index.append(f"- Summary: {summarise(text)}")
		index.append("")

	(out / "index.md").write_text("\n".join(index), encoding="utf-8")
	print(f"{len(pages)} pages, {len(images)} images -> {out}")


if __name__ == "__main__":
	main()
