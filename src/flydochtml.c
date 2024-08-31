/**************************************************************************************************
  flydochtml.c - create markdown output from a flyDoc_t structure
  Copyright 2024 Drew Gislason
  License MIT <https://mit-license.org>
**************************************************************************************************/
#include <sys/stat.h>
#include "flydoc.h"
#include "FlyFile.h"
#include "FlyList.h"
#include "FlyMem.h"
#include "FlyStr.h"
#include "FlyUtf8.h"
#include "FlyMarkdown.h"

/*!
  @defgroup flydoc_html  Create markdown output from flyDoc_t structure

  **Layout for Mainpage**

      +---------+----------------------+
      | Logo    | Title Bar            |
      +---------+----------------------+
      | Classes | Examples | Documents |
      | Modules |          |           |
      |         |          |           |
      +---------+----------+-----------+

  **Layout for Module/Class/Markdown page**

      +-------+--------------------+
      | Logo  | Title Bar          |
      +-------+--------------------+
      |sidebar| main area          |
      |       |                    |
      |       |                    |
      +-------+--------------------+
*/

//
// HTML
//
static const char m_szHtmlHead[] =
  "<!DOCTYPE html>\r\n"
  "<html>\r\n"
  "<head>\r\n"
  "<title>%s</title>\r\n"
  "<meta charset=\"UTF-8\" name=\"viewport\" content=\"width=device-width, initial-scale=1\">\r\n"
  "<link rel=\"stylesheet\" href=\"%sw3.css\">\r\n"
  "%s"      // optional <meta font=
  "</head>\r\n"
  "<body>\r\n";

static const char m_szHtmlFontOpen[]     = "<style>body{font-family:";        // e.g. body{font-family:"American Typewriter"}
static const char m_szHtmlFontHeadings[] = "}h1,h2,h3,h4,h5,h6{font-family:"; // e.g. h1,h2,h3,h4,h5,h6{font-family:Garamond}
static const char m_szHtmlFontClose[]    = "}</style>\r\n";

// title row, bar color
static const char m_szTitleBarOpen[] =
  "<div class=\"w3-cell-row\">\r\n"
  "  <div class=\"w3-container w3-cell w3-mobile %s\">\r\n" // color
  "    <div class=\"w3-container w3-cell w3-mobile\">\r\n"
  "      <p>%s</p>\r\n";          // logo <img link>
static const char m_szTitleBarVersion[] =
  "      <center><p>version %s</p></center>\r\n";  // version string
static const char m_szTitleBarTitle[] =
  "    </div>\r\n"
  "    <div class=\"w3-container w3-cell w3-mobile\">\r\n"
  "      <h1>%s</h1>\r\n";   // title
static const char m_szTitleBarSubtitle[] =
  "      <h3>%s</h3>\r\n";     // brief
static const char m_szTitleBarClose[] =
  "    </div>\r\n"
  "  </div>\r\n"
  "</div>\r\n";

// text row, normal color
static const char m_szMainTextOpen[] = 
  "<div class=\"w3-cell-row\">\r\n"
  "  <div class=\"w3-container w3-cell w3-mobile\">\r\n";
static const char m_szMainTextClose[] =
  "  </div>\r\n"
  "</div>\r\n";

// open row for all 3 columns
static const char m_szMainRowOpen[] =  
  "<div class=\"w3-cell-row\">\r\n";

static const char m_szMainColOpen[] =     // open a single column
  "  <div class=\"w3-container w3-cell w3-mobile\">\r\n"
  "    <h2>%s</h2>\r\n";                  // e.g. "Modules & Classes", "Examples", "Documents"

static const char m_szMainColObjects[] =
  "    <p><b>%u %s</b></p>\r\n";          // e.g. "12 Modules"

static const char m_szMainColObjectsSep[] = // separates Modules and Classes in 1st column
  "    <p> </p>\r\n";

static const char m_szMainColExampleGroup[] = 
  "    <p><b>%s Example(s)</b></p>\r\n";

static const char m_szMainColRefLine[] = 
  "    <p><a href=\"%s\">%s</a> - %s</p>\r\n";

static const char m_szMainColExampleLine[] = 
  "    <p>Example: <a href=\"%s\">%s</a></p>\r\n";

static const char m_szMainColClose[] =
  "  </div>\r\n";

static const char m_szMainRowClose[] =
  "</div>\r\n";

static const char m_szMainEnd[] =
  "</body>\r\n"
  "</html>";

// <img src=\"flydoclogo.png\" class=\"w3-circle\" style=\"width:150px\">
// <h1>Main Page</h1>
// static const char m_szModTitleBar[] =
//   "<div class=\"w3-cell-row\">\r\n"
//   "  <div class=\"w3-container w3-cell-middle w3-mobile %s\">\r\n"  // color e.g. w3-black
//   "    <div class=\"w3-container w3-cell\">\r\n"
//   "      <p><a href=\"index.html\">%s</a></p>\r\n"    // logo
//   "    </div>\r\n"
//   "    <div class=\"w3-container w3-cell\">\r\n"
//   "      <h1>%s%s</h1>\r\n"   // Title or Class Title
//   "    </div>\r\n"
//   "  </div>\r\n"
//   "</div>\r\n";

static const char m_szModLeftOpen[] =
  "<div class=\"w3-cell-row\">\r\n"
  "  <div class=\"w3-container w3-cell w3-mobile %s\">\r\n";  // color, e.g. w3-blue

// static const char m_szModLeftVersion[] =
//   "    <p>version %s</p>\r\n";   // version 0.9.53

static const char m_szModLeftSpacer[] =
  "    <p>\r\n";

static const char m_szModLeftLine[] =
  "      <a href=\"%s\">%s</a><br>\r\n";  // href, pFunc->szFunc

static const char m_szModLeftBarEnd[] =
  "    </p>\r\n"
  "  </div>\r\n";

static const char m_szModRightOpen[] =
  "  <div class=\"w3-container w3-cell w3-mobile\">\r\n";

static const char m_szModRightTitle[] =
  "    <h2>%s</h2>";

// static const char szModRightLine[] = 
//   "    <p>%.*s</p>\r\n";

static const char m_szModRightFuncHead[] =
  "    <h3 id=\"%s\" class=\"%s\">%s</h3>\r\n"  // href, color, pFunc->szFunc
  "    <p>%s</p>\r\n"   // pFunc->szBrief
  "    <p><a href=\"#top\">Back to top</a></p>\r\n";

static const char m_szModRightProtoOpen[] =
  "    <p><b>Prototype</b></p>\r\n"
  "    <div class=\"w3-code w3-monospace notranslate\">\r\n";

