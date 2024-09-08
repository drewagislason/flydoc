# FlyDoc User Guide

Version 1.0 released 08/31/2024  
Copyright (c) 2024, Drew Gislason  
License: [MIT](https://mit-license.org)

A command-line tool for creating source code documentation HTML from commented source code.

## 1 - Features

* Create developer API documentation from commented source code
* All comments treated as markdown for rich display in HTML
* Works with with most coding languages, C/C++, Java, Python, Go, Rust, Javascript, etc...
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

All comments are treated as markdown, so tables, images, links, bulleted lists and other markdown
features can be used in your source code documentation.

flydoc works with a variety of coding languages, including: C, C++, C#, Go, Java, Javascript,
Python, Ruby, Rust and Swift. Basically any language that uses `#` or `/*` or `//` for comments.

flydoc uses @keywords in special comments to control the look and content of the documentation. If
you are familiar with Doxygen, flydoc is (mostly) a subset, but sometimes with slightly different
parameters. So... not Doxygen compatible, but not difficult to convert.

If creating markdown output from source code documentation, the file can can easily be turned into
a .pdf using tools like Marked 2, MacDown or Typora.

For an example of flydoc output, see: <https://drewagislason.github.io/index.html>  
For markdown usage, see: <https://www.markdownguide.org/cheat-sheet/>  

flydoc is named after [Firefly](https://en.wikipedia.org/wiki/Firefly_(TV_series), an iconic space western.

### 1.2 - Usage

flydoc runs in the command-line terminal. Below is `flydoc --help`:

```
flydoc v1.0

Usage = flydoc [-n] [-o out/] [-s] [-v] [--exts .c.js] [--local] [--markdown] [--noindex] in...

Options:
-n             Parse inputs only, no output, useful to check for warnings
-o             Output folder/
-s             Sort modules/functions/classes/methods: -s- (off), -s (on: default)
-v[=#]         Verbosity: -v- (none), -v=1 (some), -v=2 (more: default)
--exts         List of file exts to search. Default: ".c.c++.cc.cpp.cxx.cs.go.java.js.py.rs.swift.ts"
--local        Create local w3.css file rather than remote link to w3.css
--markdown     Create a single combine markdown file rather than HTML pages
--noindex      Don't create index.html (mainpage). Allows for custom main page
--slug         Create a reference id (slug) from a string
--user-guide   Print flydoc user guide to the screen
in...          Input files and folders
```

The "in..." arguments are input files, folders and file masks as shown in the examples below:

```
$ flydoc foo.c -o .
$ flydoc --markdown -o foo/ ../myproject/
$ flydoc -o ../api_html/ src_py/ src_rust/ src_c/
$ flydoc -n my_python_project/*.py
```

Options and arguments can be placed in any order. For example, the following are equivalent:

```
$ flydoc --local -o outfolder/ ../project/src/ ../project/lib/
$ flydoc ../project/src/ -o outfolder/ ../project/lib/ --local
```

### 1.3 - Input Files

flydoc looks for 3 types of inputs files:

* Source files (as defined by `--exts` option) with special doc comments
* Markdown files (as defined by file extensions `.markdown.md.mdown`)
* Image files (as defined by file extensions `.jpg.jpeg.png.gif`)

Any images referred to in the source or markdown files by name only (no URL or path) will be copied
to the output folder to receive the markdown or HTML. A warning will be issued if that image can't
be found in the list of input files.

### 1.4 - Output Files

The output folder produced by flydoc is a flat folder full of all the files necessary to display
the static HTML pages or render the markdown. For example:

```
Bar.html
Baz.html
Foo.html
```

If using the option `--markdown`, then the output would be markdown files with headings:

```
Bar.md
Baz.md
Foo.md
```

If using the option `--combine`, then the output would be a markdown file:

```
foo.md
```

### 1.5 - Options

The `-n` option allows you to see what files would be processed without processing them and issues
any warnings found during that processing such as "missing graphic".

The `-o` option is mandatory unless `-n` is used. It specifies the output file or folder. The
output must be a folder if the `--html` option is specified otherwise it must be a markdown file,
e.g. "my_markdown.md".

The `-s` option sorts modules, classes, functions, methods and examples.

The `-v` verbose option on by default and lists statistics on how many modules and functions,
classes, methods, documents, examples and images were found. Set verbose to 2 `-v=2` to also see
the list of files processed. Set verbose to off with `-v-` to have no screen output except for
warnings.

The `--combine` option instructs flydoc to combine all documentation into a single markdown file.
This automatically turns on the `--markdown` option.

The `--exts` option let you change the default list of file extensions to search for. The default
is ".c.c++.cc.cpp.cxx.cs.go.java.js.py.rs.swift.ts.".

The `--markdown` option indicates the output shall be HTML (.html) in a folder, and not markdown
(.md) in a file.

The `--local` option is only useful for HTML output, as it creates a local copy of `w3.css` so that
no internet access is required to load the HTML pages.

## 2 - Building flydoc

flydoc is a command-line program written in C. It relies on the firefly C library (flylibc).

1. Install a C compiler (WSL 2 or minGW on Windows, XCode on macOS, gcc on Linux)
2. Install git
3. Optionally install make

To verify these tools are installed properly, open a terminal window and type:

```
$ cc --version
$ git --version
$ make --version  # optional
```

Next, chose a file folder where you will git clone flymake and flylibc.

Open a terminal. Run the following command-line programs/scripts:

```bash
$ git clone git@github.com:drewagislason/flylibc.git
$ git clone git@github.com:drewagislason/flydoc.git
$ cd flydoc/src
$ make -B # or ./make.sh if make is not installed
```

If you already have flymake installed <https://github.com/drewagislason/flymake>, a simpler way to build flydoc is:

```bash
$ git clone git@github.com:drewagislason/flydoc.git
$ flymake flydoc
```

One way to make flymake accessible to all your projects is to keep it in a folder in your `$PATH`.

```
mkdir ~/bin/
# edit ~/.zshrc or ~/.bashrc and add the following line
export PATH=$PATH:~/bin
# copy flymake with priveleges for all users
sudo cp flymake ~/bin/
```

If you are new to C, git or zsh or bash, consider the following links:

Git: <https://www.atlassian.com/git>  
Bash: <https://linuxhandbook.com/bash/>  
C: <https://www.w3schools.com/c/index.php>  

You might also be interested in some related projects:

flydoc : <https://github.com/drewagislason/flydoc>  
flylibc: <https://github.com/drewagislason/flylibc>  
flymake: <https://github.com/drewagislason/flymake>  
ned    : <https://github.com/drewagislason/ned>  
FireFly C Library documentation: <https://drewagislason.github.io>  

## 3 - flydoc Basics

flydoc looks for @keywords inside of special comments. The special comments are a slight addition
to standard comments you probably already have in your source code. The contents of the comments
are expected to be markdown in format.

The source code documentation is gathered into modules, classes, documents and examples. Each
module gets its own HTML file if the `--html` option is used, or it's own chapter in a markdown
file HTML output is not selected.

The results are pages (or chapters) of functions organized by class or module. Modules and classes
can span multiple source files. Module and class overall documentation can be in markdown files.

### 3.1 - Convert Source Comments to HTML

This example demonstrates converting source code comments to HTML. 

The command-line below generates an HTML file called math_circle.html (because of the @defgroup
name) that can be opened in any browser.

```
$ flydoc --html -o . math_circle.c  # created file math_circle.html can be opened by a browser
```

The `math_circle.c` file contains flydoc style comments:

```c
/* file: math_circle.c */

/*!
  @defgroup math_circle   A math library to eventually rival MatLab

  A few circle formulas
*/
 #define PI 3.1415926

/*!
  Calculate circumference of a circle = 2 pi r
  @param  r   radius
  @return circumference of the circle
*/
double math_circle_circumference(double r)
{
  return 2.0 * PI * r;
}

/*!
  Calculate volume of a sphere = 4/3 pi r^3
  @param  r   radius
  @return volume of the sphere
*/
double math_sphere_volume(double r)
{
  return (4.0 / 3.0) * PI * (r * r * r);
}

/*!
  Calculate surface area of a sphere = 4 pi r^2
  @param  r   radius
  @return surface area of the sphere
*/
double math_sphere_surface_area(double r)
{
  return 4.0 * PI * (r * r);
}
```

### 3.2 - Combine Markdown Files

This example demonstrates combining multiple markdown files into a single markdown file.

```
$ flydoc -o folder/ *.md
$ cat folder/
```

input file1.md:

```
# File1 Title

intro...

## File1.Topic1

text...
```

input file2.md:

```
# File2 Title

This is File2. It describes Foo and Bar

## Foo Topic1

foo text...

## Bar Topic2

bar text...
```

### 3.3 - Document Full Project

This Example includes Modules, Classes, Examples and Markdown Documents. It assumes you have git
installed and a GitHub account.

```
$ git clone git@github.com:drewagislason/flylibc.git
$ flydoc --html --local -o flylibc_docs/ flylibc/
$ ls flylibc_docs/   # flylibc_docs/index.html can be opened by a browser
```

You can see the results of flydoc online at <https://drewagislason.github.io/index.html>.

### 3.4 - Including Images In a Document

You can include images anywhere in a flydoc document. If you include the image without a URL path
and include the image in the input file list, then the image will be copied to the output folder.

For example:

    Hello ![world](world.png).

Would bring look for world.png in the input files and copy it if found and print a warning if not found.

The following would not look for the image as it has a URL:

    Hello ![world](https://pluspng.com/img-png/free-png-hd-world-globe-download-2400.png)

The optional "title" in an image can also be used for formatting the image. Examples are below.

      Option 1: ![Simple](image.png)  
      Option 2: ![Circle Image](image.png "w3-circle")  
      Option 3: ![More Control](image.png "class=\"w3-round\" style=\"width:80%\"")  
      Option 4: ![With Title](image.png "just some title")  

See also <https://www.w3schools.com/w3css/w3css_images.asp>

## 4 - Keywords

The flydoc @keywords are as follows:

Keyword    | Usage Parameters   | Description
---------- | ------------------ | -----------
 @class    | name description   | define a class
 @color    | sidebar {titlebar} | define color scheme for HTML output
 @defgroup | name description   | define a group (module)
 @example  | title              | a markdown code block must follow
 @font     | body {headings}    | define fonts for body and optionally headings
 @fn       | prototype          | document a function
 @inclass  | name               | all that follows is in this class
 @ingroup  | name               | all that follows is in this module
 @logo     | link               | define logo img for whole project (e.g. file.png)
 @mainpage | title              | mainpage with title
 @param    | name description   | function/method parameter
 @return   | description        | function/method return value(s), also @returns
 @version  | version            | version string for the whole project

The @keywords must be flush left on a given line. All parameters must be on that same line.

Most @keywords are removed from the final text. The exceptions are @param and @return.

In the Usage Parameters column above, any "name" must be a C-like name, as it's the name of a
module, class, method, function or variable, for example: MyModule or my_class2.

The keywords @class, @defgroup, @mainpage define a section. All other keywords are only processed
within the context of one of those sections. Sections end up as HTML files or become level 2
headings, e.g. "## heading", within a combined markdown file, depending on output options.

Note: A group, also called a "module", is simply a collection of functions under a single name.

Keywords @color, @font, @log and @version affect the style of the HTML and are applied to the
section. Sections without their own styles inherit from the @mainpage.

### 4.1 - Keyword @class

Usage = `@class name description`

The @class keyword defines a class, which has a collection of methods. Its parameters are a C-like
name and a single line text description, followed by optional markdown text.

Example:

```
/*!
  @class FlyWin  A full-color, overlapping windowing class for ANSI terminals

  Text and perhaps examples here to describe the class in detail...
*/
```

Add @color, @font, @logo or @version, to further customize the class. Otherwise it will inherit
these traits from the @mainpage.

Rather than including the @class in a flydoc comment in a source code file, you can instead include
a markdown file with the same name as the class:

file FlyWin.md:

```
@class FlyWin  A full-color, overlapping windowing class for ANSI terminals

Text and perhaps examples here to describe the class in detail...
```

### 4.2 - Keyword @color

Usage = `@color sidebar {titlebar}`

You can customize the HTML colors that are used to decorate the sidebar and titlebar. The defaults
are `w3-blue` and `w3-black`. The main page uses the sidebar color for its titlebar.

Color has no effect on markdown output. The line is removed.

The @mainpage colors will be reflected to all modules and classes that don't define their own
colors.

```
/*!
  @mainpage   My Project
  @color      w3-purple w3-orange
*/
```

For W3CSS color styles, see: <https://www.w3schools.com/w3css/w3css_colors.asp>

For more HTML visual customization, see also [4.8 - @logo](#4.8-logo)

### 4.3 - Keyword @defgroup

Usage = `@defgroup name description`

The @defgroup keyword collects a set of functions in a single module (which may encompass multiple
source files).

```
/*!
    @defgroup FlyWin  A full-color, overlapping windowing module for ANSI terminals
    @color w3-red w3-blue

    Text and perhaps examples here to describe the module in detail...
*/
```

Add the @color, @logo or @version to further customize the module. Otherwise it will inherit these
traits from the @mainpage.

Rather than including the @defgroup in a flydoc comment in some source file, you can instead
include a markdown file with the same name as the module:

file FlyWin.md:

```
# FlyWin  A full-color, overlapping windowing module for ANSI terminals
@color w3-red w3-blue

Text and perhaps examples here to describe the module in detail...
```

### 4.4 - Keyword @example

Usage = `@example title` followed by fenced code block.

The @example keyword defines a code example. Its only parameter is a title, which may contain any text.

Examples can appear anyplace within a class, method, module, function, mainpage or markdown
document.

The @example line with title must be followed by a fenced code block, that is a blank line, then a
pair of lines contain only triple backticks. The 1st triple ticks can be followed by a coding
language hint, for example:

    @example my_c_example

    ```c
    int meaning_of_life = 42;
    ```

Alternately, as per standard markdown, the fenced code block can be indended by 4 spaces rather
than using triple backticks. This doesn't allow specifying a coding language hint to the rendering
of the markdown to .pdf or HTML.

```
/*!
  @ingroup    MyModule
  @example    Some Example Name

      Example code here...

  Can be other text here that's not part of the example.
*/

/*!
  my_function description
  @example    my_function code example

      Example code here....

  @param      n   Number of iterations
  @returns    none
*/
void my_function(unsigned n)
{
  // code here...
}
```

All examples, along with links, are listed on the mainpage of the project.

### 4.5 - Keyword @fn

Usage = `@fn prototype`

flydoc can generally figure out the function prototype for all C-like languages like C/C++, Go,
Python, Rust, Javascript, etc... However, some languages allow functions without parentheses. In
this case, you can specify that it is a function by using the @fn keyword.

But some languages, like Python, allow a prototypes with no prentheses. In this case use the `@fn`
keyword followed by any text.

```
##! @fn       backup_database()
#   @return   {Integer} status
#
def backup_database
  # code here...
end
```

### 4.6 - Keyword @font

Usage = `@font body {headings}`

You can customize the HTML font that is used to render the text (body) and (optionally) headings
for any given HTML page or the entire project.

If the font is multiple words, it must be enclosed "in quotes". Separate alternate fonts or styles
with commas.

The defaults are from w3.css which are `Verdana,sans-serif` and `"Segoe UI",Arial,sans-serif`.

If @font appears in a class, module or markdown document, then the font(s) are for that file only.
Otherwise, each module, class and document will inherit the font from the @mainpage.

An example is below:

```
@mainpage MyProject
@font     "American Typewriter" Garamond

```

This renders to the HTML:

```html
<style>body{font-family:"American Typewriter"}h1,h2,h3,h4,h5,h6{font-family:Garamond}</style>
```

For a discussion of fonts, see <https://www.w3schools.com/Css/css_font.asp>

### 4.7 - Keyword @inclass

Usage = `@inclass name`

While @class on a particular name must be only defined once per project, @inclass can be used
anywhere.

Perhaps class methods are spread across multiple source files. In this case, use one @class to
define the class, then @inclass in the other source files to indicate they are part of that same
class.

```
/*!
  @inclass  MyClass
  @example  My Example for MyClass

      code here...
*/
```

All following method/function documentation are considered in the class, until another `@class`,
`@group`, `@ingroup` or `@inclass` is specified, or the end of the file is reached.

If you define `@inclass foo` and class foo does not exist in any of the input files, flydoc will
issue an error.

### 4.8 - Keyword @ingroup

Usage = `@ingroup name`

While @defgroup on a particular name must be only defined once per project, @ingroup can be used
anywhere.

Perhaps a module's functions are spread across multiple source files. In this case, use
one @defgroup to define the module, then @ingroup in the other source files to indicate they are
part of that same module.

```
/*!
  @ingroup  MyModule
  @example  My Example for MyModule

      code here...
*/
```

All following method/function documentation are considered in the group/module, until another
`@class`, `@group`, `@ingroup` or `@inclass` is specified, or the end of the file is reached.

If you define `@ingroup foo` and group (module) foo does not exist in any of the input files,
flydoc will issue an error.

### 4.9 - Keyword @logo

Usage = `@logo link`

The @logo keyword defines the image you want to see in upper left of the HTML title bar. The logo
can be a company logo, a project icon or any image you want. The image file must be something a
browser can render. flydoc looks for the file extensions `.png.jpg.jpeg.gif`.

There can be an @logo in any text for the mainpage, modules, classes, and markdown files.

The logo link is in standard markdown image link form `![text](imagefile.jpg "tooltip")`.

The "tooltip" field is optional. If this field begins with "w3-" then it assumes you want that
class contents, usually image classes, otherwise "tooltip" becomes the "title=" attribute of the
image.

An example of using making a gray-scale circle from an image could be:

```
/*!
  @logo ![flydoc](flydoclogo.png "w3-circle w3-grayscale-max")
*/
```

The result HTML will look something like:

```
<img src="flydoclogo.png" alt="flydoc" class="w3-circle w3-grayscale-max" style="width:150px">
```

Logos links are included in markdown on the line after the document/section title:

```
# My Project

![flydoc](flydoclogo.png "w3-circle w3-grayscale-max")

First line or paragraph after title.
```

An @logo in the @mainpage section applies to all pages as the default.

An @logo in an @class or @defgroup overrides the @mainpage logo and applies to that one module or
class. This allows you to customize the look of each module or class.


See: <https://www.w3schools.com/w3css/w3css_images.asp>  
For converting files to small versions of themselves, see: <https://tinypng.com>  

For more flydoc HTML visual customization, see also [4.2 - @color](#4.2-color-sidebar-titlebar)  

### 4.10 - Keyword @mainpage

Any given project must have only a single @mainpage, regardless of how many modules, classes,
functions, methods and examples there are. If there is more than one @manpage in the input files,
then only the 1st one encountered is used. The others are ignored with a warning issued by
flydoc.

The mainpage is the high-level naviation page and describes the project overall. It includes links
to every class, module, document and example.

The mainpage can either be in a source file special comment, or a markdown file called
`mainpage.md`.

In markdown output, this becomes the document main title and section. In HTML output, this is the
index.html.

```
/*!
  @mainpage   FireFly C Library and Tools

  A set of C algorithms and tools to make developing C or C++ programs easier

  @version    0.95.3
  @logo       ![w3-circle w3-grayscale-max](flydoclogo.png)
*/
```

### 4.11 - Keyword @param

Methods and functions may have parameters. Describe them here. The name is the variable name, the
description is a short one-line description. Ignored if not in a function comment.

```
/*!
  Returns pi to n digits in fixed point.
  @param  nDigits   Number of digits after the "3", e.g. 5 to get 3.14159 (314159)
  @return The number pi in fixed point (e.g. 314 == 3.14)
*/
unsigned long pi_fixed_point(unsigned nDigits)
{
  // code here...
}
```

### 4.12 - Keyword @return

Methods and functions may return values. Describe what is returned here. @returns is the same
as @return. Ignored if not in a function comment.

```
/*!
  Returns pi to n digits in fixed point.

  pi is already precalulated, so this is very fast. With a 64-bit longs, max nDigits are 18
  (any thing higher will be ignored): 3141592653589793238

  @param  nDigits   Number of digits after the "3", e.g. 5 to get 3.14159 (314159)
  @return The number pi in fixed point (e.g. 314 == 3.14)
*/
unsigned long pi_fixed_point(unsigned nDigits)
{
  // code here...
}
```

### 4.13 - Keyword @version

The @version is displayed in the upper left under the logo (if any). Each class or module may have
its own version, otherwise it will inherit the version from the @mainpage. If no @version is
provided on the @mainpage, no version will be displayed.

The version string can be anything. An example might be `1.0.222`, but could be `foo bar`. The
example below uses `0.9.gitsha`.

```
/*!
  @version  0.9.6df33b8
*/
```

For a discussion on versioning, see <https://semver.org>

## 5 - Advanced Topics

This chapter describes various advanced flydoc topics.

### 5.1 - Images

Markdown uses the following form for image links, where "title" is optional:

```
![alt](link.png "title")
```

This converts to the HTML:

```
<img alt="alt" src="link.png" title="title">
```

Unfortunately, standard markdown doesn't allow any formatting, positioning or sizing of images.

flydoc uses the optional "title", which is normally a tooltip (hover over the image in browswer)
for image formatting. The "title" formatting rules are as follows:

1. If no "title", then no class or style
2. If "title" begins with "w3_", then any w3_whatever goes in class="", and anything else goes in style=""
3. Otherwise title only, no special attributes

Examples of the "title" rules are below:

```html
no_title:    ![My Image](https://www.w3schools.com/Css/pineapple.jpg)
HTML:        <img alt="My Image" src="https://www.w3schools.com/Css/pineapple.jpg")>

w3_title:    ![ ](moon.jpeg "w3-circle w3-grayscale-max width:150px")
HTML:        <img alt=" " src="moon.jpeg" class="w3-circle w3-grayscale-max" style="width:150px")>

title_only: ![Corvette](car.png "this car is fast!!")
HTML:        <img alt="Corvette" src="car.png" title="this car is fast!!")>
```

If course you can always use HTML directly in markdown. The example below centers the image:

```html
<p style="text-align:center"><img src="math.jpeg" alt="Math Image" class="w3-grayscale-max" style="width:150px; display:inline"></p>
```

If the image doesn't have a path, either in markdown or in HTML, then flydoc will look through the
list of input files for that image so it can be copied to the output folder. If not found, a
warning will be issued.

flydoc uses the "title" for W3CSS image class information. For example, to make a grey scale
circle in the resulting HTML, use:

For more about W3CSS image classes, see <https://www.w3schools.com/w3css/w3css_images.asp>  

### 5.2 - Links

flydoc generates automatic (and predictable) HTML links for many markdown objects. All links are
case sensitive because URIs are case sensitive and C-like languages are case sensitive. That is
"foo" different than "Foo" which is different than "FOO".

Filename links:

* mainpage - always file index.html
* modules - modulename.html, case sensitive (e.g. MyModule.html or my_module.html)
* classes - classname.html, case sensitive (e.g. MyClass.html or my_class.html)
* markdown files (documents) - filename, case sensitive

Note: if you have filenames that conflict, flydoc will warn you. For example, say you have a C
module called "foo", and a markdown document called "foo.md", both of which will
become "foo.html", rename one of the inputs, perhaps "foo_md.md".

Local links (within a file):

* Functions, methods - cname, case sensitive (e.g. my_function or MyMethod)
* Headers - slug of header, case sensitive (e.g. "## My Header" becomes id="My-Header)
* Examples - slug of example title, case sensitive (e.g. "@example My Example" becomes id="My-Example")
* Footnotes - slug of footnote link (e.g. `[^footnote]` becomes id="footnote")

A "slug" is a URI friendly tranformation of a string. The basics are: case sensitive, alpha,
numeric, or one of `-._~`. If any character is not slug-worthy it is converted to a single
dash "-". Any dashes at the start/end of the slug are removed.

The example below shows a reference to a markdown header (notice slug conversion for id).

```
For garden tips, see [Section 3.5: I Hate @*&! Slugs!](#Section-3.5-I-Hate-Slugs)

    sometime later...

## Section 3.5: I Hate @*&! Slugs!
```

Note: flydoc has an option for creating slugs from a string so you don't have to:

```
$ flydoc --slug "Section 3.5: I Hate @*&! Slugs!"
Section-3.5-I-Hate-Slugs
```

Also Note: you can use UTF-8 in your slugs as long as your editor can produce UTF-8 markdown
because each HTML file is has meta charset="UTF-8" at the top of the file.

The local links can be combined with the filename. For example, the link for function "bar()" in
the module "foo" would be "foo.html#bar" if referenced externally from foo.html, or just "#bar".

HTML has various rules for special characters within references (links). It's complicated, and
depends on the version of your browser. Keep to alphanumeric, spaces, underscores, dots and dashes
as much as possible.

See <https://stackoverflow.com/questions/1236856/in-the-dom-are-node-ids-case-sensititve>  
See <https://www.rfc-editor.org/rfc/rfc3986#section-2.3>  
See <https://en.wikipedia.org/wiki/UTF-8>  

### 5.3 - Warnings and Errors

flydoc will produce warnings in the standard `file:line:col: message` format if something is wrong
with the inputs.

```
$ flydoc -o mytest.md test.*
test.c:7:10: warning: unknown keyword
  @whoops bad keyword
  ^

test.c:9:4: warning: image not found
  [Bald Eagle](eagle.jpeg)
               ^

test.py:99:10: warning: no class or module defined. Use @inclass or @ingroup
def my_py_function(name):
^

test.rs:100:1: warning: duplicate class or module
  @class foo
         ^

warning: invalid output folder: ../nothere/folder/
```

Make sure to resolve warnings or your HTML or markdown documentation may look or link poorly.

### 5.4 - Commenting Best Practices

Keep source code documentation brief. Describe the function or modules's purpose, parameters and
return value(s) as a reference. You can always write a full tutorial or details on design in a
markdown file in some other tool if your APIs need it.

Good coding practice hides implementation details. Only document public APIs with the special
flydoc comments. Private functions and methods classes and modules should use normal comments. 

You can comment both private and public functions exactly the same way, except for the marker on
the opening comment. For example, flydoc will ignore the `foo_private()` below because there is no
opening `/*!`, but will include the section function `foo_power()`.

```c
/*
  Transform n

  @param  n  - a number
  @return transformed n
*/
static int foo_private(int n)
{
    // ...
}

/*!
  Return n to the power. Does not detect wrap, e.g 2 to the power of 3 is 8.

  @aparam n       0 - SIZE_MAX
  @param  power   0 - UINT_MAX
  @return n to the power of power
*/
size_t foo_power(size_t n, unsigned power)
{
  // ...
}
```

flydoc special comments should be in the source files, not header files, as flydoc doesn't
search .h or .hpp files by default.

Stick to the basics for your markdown: text, bullets, ordered lists, tables, code blocks, images
and links. The markdown engine is pretty good, but some corner cases may break it.

Only define one @defgroup or @class per module or class, as any others of the same name will be
ignored. If a module or class is spread across multiple source files, use @inclass or @ingroup at
the top the other source files.

All commented text, with the exception of the line oriented @keywords, is in markdown.

See <https://www.markdownguide.org/cheat-sheet/>

### 5.5 - Using Markdown Files Sections

Sometimes, the front matter text for the @mainpage, @defgroup and @class can get lengthy. Instead
of including this text in one or more source files, you can include it instead in markdown files.

These keywords still define sections (HTML pages or markdown headings). Just place them in your
markdown file at the top, just like you would inside a comment.

Assume you have three C modules, foo, bar and baz. Simply make the files `foo.md`, `bar.md` and
`baz.md` and include them in your inputs.

For example, in the folder/file layout below, simply include folder `project_foo/` as an input folder:

```
$ ls -R project_foo/
project_foo/
  images/
    main.png
    flag.jpeg
    eagle.jpg
  front_matter/
    mainpage.md
    foo.md
    bar.md
    baz.md
  inc/
    foo.h
    bar.h
    baz.h
  src/
    foo1.c
    foo2.c
    baz_because.c
    bar_none.c
$ flydoc --html -o foo_html/ project_foo/
```

### 5.6 - International Languages

Source code documentation is often in english, but it may be in any human language. Simply use an editor that supports UTF-8.

For example, assuming your editor supports UTF-8, you should see flame, cat, smile and a japanese character (tree) below:

🔥 🐈 😊 木

One editor that supports UTF-8 is [Sublime Text](https://www.sublimetext.com).

## 6 - Flydoc Comments By Language

This chapter gives examples in various languages for documenting the code.

The flydoc tool only pays attention to documenation inside of "special" comments, that is, those
that generally have a bang `!` character after the opening of the comment. Example of comment
types across languages is below.

```
/*!
    Preferred C/C++, Java, Javascript, Rust, Swift or Typescript flydoc comment
*/

/*!
 *  Another type of C/C++ flydoc comment
 */

##!
#   Preferred Ruby flydoc comment
#
```

Python is special, in that the documentation for a function FOLLOWS the function definition:

```python
def PyArea(h,w):
    """!
        Calculate the area of a rectangle
        @param  h   height
        @param  w   width
        @return area of rectangle
    """
    return h * w
```

### 6.1 - C Doc Comments

```c
#include "maths.h"

/*!
  @mainpage Maths
  @version  0.42

  A multi-language math library that may someday rival MATLAB

  @defgroup maths A language independent API for higher mathematics

  Works across the top languages of today: C, C++, C#, Go, Java, Javascript, Python, Ruby, Rust,
  Swift and Typescript

  @example maths  An example of calculating area.

      #include "maths.h"
      int main(void) {
        printf("Area of 5x4 rectangle = %u\n", area(h,w));
        return 0;
      }
*/

/*!***************************************************************************
  Calculates area of a rectangle
  @param  h   height
  @param  w   width
  @return area of rectangle
*****************************************************************************/
unsigned area(unsigned h, unsigned w)
{
  return h * w;
}

/*!
 * Calculates volume of a cube
 *
 * @example volume   An example of calculating volume.
 *
 * printf("volume of 3x4x5 cube = %u\n", volume(3,4,5));
 * 
 *     // code indented by 4 spaces
 *
 * @param  h   height
 * @param  w   width
 * @param  d   depth
 * @return volume of the cube
*/
unsigned volume(unsigned h, unsigned w, unsigned d)
{
  return h * w * d;
}

int main(void)
{
  printf("Area of 5x4 rectangle = %u\n", area(5,4));
  printf("volume of 3x4x5 cube = %u\n", volume(3,4,5));
  return 0;
}
```

### 6.2 - C++ Doc Comments

```c++
#include <iostream>
using namespace std;

/*!
  @class Cars A class for listing automobiles

  Eventually target market will be car and dealerships and repair shops.
*/
class Car {
  public:
    string brand;
    string model;
    int year;

    /*!
      Constructor for Car
      @param    _brand    brand of car (e.g. Honda)
      @param    _model    model of car (e.g. Ridgeline)
      @param    _year     year of car (e.g. 2022)
    */
    Car(string _brand, string _model, int _year) {
      brand = _brand;
      model = _model;
      year = _year;
    }
    void Print(void);
};

int IsCarNewish(int year)
{
  return (year >= 1999);
} 

/*!
  Print out make, model, year of the car
  @return none
*/
void Car::Print(void)
{
  cout << this->brand << " " << this->model << " " << this->year;
  if(IsCarNewish(this->year))
    cout << " new(ish)";
  cout << "\n";
}

int main() {
  // Create Car objects and call the constructor with different values
  Car carObj1("BMW", "X5", 1999);
  Car carObj2("Ford", "Mustang", 1969);

  // Print values
  cout << carObj1.brand << " " << carObj1.model << " " << carObj1.year << "\n";
  cout << carObj2.brand << " " << carObj2.model << " " << carObj2.year << "\n";
  carObj1.Print();

  return 0;
}
```

### 6.3 - Javascript Doc Comments

```javascript
#!/usr/bin/env node

/*!
  Calculates area of a rectangle
  @ingroup  maths
  @param  h   height
  @param  w   width
  @return area of rectangle
*/
function jsArea(h, w) {
  return h * w; 
}

console.log( "Javascript example: area = %d", jsArea(5,4));
```

### 6.4 - Python Doc Comments

```python
#!/usr/bin/env python3

def PyArea(h,w):
    """!
        Calculate the area of a rectangle
        @param  h   height
        @param  w   width
        @return area of rectangle
    """
    return h * w

print("Python example: area = ", PyArea(5,4))
```

### 6.5 - Ruby Dock Comments

You can always use RDoc to document Ruby source code. But if using flydoc, do the following:

```ruby
##!
# Find higest altitude given an array of gain/loss points.
# @param  gain  Integer[]
# @return Integer
def highest_altitude(gain)
  # ...
end 
```

### 6.6 - Rust Doc Comments

Rust has its own documentation tool, which you may want to use instead of flydoc. Other than using
markdown inside the comment format, rustdoc and flydoc aren't compatible.

See <https://doc.rust-lang.org/rustdoc/what-is-rustdoc.html>  
See also: <https://doc.rust-lang.org/reference/comments.html>  

Below is a flydoc example for commenting Rust.

```rust
/*!
  Calculates area of a rectangle
  @ingroup  maths
  @param  h   height
  @param  w   width
  @return area of rectangle
*/
fn rust_area(h: i32, w: i32) -> i32 {
    h * w
}

fn main() {
  println!("Rust example: area = {}", rust_area(5, 4))
}
```

### 6.7 - Swift Doc Comments

```swift
/*!
  Calculates area of a rectangle
  @ingroup  maths
  @param  h   height
  @param  w   width
  @return area of rectangle
*/
func SwiftArea(h: Int, w:Int) -> Int {
    return h * w
}

func main() {
    print("Swift example: area = ", SwiftArea(h:5, w:4))
}
```

### 7 - Useful Links

For an example of flydoc output, see <https://drewagislason.github.io/index.html>  
For a W3.css tutorial, see <https://www.w3schools.com/w3css/default.asp>  
For a markdown tutorial, see <https://www.markdownguide.org/cheat-sheet/>  
For an online markdown converter, see <https://markdowntohtml.com>  
For markdown editors, see: <https://www.makeuseof.com/best-markdown-editors-windows/>  
For popular coding languages, see: <https://madnight.github.io/githut/#/pull_requests/2021/3>  
