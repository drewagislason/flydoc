# flydoc READFME.md

Version 1.0 released 08/31/2024  
Copyright (c) 2024, Drew Gislason  
License: [MIT](https://mit-license.org)

A command-line tool for creating source code documentation HTML from commented source code.

All doc comments are treated like markdown. Like markdown, flydoc itself is minimalist. It creates
static HTML web pages that can then be hosted anywhere. or it can create a combined markdown file
that can be easily turned into a .pdf by other tools.

flydoc encourages a single source of truth: that is, the comments and examples in your source code
are also the developer documentation for functions and methods. Lengthy API documentation,
tutorials and examples with graphics can go into markdown files that become part of the same
documentation and become part of your git check-in process.

flydoc is mobile first, that is, the HTML output looks great on a smart phone, tablet or desktop.
The HTML colors and images can be customized per project.

## Features

* Create developer API documentation from commented source code
* All comments treated as markdown for rich display in HTML
* Works with with most coding languages, C/C++, Java, Python, Rust, Javascript, etc...
* Supports all Unicode symbols, emoji and languages by using UTF-8 as encoding
* Control what is documented and what is not included (public APIs vs private functions/methods)
* Mobile first: HTML looks great on a phone, tablet or desktop
* Adjust colors and images for project/company customized look to HTML
* Output options include:
  1. A set of static HTML/CSS files with links and images
  2. A set of markdown files
  3. A single markdown file
* Predictable links for easy customized HTML
* Warnings are in standard `file:line:col: warning: text` format for easy parsing

See the docs folder, file flydoc_user_guide.md for more information.

Markdown files can be loaded into any editor.