static const char m_szModRightProtoLine[] =
  "      %.*s<br>\r\n";

static const char m_szModRightProtoClose[] =
  "    </div>\r\n";

static const char m_szModRightNotesOpen[] =
  "    <p><b>Notes</b></p>\r\n";

// static const char szModRightNotesLine[] =
//   "    <p>%.*s</p>\r\n";

static const char m_szModEnd[] =
  "  </div>\r\n"
  "</div>\r\n"
  "</body>\r\n"
  "</html>\r\n";

// static const char szDocHtmlModExampleHead[] = "    <h3 id=\"%s\" class=\"%s\">%s</h3>\r\n";  // href, color, pFunc->szFunc
// static const char m_szDocHtmlExample[]        = "    <p id=\"%s\"><b>%s</b></p>\r\n";
// static const char szExampleTitle[]          = "Examples";

// static const char szPrefixClass[]    = "class-";

  typedef enum
{
  MP_TYPE_MODULES_CLASSES,
  MP_TYPE_MODULES,
  MP_TYPE_CLASSES,
  MP_TYPE_EXAMPLES,
  MP_TYPE_DOCUMENTS,
} mpType_t;

typedef struct
{
  mpType_t    type;
  const char *szHeading;
} mainpageColType_t;

static const char   m_szTableOfContents[]           = "Table of Contents";
static const char   m_szHeadingModulesAndClasses[]  = "Modules & Classes";
static const char   m_szHeadingModules[]            = "Modules";
static const char   m_szHeadingClasses[]            = "Classes";
static const char   m_szHeadingExamples[]           = "Examples";
static const char   m_szHeadingDocuments[]          = "Documents";
static const char   m_szHeadingModuleSingular[]     = "Module";
static const char   m_szHeadingClassSingular[]      = "Class";
static const char   m_szHeadingExampleSingular[]    = "Example";
static const char   m_szHeadingDocumentSingular[]   = "Document";

/*-------------------------------------------------------------------------------------------------
  Allocate the font-family for body and optionaly headings.

  If this section doesn't have the fields, then inherit from mainpage.

  Example output:

  ```html
      <style>body{font-family:Copperplate}h1,h2,h3,h4,h5,h6{font-family:"American Typewriter"}</style>
  ```

  @param  pSection    a pointer to the section
  @return ptr to font string, which must be freed by higher layer.
-------------------------------------------------------------------------------------------------*/
static char * MdFontStyleAlloc(flyDoc_t *pDoc, const char *szFontBody, const char *szFontHeadings)
{
  unsigned    size;
  char        *szHtmlFont = NULL;

  if(szFontBody)
  {
    // calculate size of string
    size = sizeof(m_szHtmlFontOpen);
    size += strlen(szFontBody);
    if(szFontHeadings)
      size += strlen(m_szHtmlFontHeadings) + strlen(szFontHeadings);
    size += strlen(m_szHtmlFontClose);

    // allcoate and fill in the font HTML
    // e.g. <style>body{font-family:Copperplate}h1,h2,h3,h4,h5,h6{font-family:"American Typewriter"}</style>
    szHtmlFont = FlyAllocZ(size);
    if(szHtmlFont)
    {
      strcpy(szHtmlFont, m_szHtmlFontOpen);
      strcat(szHtmlFont, szFontBody);
      if(szFontHeadings)
      {
        strcat(szHtmlFont, m_szHtmlFontHeadings);
        strcat(szHtmlFont, szFontHeadings);
      }
      strcat(szHtmlFont, m_szHtmlFontClose);
    }
  }

  // no fonts specified, include no HTML
  if(!szHtmlFont)
    szHtmlFont = FlyStrClone("");

  return szHtmlFont;
}

/*!------------------------------------------------------------------------------------------------
  Make an HTML reference id from optional base and optional title.

  At least one of szBase and szTitle must be defined (non-NULL).

  File only ref    = "MyModule.html"
  Local only ref   = "#local-ref"
  File + local ref = "MyModule.html#local-ref"

  szBase is the base part of a filename, so it follows the OS rules for a filename. Example:

      "MyModule" will become "MyModule.html" in the reference.

  szTitle can be any string, is case sensitive, and will be converted to a URL friendly slug
  string. Example:

      "  This $%@! Long Title  " becomes slug string "This-Long-Title"

  @param  szRef     buffer receive reference string, may be NULL to just get length
  @param  size      sizeof szRef buffer
  @param  szBase    file basename, e.g. "MyModule", or NULL if local reference only
  @param  szTitle   Local reference in normal "string form", converted to slug, e.g. "#string-form"
  @return length of ref (even if bigger than size)
-------------------------------------------------------------------------------------------------*/
size_t FlyDocStrToRef(char *szRef, unsigned size, const char *szBase, const char *szTitle)
{
  const char  szHtmlExt[]  = ".html";
  const char  szHash[]     = "#";
  char       *pszSlug;
  size_t      len = 0;

  // at least one of szBase or szTitle must be defined, otherwise the id makes no sense.
  FlyAssert(szBase || szTitle);

  if(size > 1)
  {
    if(szRef)
      *szRef = '\0';

    // reference has file part
    if(szBase)
    {
      if(szRef)
      {
        FlyStrZCat(szRef, szBase, size);
        FlyStrZCat(szRef, szHtmlExt, size);
      }
      len = strlen(szBase) + strlen(szHtmlExt);
    }

    // reference has local part
    if(szTitle)
    {
      FlyStrZCat(szRef, szHash, size);
      len += strlen(szHash);
      pszSlug = szRef ? szRef + strlen(szRef) : NULL;
      len += FlyUtf8SlugCpy(pszSlug, szTitle, size, FlyStrLineLen(szTitle));
    }
  }

  return len;
}

