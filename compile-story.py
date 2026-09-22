#!/usr/bin/env python3
"""
compile-story.py: compiles a Twee-flavored branching-narrative script into the
binary format Story.hpp / Story.cpp load at runtime.

This is the asset pipeline for this game's text: you write passages in plain
text (easy to read, easy to diff, no special editor needed), and this script
turns them into a compact binary blob so the game engine only ever has to
follow indices -- it never parses story text itself.

Source syntax (a small subset of Twee 3, https://github.com/iftechfoundation/twine-specs):

    :: PassageName
    Passage body text, which can span
    multiple lines and blank lines.

    [[Choice text->TargetPassage]]
    [[TargetPassage]]              (choice text defaults to the target name)

A passage with no [[...]] links becomes an ending (shown with no choices).
The passage named 'Start' becomes the story's entry point and is always
compiled to index 0; if no passage is named 'Start', the first passage in
the file is used instead.

Usage:
    python compile-story.py <story.twee> <output.story>
"""
import re
import struct
import sys

PASSAGE_HEADER_RE = re.compile(r'^::\s*(.+?)\s*(?:\[.*\])?\s*$') # ":: Name [optional tags]"
LINK_RE = re.compile(r'\[\[(.*?)\]\]')

def parse_passages(text):
	"""Split the source into a list of (name, raw_body) pairs, in file order."""
	passages = []
	name = None
	body_lines = []
	def flush():
		if name is not None:
			passages.append((name, "\n".join(body_lines).strip()))
	for line in text.splitlines():
		m = PASSAGE_HEADER_RE.match(line)
		if m:
			flush()
			name = m.group(1)
			body_lines = []
		else:
			body_lines.append(line)
	flush()
	return passages

def extract_links(body):
	"""Strip [[...]] links out of a passage body; return (clean_text, [(choice_text, target_name), ...])."""
	choices = []
	def repl(m):
		inner = m.group(1)
		if '->' in inner:
			text, target = inner.split('->', 1)
		elif '<-' in inner:
			target, text = inner.split('<-', 1)
		elif '|' in inner:
			text, target = inner.split('|', 1)
		else:
			text = target = inner
		choices.append((text.strip(), target.strip()))
		return ''
	clean = LINK_RE.sub(repl, body)
	clean = "\n".join(line.rstrip() for line in clean.splitlines())
	clean = re.sub(r'\n{3,}', '\n\n', clean).strip()
	return clean, choices

def write_chunk(f, magic, data_bytes):
	assert len(magic) == 4
	f.write(magic.encode('ascii'))
	f.write(struct.pack('<I', len(data_bytes)))
	f.write(data_bytes)

def main():
	if len(sys.argv) != 3:
		sys.exit("Usage: python compile-story.py <story.twee> <output.story>")
	src_path, out_path = sys.argv[1], sys.argv[2]

	with open(src_path, 'r', encoding='utf-8') as f:
		raw_passages = parse_passages(f.read())

	if not raw_passages:
		sys.exit(f"error: no passages ('::' Name) found in {src_path}")

	names = [name for name, _ in raw_passages]
	dupes = {n for n in names if names.count(n) > 1}
	if dupes:
		sys.exit(f"error: duplicate passage name(s) in {src_path}: {', '.join(sorted(dupes))}")

	#'Start' (if present) always compiles to index 0, since that's the story's entry point:
	if 'Start' in names:
		start_i = names.index('Start')
		order = [start_i] + [i for i in range(len(raw_passages)) if i != start_i]
	else:
		order = list(range(len(raw_passages)))
	ordered = [raw_passages[i] for i in order]
	name_to_index = {name: i for i, (name, _) in enumerate(ordered)}

	strings = bytearray()
	passage_records = [] # (text_begin, text_end, choices_begin, choices_end)
	choice_records = [] # (text_begin, text_end, target)

	for name, body in ordered:
		clean_text, links = extract_links(body)

		text_begin = len(strings)
		strings += clean_text.encode('utf-8')
		text_end = len(strings)

		choices_begin = len(choice_records)
		for choice_text, target_name in links:
			if target_name not in name_to_index:
				sys.exit(f"error: passage '{name}' links to unknown passage '{target_name}'")
			c_begin = len(strings)
			strings += choice_text.encode('utf-8')
			c_end = len(strings)
			choice_records.append((c_begin, c_end, name_to_index[target_name]))
		choices_end = len(choice_records)

		passage_records.append((text_begin, text_end, choices_begin, choices_end))

	with open(out_path, 'wb') as f:
		write_chunk(f, 'str0', bytes(strings))
		write_chunk(f, 'pass', b''.join(struct.pack('<IIII', *r) for r in passage_records))
		write_chunk(f, 'chce', b''.join(struct.pack('<III', *r) for r in choice_records))

	endings = sum(1 for _, _, cb, ce in passage_records if cb == ce)
	print(f"wrote {out_path}: {len(ordered)} passage(s), {len(choice_records)} choice(s), {endings} ending(s), {len(strings)} byte(s) of text.")

if __name__ == '__main__':
	main()
