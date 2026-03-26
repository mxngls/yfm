# TAY

## This Ain't YAML

In the realm of static site generators the question of how to couple
individual posts with associated metadata has many different answers.

Maintaining a balance between ease of use — the user's perspective — and
maintainability — the developer's perspective — proves to be a constant
source of friction. Several possible answers present themselves, each
with their own strengths and weaknesses. The most common approach is to
either store metadata indirectly through some form of database or
encode it directly in the posts themselves.

The latter seems to be the most common approach encountered throughout
different ecosystems and is something commonly called YAML frontmatter.
YAML is a complicated beast though, and when even someone like Rui,
who works on parsers for a living, [says
so](https://x.com/rui314/status/1449936401798033409) it must be true.

Supporting the full YAML specification for something as simple
as storing a couple of hundred bytes encoded as key-value pairs seems
rather wasteful.

Two other approaches present themselves. First,
implementing a simpler custom serialization language. Devine did this
with both [Tablatal and
Indental](https://wiki.xxiivv.com/site/libraries.html). Another possible
way is to limit oneself to just the elements of an existing spec that
are needed to productively support intended use cases. Apple did this
with [`.tbd`](https://developer.apple.com/forums/thread/715385) files
which aim to provide a <q>compact description of the contents of a dynamic
library</q>.

TAY aims to be a subset of YAML that selects only those parts of the
spec that work towards our stated goal above.

---

## Format Specification

Every TAY document is a valid
[YAML 1.2.2](https://yaml.org/spec/1.2.2/) document, but not the other
way around. The following sections define what is supported and where
the tokenizer intentionally deviates from the full spec.

### Document Markers

`---` marks the start of a document, `...` marks the end.

```yaml
---
title: "My Document"
...
```

Directives (`%YAML`, `%TAG`) before `---` are not recognized
([§6.8](https://yaml.org/spec/1.2.2/#68-directives)).

Spec references:
[§9.1.1](https://yaml.org/spec/1.2.2/#911-document-start-marker),
[§9.1.2](https://yaml.org/spec/1.2.2/#912-document-end-marker)

### Block Mappings

Key-value pairs where keys are plain (unquoted) identifiers followed
by `: ` and a value. Nested mappings are supported through
indentation.

```yaml
title: "My Application"
created_at: "2024-03-15 10:00:00"
site:
  url: http://example.com
  name: "Example"
```

Spec reference:
[§8.2.2](https://yaml.org/spec/1.2.2/#822-block-mappings)

### Block Sequences

List items use `- ` (dash followed by a space). Each item can contain
a value or a group of key-value pairs at an indented level.

```yaml
authors:
  - name: "Alice"
    email: "alice@example.com"
  - name: "Bob"
    email: "bob@example.com"
```

The dash must be followed by a space and content on the same line.
Nested sequences are not supported.

Spec reference:
[§8.2.1](https://yaml.org/spec/1.2.2/#821-block-sequences)

### Double-Quoted Scalars

String values are double-quoted. Backslash escapes (e.g. `\"`, `\\`)
are recognized so that escaped quotes do not end the string, but
escape sequences like `\n`, `\t`, and `\uXXXX` are not expanded —
the raw content between quotes is preserved. Single-quoted scalars
([§7.3.2](https://yaml.org/spec/1.2.2/#732-single-quoted-style))
are not supported.

```yaml
title: "Hello \"World\""
```

Spec reference:
[§7.3.1](https://yaml.org/spec/1.2.2/#731-double-quoted-style)

### Plain Scalars

Bare (unquoted) strings are supported as both keys and values. A
plain scalar terminates at `: `, `[`, `"`, `|`, or newline. The
colon and hash characters only act as terminators when followed or
preceded by whitespace respectively, consistent with the spec.

```yaml
url: http://example.com
color: #ff0000
```

The full spec's plain scalar rules are significantly more complex,
involving context-dependent flow indicators and multi-line folding
([§7.3.3](https://yaml.org/spec/1.2.2/#733-plain-style)). TAY uses
a simplified character-level scan.

Spec reference:
[§7.3.3](https://yaml.org/spec/1.2.2/#733-plain-style)

### Literal Block Scalars

Multiline values use the `|` indicator. The block ends when
indentation returns to or below the parent level.

```yaml
description: |
  This is a multiline string.
  Line breaks are preserved.

  Blank lines work too.
```

Chomping modifiers (`|-`, `|+`), explicit indentation indicators
(`|2`), and folded block scalars (`>`,
[§8.1.3](https://yaml.org/spec/1.2.2/#813-folded-style)) are not
supported.

Spec reference:
[§8.1.2](https://yaml.org/spec/1.2.2/#812-literal-style)

### Flow Sequences

Inline lists of double-quoted strings are supported. Nested sequences
and flow mappings are not.

```yaml
tags: ["programming", "c", "yaml"]
```

Spec reference:
[§7.4.1](https://yaml.org/spec/1.2.2/#741-flow-sequences)

### Comments

Full-line and inline comments are supported. A `#` preceded by
whitespace (or at the start of a line) begins a comment that runs to
the end of the line.

```yaml
# This is a comment
title: "My Document"  # Inline comment
```

Spec reference:
[§6.6](https://yaml.org/spec/1.2.2/#66-comments)

---

## Example

```yaml
---
# Post metadata
title: "My Site"
created_at: "2024-03-15 10:00:00"
updated_at: "2024-03-15 14:30:00"
tags: ["web", "static-site"]
authors:
  - name: "Alice"
    email: "alice@example.com"
  - name: "Bob"
    email: "bob@example.com"
description: |
  This is a sample document demonstrating
  all supported features.

  It includes multiline text blocks.
...
```

---

## Implementation Limits

- Maximum 512 tokens per document
- Maximum 16 indentation levels
- Maximum 512 bytes input file size

---

## References

- [YAML Ain't Markup Language (YAML™) Version 1.2 — Revision 1.2.2 (2021-10-01)](https://yaml.org/spec/1.2.2/)
- [Rui Ueyama's YAML tokenizer (sold)](https://github.com/bluewhalesystems/sold/blob/main/macho/yaml.cc)