/*!------------------------------------------------------------------------------------------------
  Write the HTML text for module or function or entire markdown file.

  1. Converts @example lines into "Example: example title", verifies
  2. Uses bar color for headings
  3. Does NOT find @example or # headings in code blocks

  @param  pDoc        Document state with open pDoc->fpOut
  @param  szText      Markdown text, '\0' terminated
  @param  szW3Color   Color for headings, e.g. w3-red-text
  @return TRUE if worked, FALSE if out of memory
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocHtmlWriteText(flyDoc_t *pDoc, const char *szText, const char *szW3Color)
{
  const char       *psz;
  const char       *szArg;
  const char       *szLine;
  const char       *szOrgLine;
  const char       *szEnd;
  char             *szHtml;
  flyDocKeyword_t   keyword;
  char              szBuf[FLYDOC_REF_MAX];
  size_t            len;
  bool_t            fWorked = TRUE;

  FlyAssert(pDoc && pDoc->fpOut);

  // printf("FlyDocHtmlWriteText(szText len %zu, szW3Wcolor %s)\n", strlen(szText), szW3Color);
  // FlyStrDump(szText, strlen(szText));

  szLine = szText;
  while(fWorked && *szLine)
  {
    // debugging (make sure line advances)
    szOrgLine = szLine;

    // find end of normal content (that is break on the next @keyword or # Heading
    szEnd = szLine;
    while(*szEnd)
    {
      // ignore keywords and headers in code blocks
      if(FlyMd2HtmlIsCodeBlk(szEnd, NULL))
      {
        FlyMd2HtmlCodeBlk(NULL, SIZE_MAX, &szEnd, NULL, NULL);
        continue;
      }
      if(FlyDocIsKeyword(szEnd, &keyword) && !FlyDocIsKeywordProto(keyword))
        break;
      else if(FlyMd2HtmlIsHeading(szEnd, NULL))
        break;
      szEnd = FlyStrLineNext(szEnd);
    }

    // handle normal content
    if(szEnd > szLine)
    {

      len = FlyMd2HtmlContent(NULL, SIZE_MAX, szLine, szEnd);

      // if(len == 0)
      //   printf("Empty Content at line %u\n", FlyStrLinePos(szText, szLine, NULL));

      // only blank lines until next "thing", just use an empty paragraph
      if(len)
      {
        szHtml = FlyDocAlloc(len + 1);
// printf("Content at line %u, %.*s, szLine %p, szEnd %p, HTML len %zu\n", 
//   FlyStrLinePos(szText, szLine, NULL), (int)FlyStrLineLen(szLine), szLine, szLine, szEnd, len);

        FlyMd2HtmlContent(szHtml, len + 1, szLine, szEnd);

// printf("Done Content at line %u\n", FlyStrLinePos(szText, szLine, NULL));
        if(fwrite(szHtml, 1, strlen(szHtml), pDoc->fpOut) <= 0)
          fWorked = FALSE;
        FlyFree(szHtml);
      }
      szLine = szEnd;
      continue;
    }

    // handle keywords (ignore, include or @example)
    szArg = FlyDocIsKeyword(szLine, &keyword);
    if(szArg)
    {
      if(keyword == FLYDOC_KEYWORD_EXAMPLE)
      {
        // we expect a code block after an example (with possibly some blank lines)
        szLine = FlyStrLineSkipBlank(FlyStrLineNext(szLine));

        // code block follows the example, build it with title
        if(FlyMd2HtmlIsCodeBlk(szLine, NULL))
        {
          FlyStrZCpy(szBuf, "Example: ", sizeof(szBuf));
          FlyStrZNCat(szBuf, szArg, sizeof(szBuf), FlyStrLineLen(szArg));
          psz = szLine;
          len = FlyMd2HtmlCodeBlk(NULL, SIZE_MAX, &psz, szBuf, NULL);
          szHtml = FlyDocAlloc(len + 1);
          FlyMd2HtmlCodeBlk(szHtml, len + 1, &szLine, szBuf, NULL);
          if(fwrite(szHtml, 1, strlen(szHtml), pDoc->fpOut) <= 0)
            fWorked = FALSE;
          FlyFree(szHtml);
        }

        // no code block following example, just make it a level 5 heading
        else
        {
          FlyStrZCpy(szBuf, "##### ", sizeof(szBuf));
          FlyStrZCat(szBuf, szArg, sizeof(szBuf));
          psz = szBuf;
          len = FlyMd2HtmlHeading(NULL, SIZE_MAX, &psz, NULL);
          szHtml = FlyDocAlloc(len + 1);
          psz = szBuf;
          FlyMd2HtmlHeading(szHtml, len + 1, &psz, NULL);
          if(fwrite(szHtml, 1, strlen(szHtml), pDoc->fpOut) <= 0)
            fWorked = FALSE;
          FlyFree(szHtml);
        }
        continue;
      }

      // ignore keywords and don't include in text
      szLine = FlyStrLineNext(szLine);
      continue;
    }

    // headings are in the bar color
    else if(FlyMd2HtmlIsHeading(szLine, NULL))
    {
      psz = szLine;
      len = FlyMd2HtmlHeading(NULL, SIZE_MAX, &psz, szW3Color);
      szHtml = FlyDocAlloc(len + 1);
      FlyMd2HtmlHeading(szHtml, len + 1, &szLine, szW3Color);
      if(fwrite(szHtml, 1, strlen(szHtml), pDoc->fpOut) <= 0)
        fWorked = FALSE;
      FlyFree(szHtml);
    }

    // should NEVER get stuck (line should keep advancing)
    FlyAssert(szLine > szOrgLine);
  }

  return fWorked;
}

/*!-------------------------------------------------------------------------------------------------
  Create a path to this HTML file in the pDoc->opts.szOut folder.

  Examples:

  This                     | Becomes That
  ------------------------ | ------------
  index                    | out_folder/index.html
  ../some/path/Tutorial.md | out_folder/Tutorial.html
  MyClassFoo               | out_folder/MyClassFoo.html
  bar_module               | out_folder/bar_module.html

  @param  pDoc      flydoc state
  @param  szPath    markdown file, module, class or index (MainPage)
  @return FILE * if worked, NULL if not.
-------------------------------------------------------------------------------------------------*/
FILE * FlyDocCreateHtmlFile(flyDoc_t *pDoc, const char *szPath)
{
  const char  szHtmlExt[] = ".html";
  const char *szBaseName;
  unsigned    len;
  FILE       *fp          = NULL;

  // get filename to display, short form, e.g. ~/folder/file.html
  FlyStrZCpy(pDoc->szPath, pDoc->opts.szOut, sizeof(pDoc->szPath));
  szBaseName = FlyStrPathNameBase(szPath, &len);
  if(!isslash(FlyStrCharLast(pDoc->szPath)))
    FlyStrZCat(pDoc->szPath, "/", sizeof(pDoc->szPath));
  FlyStrZNCat(pDoc->szPath, szBaseName, sizeof(pDoc->szPath), len);
  FlyStrZCat(pDoc->szPath, szHtmlExt, sizeof(pDoc->szPath));
  if(pDoc->opts.verbose >= FLYDOC_VERBOSE_MORE)
    printf("  %s\n", pDoc->szPath);

  // create a file for writing (higher layer will issue a warning)
  if(!pDoc->opts.fNoBuild)
    fp = fopen(pDoc->szPath, "w");

  return fp;
}

/*!-------------------------------------------------------------------------------------------------
  Create an HTML image from a markdown image. Surround it by a reference anchor if given.

  @param  szMdImage   e.g. "![alt](file.png)"
  @param  szRef       e.g. "index.html" or NULL (no ref)
  @return ptr to allocated HTML
-------------------------------------------------------------------------------------------------*/
char * FDocAllocImageWithRef(const char *szMdImage, const char *szRef)
{
  static const char szRefOpen[] = "<a href=\"";
  static const char szRefMid[]  = "\">";
  static const char szRefEnd[]  = "</a>";

  char       *szHtml = NULL;
  const char *psz;
  size_t      len;
  size_t      size;

  // determine size of image in HTML form
  psz = szMdImage;
  len = FlyMd2HtmlImage(NULL, SIZE_MAX, &psz);

  if(szRef == NULL)
    size = len + 1;
  else
    size =  strlen(szRefOpen) + strlen(szRef) + strlen(szRefMid) + len + strlen(szRefEnd) + 1;
  szHtml = FlyAllocZ(size);
  if(szHtml)
  {
    if(szRef)
    {
      FlyStrZCpy(szHtml, szRefOpen, size);
      FlyStrZCat(szHtml, szRef, size);
      FlyStrZCat(szHtml, szRefMid, size);
    }
    psz = szMdImage;
    FlyMd2HtmlImage(szHtml + strlen(szHtml), size, &psz);
    if(szRef)
      FlyStrZCat(szHtml, szRefEnd, size);
  }

  return szHtml;
}

/*!-------------------------------------------------------------------------------------------------
  Write the HTML front matter and title bar for all types of pages:

  1. mainpage
    * Uses side bar color for title bar
    * Includes subtitle in title bar
  2. module/class
  3. document (markdown file)

  @param  pDoc      state of flydeoc
  @param  pSection  section to open, contains title, subtitle, etc...
  @param  pStyle    page style: @color, @font, @logo, @version, etc...
  @return none
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocHtmlWriteOpen(flyDoc_t *pDoc, const flyDocSection_t *pSection, const flyDocStyle_t *pStyle)
{
  const char   *szTitleColor; 
  char         *szHtml;
  bool_t        fIsMainPage = FALSE;
  bool_t        fWorked     = TRUE;

  // some things are different for main page
  if(pDoc->pMainPage && pSection == &pDoc->pMainPage->section)
  {
    fIsMainPage = TRUE;
    szTitleColor = pStyle->szBarColor;
  }
  else
    szTitleColor = pStyle->szTitleColor;

  // write <head> section, which may include special fonts
  FlyAssert(FlyStrCount(m_szHtmlHead, "%s") == 3);
  szHtml = MdFontStyleAlloc(pDoc, pStyle->szFontBody, pStyle->szFontHeadings);
  FlyDocAllocCheck(szHtml);
  if(fWorked && fprintf(pDoc->fpOut, m_szHtmlHead, pSection->szTitle, pDoc->opts.fLocal ? "" : szW3CssPath, szHtml) <= 0)
    fWorked = FALSE;
  FlyFreeIf(szHtml);

  // write title bar and logo
  if(fWorked)
  {
    FlyAssert(pStyle->szLogo);
    szHtml = FDocAllocImageWithRef(pStyle->szLogo, fIsMainPage ? NULL : "index.html");
    FlyDocAllocCheck(szHtml);
    FlyAssert(FlyStrCount(m_szTitleBarOpen, "%s") == 2);
    if(fprintf(pDoc->fpOut, m_szTitleBarOpen, szTitleColor, szHtml) <= 0)
      fWorked = FALSE;
    FlyFree(szHtml);
  }

  // optional version goes below logo in same column
  if(fWorked && pStyle->szVersion && strlen(pStyle->szVersion))
  {
    FlyAssert(FlyStrCount(m_szTitleBarVersion, "%s") == 1);
    if(fprintf(pDoc->fpOut, m_szTitleBarVersion, pStyle->szVersion) <= 0)
      fWorked = FALSE;
  }

  // close logo column, open a new column and write title heading
  if(fWorked)
  {
    FlyAssert(FlyStrCount(m_szTitleBarTitle, "%s") == 1);
    if(fprintf(pDoc->fpOut, m_szTitleBarTitle, pSection->szTitle) <= 0)
      fWorked = FALSE;
  }

  // write optional subtitle, but only for mainpage
  if(fWorked && pSection->szSubtitle)
  {
    FlyAssert(FlyStrCount(m_szTitleBarSubtitle, "%s") == 1);
    if(fprintf(pDoc->fpOut, m_szTitleBarSubtitle, pSection->szSubtitle) <= 0)
      fWorked = FALSE;
  }

  if(fWorked)
  {
    FlyAssert(FlyStrCount(m_szTitleBarClose, "%s") == 0);
    if(fprintf(pDoc->fpOut, m_szTitleBarClose) <= 0)
      fWorked = FALSE;
  }

  return fWorked;
}

/*!-------------------------------------------------------------------------------------------------
  Write each module into the cell content for this list. File is already open in pDoc->fpOut.

  @param    pDoc    flydoc state
  @param    
  @return TRUE if worked, FALSE if problem writing to file
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocHtmlWriteMainModList(flyDoc_t *pDoc, flyDocModule_t *pModList)
{
  char              szRef[FLYDOC_REF_MAX];
  flyDocModule_t   *pMod;
  const char        *szHeading;
  bool_t            fWorked = TRUE;
  unsigned          nMods;

  FlyAssert(pDoc && pDoc->fpOut);

  nMods = FlyListLen(pModList);
  if(nMods)
  {
    if(pModList == pDoc->pClassList)
      szHeading = (nMods == 1) ? m_szHeadingClassSingular : m_szHeadingClasses;
    else
      szHeading = (nMods == 1) ? m_szHeadingModuleSingular : m_szHeadingModules;

    // small heading for objects
    if(fprintf(pDoc->fpOut, m_szMainColObjects, nMods, szHeading) <= 0)
      fWorked = FALSE;

    // write a link to each class or module
    pMod = pModList;
    while(fWorked && pMod)
    {
      // create reference
      FlyDocStrToRef(szRef, sizeof(szRef), pMod->section.szTitle, NULL);

      // "<p><a href=\"%s.html\">%s</a> - %s</p>";
      FlyAssert(FlyStrCount(m_szMainColRefLine, "%s") == 3);
      if(fprintf(pDoc->fpOut, m_szMainColRefLine, szRef, pMod->section.szTitle,
                 pMod->section.szSubtitle ? pMod->section.szSubtitle : "") <= 0)
      {
        fWorked = FALSE;
      }
      pMod = pMod->pNext;
    }
  }

  return fWorked;
}

/*!-------------------------------------------------------------------------------------------------
  Write each example from this section into the main page index.html.

  Helper to FlyDocHtmlWriteMainPage();

  Examples might be found in modules, classes and markdown documents.

  The szTitlePrefix is used to generate the heading for the example(s);

      Module Foo Example(s)
      Class Bar Example(s)
      Document Baz Example(s)
      Main Page Example(s)

  @param    pDoc            flydoc state
  @param    szRefBase       Reference base, e.g. "MyModule.html", "markdown.html" or NULL
  @param    pSection        Section containing example list
  @param    szTItlePrefix   e.g. "Module" so we an print the title :
  @return   TRUE if worked, FALSE if there is a problem with writing to file
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocHtmlWriteExampleList(flyDoc_t *pDoc, const char *szRefBase, flyDocSection_t *pSection, const char *szTitlePrefix)
{
  char              szRef[FLYDOC_REF_MAX];
  flyDocExample_t  *pExample;
  unsigned          nExamples;
  bool_t            fIsMainPage;
  bool_t            fWorked = TRUE;

  // ignore empty example lists
  nExamples = FlyListLen(pSection->pExampleList);
  if(nExamples)
  {
    fIsMainPage = (pDoc->pMainPage && pSection == &pDoc->pMainPage->section) ? TRUE : FALSE;

    // write module/document name, e.g. "Module Foo", or "Document my_markdown.md"
    FlyStrZCpy(szRef, szTitlePrefix, sizeof(szRef));
    FlyStrZCat(szRef, " ", sizeof(szRef));
    if(!fIsMainPage)
      FlyStrZCat(szRef, pSection->szTitle, sizeof(szRef));
    FlyAssert(FlyStrCount(m_szMainColExampleGroup, "%s") == 1);
    if(fprintf(pDoc->fpOut, m_szMainColExampleGroup, szRef) <= 0)
      fWorked = FALSE;

    pExample = pSection->pExampleList;
    while(fWorked && pExample)
    {
      // create reference
      FlyDocStrToRef(szRef, sizeof(szRef), szRefBase, pExample->szTitle);
  
      // "<p>Example: <a href=\"%s\">%s</a></p>\r\n";
      FlyAssert(FlyStrCount(m_szMainColExampleLine, "%s") == 2);
      if(fprintf(pDoc->fpOut, m_szMainColExampleLine, szRef, pExample->szTitle) <= 0)
        fWorked = FALSE;
      pExample = pExample->pNext;
    }
  }

  return fWorked;
}

/*!-------------------------------------------------------------------------------------------------
  Write each example into the cell content for examples column.

  Heleper to FlyDocHtmlWriteMainPage()

  Examples might be found in modules, classes and markdown documents.

  @pDoc         Document state
  @return       TRUE if worked, FALSE if there is a problem with writing to file
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocHtmlWriteMainExamplesAll(flyDoc_t *pDoc)
{
  flyDocModule_t   *pMod;       // modules or classes may have examples
  flyDocMarkdown_t *pDocument;  // markdown documents can have examples too
  const char       *szHeading;
  char              szNameBase[FLYDOC_REF_MAX];
  bool_t            fWorked = TRUE;

  FlyAssert(pDoc && pDoc->fpOut);

  if(pDoc->nExamples)
  {
    // small heading for objects
    szHeading = (pDoc->nExamples == 1) ? m_szHeadingExampleSingular : m_szHeadingExamples;
    if(fprintf(pDoc->fpOut, m_szMainColObjects, pDoc->nExamples, szHeading) <= 0)
      fWorked = FALSE;

    // mainpage examples
    if(fWorked && pDoc->pMainPage)
      fWorked = FlyDocHtmlWriteExampleList(pDoc, NULL, &pDoc->pMainPage->section, "Main Page");

    // modules
    pMod = pDoc->pModList;
    while(fWorked && pMod)
    {
      fWorked = FlyDocHtmlWriteExampleList(pDoc, pMod->section.szTitle, &pMod->section, "Module");
      pMod = pMod->pNext;
    }

    // classes
    pMod = pDoc->pClassList;
    while(fWorked && pMod)
    {
      fWorked = FlyDocHtmlWriteExampleList(pDoc, pMod->section.szTitle, &pMod->section, "Class");
      pMod = pMod->pNext;
    }

    // Markdown documents may have examples
    pDocument = pDoc->pMarkdownList;
    while(fWorked && pDocument)
    {
      FlyDocMakeNameBase(szNameBase, pDocument->section.szTitle, sizeof(szNameBase));
      fWorked = FlyDocHtmlWriteExampleList(pDoc, szNameBase, &pDocument->section, "Document");
      pDocument = pDocument->pNext;
    }
  }

  return fWorked;
}

/*!-------------------------------------------------------------------------------------------------
  Write each markdown document reference/title into the cell column.

  Helper to FlyDocHtmlWriteMainPage()

  @param    pDoc            Document context
  @param    pMarkdownList   List of markdown files (may be NULL)
  @return   none
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocHtmlWriteMainDocList(flyDoc_t *pDoc, flyDocMarkdown_t *pMarkdownList)
{
  flyDocMarkdown_t *pDocument;
  char              szRef[FLYDOC_REF_MAX];
  char              szNameBase[FLYDOC_REF_MAX];
  const char       *szHeading;
  unsigned          nDocs;
  bool_t            fWorked = TRUE;

  FlyAssert(pDoc && pDoc->fpOut);

  nDocs = FlyListLen(pMarkdownList);
  szHeading = (nDocs == 1) ? m_szHeadingDocumentSingular : m_szHeadingDocuments;

  // small heading for objects
  FlyAssert(FlyStrCount(m_szMainColObjects, "%u") == 1);
  FlyAssert(FlyStrCount(m_szMainColObjects, "%s") == 1);
  if(fprintf(pDoc->fpOut, m_szMainColObjects, nDocs, szHeading) <= 0)
    fWorked = FALSE;

  pDocument = pMarkdownList;
  while(pDocument)
  {
    // make reference to markdown file
    FlyDocMakeNameBase(szNameBase, pDocument->section.szTitle, sizeof(szNameBase));
    FlyDocStrToRef(szRef, sizeof(szRef), szNameBase, NULL);

    FlyAssert(FlyStrCount(m_szMainColRefLine, "%s") == 3);
    if(fprintf(pDoc->fpOut, m_szMainColRefLine, szRef, pDocument->section.szTitle,
               pDocument->section.szSubtitle ? pDocument->section.szSubtitle : "") <= 0)
    {
      fWorked = FALSE;
    }
    pDocument = pDocument->pNext;
  }

  return fWorked;
}

/*!------------------------------------------------------------------------------------------------
  Define up to 3 columns dpeending on content.

  Modules and Classes will be combined if there are also Examples and Documents, otherwise order is
  Modules, Classes, Examples. Documents.

  @param  pDoc        flydoc state with statistics up to date
  @param  pColTypes   array of 3 mainPageColType_t to be filled in
  @return 0-3, dependning on how many columns
-------------------------------------------------------------------------------------------------*/
unsigned FlyDocHtmlMainPageCols(flyDoc_t *pDoc, mainpageColType_t *aColTypes)
{
  unsigned  nCols = 0;

  // if all 4 lists, combined modules/classes into 1 column
  nCols = 0;
  if(pDoc->nModules && pDoc->nClasses && pDoc->nExamples && pDoc->nDocuments)
  {
    aColTypes[nCols].type       = MP_TYPE_MODULES_CLASSES;
    aColTypes[nCols].szHeading  = m_szHeadingModulesAndClasses;
    ++nCols;
  }

  // otherwise, each list gets its own column
  else
  {
    if(pDoc->nModules)
    {
      aColTypes[nCols].type       = MP_TYPE_MODULES;
      aColTypes[nCols].szHeading  = m_szHeadingModules;
      ++nCols;
    }
    if(pDoc->nClasses)
    {
      aColTypes[nCols].type       = MP_TYPE_CLASSES;
      aColTypes[nCols].szHeading  = m_szHeadingClasses;
      ++nCols;
    }
  }
  if(pDoc->nExamples)
  {
    aColTypes[nCols].type       = MP_TYPE_EXAMPLES;
    aColTypes[nCols].szHeading  = m_szHeadingExamples;
    ++nCols;
  }
  if(pDoc->nDocuments)
  {
    aColTypes[nCols].type       = MP_TYPE_DOCUMENTS;
    aColTypes[nCols].szHeading  = m_szHeadingDocuments;
    ++nCols;
  }

  return nCols;
}

/*!------------------------------------------------------------------------------------------------
  Write the pDoc mainpage to index.html

  **Layout for Mainpage**

      +------+---------------------+
      | Logo | Title Bar           |
      +--------+----------+--------+
      |Modules | Examples | Docs   |
      |Classes |          |        |
      |        |          |        |
      +-------+--------------------+

  @param  pDoc    document context
  @return TRUE if worked, FALSE if couldn't create folder, files or allocate memory
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocHtmlWriteMainPage(flyDoc_t *pDoc)
{
  flyDocMainPage_t   *pMainPage;
  flyDocStyle_t       style;
  unsigned            nPages;
  bool_t              fWorked = TRUE;
  mainpageColType_t   aColTypes[3];
  unsigned            nCols;
  unsigned            i;

  // don't make a maain page if only one HTML page: a single doc, module or class
  nPages = pDoc->nModules + pDoc->nClasses + pDoc->nDocuments;
  if((pDoc->pMainPage == NULL) && (nPages == 1))
    return fWorked;

  // generate pseudo mainpage if multiple modules/classes/docs but no actual mainpage
  if(pDoc->pMainPage == NULL)
  {
    pMainPage = FlyDocAlloc(sizeof(*pDoc->pMainPage));
    memset(pMainPage, 0, sizeof(*pDoc->pMainPage));
    pDoc->pMainPage = pMainPage;
  }

  // mainpage MUST have a title, so use "Table of Contents"
  if(fWorked)
  {
    pMainPage = pDoc->pMainPage;
    if(pMainPage->section.szTitle == NULL)
    {
      pMainPage->section.szTitle = FlyStrClone(m_szTableOfContents);
      FlyDocAllocCheck(pMainPage->section.szTitle);
    }
  }

  // create the HTML file
  if(fWorked)
  {
    pDoc->fpOut = FlyDocCreateHtmlFile(pDoc, "index");
    if(!pDoc->fpOut)
      fWorked = FALSE;
  }

  // determine style: @color, @font, @logo, @version
  // and write the opening title bar
  FlyDocStyleGet(pDoc, &pMainPage->section, &style);
  if(fWorked && !FlyDocHtmlWriteOpen(pDoc, &pMainPage->section, &style))
    fWorked = FALSE;

  // write any main page text before starting any columns
  if(pMainPage->section.szText)
  {
    if(fprintf(pDoc->fpOut, "%s", m_szMainTextOpen) <= 0)
      fWorked = FALSE;
    if(fWorked && !FlyDocHtmlWriteText(pDoc, pMainPage->section.szText, style.szHeadingColor))
      fWorked = FALSE;
    if(fWorked && fprintf(pDoc->fpOut, "%s", m_szMainTextClose) <= 0)
      fWorked = FALSE;
  }

  // Create columns (1 2 or 3 depending on content)
  nCols = FlyDocHtmlMainPageCols(pDoc, aColTypes);
  if(fWorked && nCols)
  {
    if(fprintf(pDoc->fpOut, m_szMainRowOpen) <= 0)
        fWorked = FALSE;

    for(i = 0; fWorked && i < nCols; ++i)
    {
      if(fprintf(pDoc->fpOut, m_szMainColOpen, aColTypes[i].szHeading) <= 0)
        fWorked = FALSE;

      switch(aColTypes[i].type)
      {
        case MP_TYPE_MODULES_CLASSES:
        if(!FlyDocHtmlWriteMainModList(pDoc, pDoc->pModList))
          fWorked = FALSE;
        if(fprintf(pDoc->fpOut, m_szMainColObjectsSep) <= 0)
          fWorked = FALSE;
        if(!FlyDocHtmlWriteMainModList(pDoc, pDoc->pClassList))
          fWorked = FALSE;
        break;
        case MP_TYPE_MODULES:
        if(!FlyDocHtmlWriteMainModList(pDoc, pDoc->pModList))
          fWorked = FALSE;
        break;
        case MP_TYPE_CLASSES:
        if(!FlyDocHtmlWriteMainModList(pDoc, pDoc->pClassList))
          fWorked = FALSE;
        break;
        case MP_TYPE_EXAMPLES:
        if(!FlyDocHtmlWriteMainExamplesAll(pDoc))
          fWorked = FALSE;
        break;
        case MP_TYPE_DOCUMENTS:
        if(!FlyDocHtmlWriteMainDocList(pDoc, pDoc->pMarkdownList))
          fWorked = FALSE;
        break;
      }

      if(fWorked && fprintf(pDoc->fpOut, m_szMainColClose) <= 0)
        fWorked = FALSE;
    }

    // done
    if(fWorked && fprintf(pDoc->fpOut, m_szMainRowClose) <= 0)
      fWorked = FALSE;
  }

  if(fWorked && fprintf(pDoc->fpOut, m_szMainEnd) <= 0)
    fWorked = FALSE;

  // done with file
  if(pDoc->fpOut)
  {
    fclose(pDoc->fpOut);
    pDoc->fpOut = NULL;
  }

  return fWorked;
}

/*!-------------------------------------------------------------------------------------------------
  Write the pDoc module to modname.html or classname.html.

  **Layout for Module or Class**

      +------+-----------------+
      | Logo | Title Bar       |
      +------+-----------------+
      |Links | Content         |
      |      |                 |
      |      |                 |
      +------+-----------------+

  @param  pDoc    document state
  @param  pMod    module to write
  @return TRUE if worked, FALSE if couldn't create folder or files
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocHtmlWriteModule(flyDoc_t *pDoc, flyDocModule_t *pMod)
{
  flyDocFunc_t     *pFunc;
  const char       *szLine;
  flyDocStyle_t    style;
  char              szRef[PATH_MAX];
  bool_t            fWorked   = TRUE;

  FlyAssert(pDoc && pMod && pMod->section.szTitle);

  // create the module HTML file, e.g. Person.html or Maths.html
  pDoc->fpOut = FlyDocCreateHtmlFile(pDoc, pMod->section.szTitle);
  if(!pDoc->fpOut)
    fWorked = FALSE;

  // determine style, @color, @font, @logo, @version
  // and write the opening title bar
  FlyDocStyleGet(pDoc, &pMod->section, &style);
  if(fWorked && !FlyDocHtmlWriteOpen(pDoc, &pMod->section, &style))
    fWorked = FALSE;

  // no left bar if not functions/methods
  if(fWorked && FlyListLen(pMod->pFuncList))
  {
    // create the left side bar: links to functions/methods
    if(fprintf(pDoc->fpOut, m_szModLeftOpen, style.szBarColor) <= 0)
      fWorked = FALSE;
    if(fWorked && fprintf(pDoc->fpOut, m_szModLeftSpacer) <= 0)
      fWorked = FALSE;

    // create links in left side bar to all functions/methods in module/class
    pFunc = pMod->pFuncList;
    while(fWorked && pFunc)
    {
      FlyDocStrToRef(szRef, sizeof(szRef), NULL, pFunc->szFunc);
      if(fprintf(pDoc->fpOut, m_szModLeftLine, szRef, pFunc->szFunc) <= 0)
        fWorked = FALSE;
      pFunc = pFunc->pNext;
    }
    if(fWorked && fprintf(pDoc->fpOut, m_szModLeftBarEnd) <= 0)
      fWorked = FALSE;
  }

  // create the right side: start with @class or @defgroup text
  if(fWorked)
  {
    if(fprintf(pDoc->fpOut, m_szModRightOpen) <= 0)
      fWorked = FALSE;
    if(fWorked && pMod->section.szSubtitle)
    {
      if(fprintf(pDoc->fpOut, m_szModRightTitle, pMod->section.szSubtitle) <= 0)
        fWorked = FALSE;
    }
    if(fWorked && pMod->section.szText && !FlyDocHtmlWriteText(pDoc, pMod->section.szText, style.szHeadingColor))
      fWorked = FALSE;
  }

  // create the right side function prototypes, notes and examples
  pFunc = pMod->pFuncList;
  while(fWorked && pFunc)
  {
    FlyDocStrToRef(szRef, sizeof(szRef), NULL, pFunc->szFunc);
    if(fprintf(pDoc->fpOut, m_szModRightFuncHead, &szRef[1], style.szHeadingColor, pFunc->szFunc,
              pFunc->szBrief ? pFunc->szBrief : "") <= 0)
    {
      fWorked = FALSE;
    }

    if(fWorked && pFunc->szPrototype)
    {
      if(fprintf(pDoc->fpOut, m_szModRightProtoOpen) <= 0)
        fWorked = FALSE;

      szLine = pFunc->szPrototype;
      while(fWorked && *szLine)
      {
        if(fprintf(pDoc->fpOut, m_szModRightProtoLine, (int)FlyStrLineLen(szLine), szLine) <= 0)
          fWorked = FALSE;
        szLine = FlyStrLineNext(szLine);
      }
      if(fWorked && fprintf(pDoc->fpOut, m_szModRightProtoClose) <= 0)
        fWorked = FALSE;
    }

    if(fWorked && pFunc->szText)
    {
      if(fprintf(pDoc->fpOut, m_szModRightNotesOpen) <= 0)
        fWorked = FALSE;
      else if(!FlyDocHtmlWriteText(pDoc, pFunc->szText, style.szHeadingColor))
        fWorked = FALSE;
    }

    pFunc = pFunc->pNext;
  }

  // end the module page
  if(fWorked && fprintf(pDoc->fpOut, m_szModEnd) <= 0)
    fWorked = FALSE;

  if(pDoc->fpOut)
    fclose(pDoc->fpOut);
  pDoc->fpOut = NULL;

  return fWorked;
}

/*-------------------------------------------------------------------------------------------------
  Allocate a new string of sz that has spaces converted to non-breaking spaces.

  Used to prevent browser from making too narrow of a column for links.

  @param  sz    a string
  @return Returns newly allocates
-------------------------------------------------------------------------------------------------*/
static char * FlyDocSpaceToNb(const char *sz)
{
  static const char szNbSp[] = "&nbsp;";
  char       *szNb;
  size_t      size;
  unsigned    n;

  n = FlyStrCount(sz, " ");
  size = strlen(sz) + (n * strlen(szNbSp)) + 1;
  szNb = FlyAlloc(size);
  if(szNb)
  {
    strcpy(szNb, sz);
    FlyStrReplace(szNb, size, " ", szNbSp, FLYSTR_REP_ALL);
  }
  return szNb;
}

/*!------------------------------------------------------------------------------------------------
  Write the pDoc->pMarkDown markdownName.html

  @param  pDoc        pointer to document state
  @param  pMarkdown   pointer to markdown item to write
  @return TRUE if worked, FALSE if couldn't create folder or files
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocHtmlWriteMarkdown(flyDoc_t *pDoc, flyDocMarkdown_t *pMarkdown)
{
  flyDocMdHdr_t    *pMdHdr;
  flyDocSection_t  *pSection    = &pMarkdown->section;
  flyDocStyle_t     style;
  char             *szNbTitle;
  char              szRef[PATH_MAX];
  bool_t            fWorked     = TRUE;

  // create the module HTML file
  pDoc->fpOut = FlyDocCreateHtmlFile(pDoc, pSection->szTitle);
  if(!pDoc->fpOut)
    return FALSE;

  // determine style, @color, @font, @logo, @version
  // and write the opening title bar
  FlyDocStyleGet(pDoc, pSection, &style);
  if(fWorked && !FlyDocHtmlWriteOpen(pDoc, pSection, &style))
    fWorked = FALSE;

  // no left bar if no headings
  if(FlyListLen(pMarkdown->pHdrList))
  {
    // create the left side bar: links to functions/methods
    if(fWorked && fprintf(pDoc->fpOut, m_szModLeftOpen, style.szBarColor) <= 0)
      fWorked = FALSE;
    if(fWorked && fprintf(pDoc->fpOut, m_szModLeftSpacer) <= 0)
      fWorked = FALSE;

    // write headers to left-handle column for easy links to sections in markdown file
    pMdHdr = pMarkdown->pHdrList;
    while(fWorked && pMdHdr)
    {
      FlyDocStrToRef(szRef, sizeof(szRef), NULL, pMdHdr->szTitle);
      szNbTitle = FlyDocSpaceToNb(pMdHdr->szTitle);
      FlyDocAllocCheck(szNbTitle);
      if(fprintf(pDoc->fpOut, m_szModLeftLine, szRef, szNbTitle) <= 0)
        fWorked = FALSE;
      FlyFree(szNbTitle);
      pMdHdr = pMdHdr->pNext;
    }
    if(fWorked && fprintf(pDoc->fpOut, m_szModLeftBarEnd) <= 0)
      fWorked = FALSE;
  }

  // create the right side: module text
  if(fWorked && fprintf(pDoc->fpOut, m_szModRightOpen) <= 0)
    fWorked = FALSE;

  // write out the entire markdown document
  if(fWorked && pSection->szText)
    fWorked = FlyDocHtmlWriteText(pDoc, pSection->szText, style.szHeadingColor);

  // end the markdown page
  if(fWorked && fprintf(pDoc->fpOut, m_szModEnd) <= 0)
    fWorked = FALSE;

  fclose(pDoc->fpOut);
  pDoc->fpOut = NULL;

  return TRUE;
}

/*!------------------------------------------------------------------------------------------------
  Write local w3.css file into folder pointed to by pDoc->opts.szOut

  @param  pDoc        pointer to document state
  @return TRUE if worked, FALSE if couldn't create folder or files
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocHtmlWriteCss(flyDoc_t *pDoc)
{
  // get filename to use into pDoc->szPath
  FlyStrZCpy(pDoc->szPath, pDoc->opts.szOut, sizeof(pDoc->szPath));
  FlyStrPathAppend(pDoc->szPath, "w3.css", sizeof(pDoc->szPath));
  if(pDoc->opts.verbose >= FLYDOC_VERBOSE_MORE)
    printf("  %s\n", pDoc->szPath);
  return FlyFileWrite(pDoc->szPath, szW3CssFile);
}

/*!------------------------------------------------------------------------------------------------
  Write flydoc_home.png file into folder pointed to by pDoc->opts.szOut

  @param  pDoc        pointer to document state
  @return TRUE if worked, FALSE if couldn't create folder or files
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocHtmlWriteImgHome(flyDoc_t *pDoc)
{
  // get filename to use into pDoc->szPath
  FlyStrZCpy(pDoc->szPath, pDoc->opts.szOut, sizeof(pDoc->szPath));
  FlyStrPathAppend(pDoc->szPath, "flydoc_home.png", sizeof(pDoc->szPath));
  if(pDoc->opts.verbose >= FLYDOC_VERBOSE_MORE)
    printf("  %s\n", pDoc->szPath);
  return FlyFileWriteBin(pDoc->szPath, imgHome, imgHome_size);
}

/*!------------------------------------------------------------------------------------------------
  Write the pDoc data to .html file(s)

  @param  pDoc   filled-in document with (optional) mainpage and modules.
  @return TRUE if worked, FALSE if couldn't create folder or files
-------------------------------------------------------------------------------------------------*/
bool_t FlyDocWriteHtml(flyDoc_t *pDoc)
{
  flyDocModule_t   *pMod;
  flyDocMarkdown_t *pMarkdown;
  bool_t            fWorked = TRUE;

  if(pDoc->opts.debug)
    printf("-- FlyDocWriteHtml(%s) ---\n", pDoc->opts.szOut);

  // create the folder
  if(!FlyDocCreateFolder(pDoc, pDoc->opts.szOut))
  {
    FlyDocPrintWarning(pDoc, szWarningCreateFolder, pDoc->opts.szOut);
    fWorked = FALSE;
  }

  // write the w3.css file if user wants a local reference to that file
  if(fWorked && pDoc->opts.fLocal && !FlyDocHtmlWriteCss(pDoc))
  {
    FlyDocPrintWarning(pDoc, szWarningCreateFile, pDoc->szPath);
    fWorked = FALSE;
  }

  if(fWorked && !FlyDocHtmlWriteMainPage(pDoc))
  {
    FlyDocPrintWarning(pDoc, szWarningCreateFile, pDoc->szPath);
    fWorked = FALSE;
  }

  // write out modules first
  pMod = pDoc->pModList;
  while(fWorked && pMod)
  {
    if(!FlyDocHtmlWriteModule(pDoc, pMod))
    {
      FlyDocPrintWarning(pDoc, szWarningCreateFile, pDoc->szPath);
      fWorked = FALSE;
      break;
    }
    pMod = pMod->pNext;
  }

  // write out class list next
  pMod = pDoc->pClassList;
  while(fWorked && pMod)
  {
    if(!FlyDocHtmlWriteModule(pDoc, pMod))
    {
      FlyDocPrintWarning(pDoc, szWarningCreateFile, pDoc->szPath);
      fWorked = FALSE;
      break;
    }
    pMod = pMod->pNext;
  }

  pMarkdown = pDoc->pMarkdownList;
  while(fWorked && pMarkdown)
  {
    if(!FlyDocHtmlWriteMarkdown(pDoc, pMarkdown))
    {
      FlyDocPrintWarning(pDoc, szWarningCreateFile, pDoc->szPath);
      fWorked = FALSE;
      break;
    }
    pMarkdown = pMarkdown->pNext;
  }

  // write the w3.css file if user wants a local reference to that file
  if(fWorked && pDoc->fNeedImgHome && !FlyDocHtmlWriteImgHome(pDoc))
  {
    FlyDocPrintWarning(pDoc, szWarningCreateFile, pDoc->szPath);
    fWorked = FALSE;
  }

  return fWorked;
}
