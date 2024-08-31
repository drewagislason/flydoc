/**************************************************************************************************
  flydoc.h - Create documentation from source code
  Copyright 2024 Drew Gislason
  License MIT <https://mit-license.org>
**************************************************************************************************/
#include "Fly.h"
#include "FlyStr.h"

#ifndef FLY_DOC_H
#define FLY_DOC_H 1

// allows source to be compiled with gcc or g++ compilers
#ifdef __cplusplus
  extern "C" {
#endif

#define FLYDOC_VER_STR "1.0"

#define FLYDOC_REF_MAX          256  // used for URLs, symbols, links, function names
#define SZ_FLY_DOC_EXTS         ".c.c++.cc.cpp.cxx.cs.go.java.js.py.rs.swift.ts"
#define FLYDOC_DEF_BAR_COLOR    "w3-blue"
#define FLYDOC_DEF_TITLE_COLOR  "w3-black"
#define FLYDOC_DEF_HBAR_COLOR   "w3-text-blue"
#define FLYDOC_DEF_HTITLE_COLOR "w3-text-black"
#define FLYDOC_MAX_DEPTH        3

typedef enum 
{
  // @keywords: IMPORTANT! if changing, you MUST also update FlyDocIsKeyword()
  FLYDOC_KEYWORD_CLASS = 0,
  FLYDOC_KEYWORD_COLOR,
  FLYDOC_KEYWORD_DEFGROUP,
  FLYDOC_KEYWORD_EXAMPLE,
  FLYDOC_KEYWORD_FN,
  FLYDOC_KEYWORD_FONT,
  FLYDOC_KEYWORD_INCLASS,
  FLYDOC_KEYWORD_INGROUP,
  FLYDOC_KEYWORD_LOGO,
  FLYDOC_KEYWORD_MAINPAGE,
  FLYDOC_KEYWORD_PARAM,
  FLYDOC_KEYWORD_RETURN,
  FLYDOC_KEYWORD_RETURNS,
  FLYDOC_KEYWORD_VERSION,
  FLYDOC_KEYWORD_UNKNOWN    // pseudo keyword to future proof @keywords
} flyDocKeyword_t;

typedef enum
{
  FLYDOC_VERBOSE_NONE = 0,    // only print warnings
  FLYDOC_VERBOSE_SOME,        // normal, prints warnings, stats (default)
  FLYDOC_VERBOSE_MORE,        // prints files being processed, created and copied
} flyDocVerbose_t;

typedef enum
{
  FLYDOC_DEBUG_NONE = 0,    // no debug printing (default)
  FLYDOC_DEBUG_SOME,        // prints entry into main functions, prints doc after parsing
  FLYDOC_DEBUG_MORE,        // prints multiline objects after parsing
  FLYDOC_DEBUG_MAX,         // printf full contents of markdown text
} flyDocDbg_t;

typedef enum
{
  FLYDOC_SORT_NONE = 0,
  FLYDOC_SORT_CODE,
  FLYDOC_SORT_ALL,
} flyDocSort_t;

typedef struct
{
  const char *szExts;
  const char *szLink;
  const char *szOut;
  const char *szSlug;
  int         debug;
  int         verbose;
  bool_t      fNoBuild;
  bool_t      fSort;
  bool_t      fLocal;
  bool_t      fMarkdown;
  bool_t      fCombine;   // applies to --markdown only
  bool_t      fNoIndex;
  bool_t      fUserGuide;
} flyDocOpts_t;

// a function
typedef struct flyDocFunc
{
  struct flyDocFunc *pNext;
  char              *szFunc;        // function CName
  char              *szBrief;
  char              *szPrototype;   // includes @param and @return lines
  char              *szText;        // may be NULL if no text beyond brief description
  const char        *szLang;
} flyDocFunc_t;

// an example
typedef struct flyDocExample
{
  struct flyDocExample  *pNext;
  char                  *szTitle;
} flyDocExample_t;

// an image converted from markdown into alt, link, title
typedef struct flyDocImage
{
  struct flyDocImage  *pNext;
  char                *szLink;      // .e.g "https//pics.com/image.png" or "lake.jpg"
} flyDocImage_t;

// used for both markdown an images files as they are pre and post-processed
typedef struct flyDocFile
{
  struct flyDocFile   *pNext;
  char                *szPath;       // path to image file src (where to copy from)
  bool_t               fReferenced;  // some image link in the markdown referenced this
} flyDocFile_t;

// main page, module, class, or markdown file
typedef struct
{
  char             *szTitle;        // title of the project
  char             *szSubtitle;     // usually one line description, or NULL (not there)
  char             *szText;         // main text or may be empty
  char             *szBarColor;     // @color defaults to "w3-blue"
  char             *szTitleColor;   // @color defaults to "w3-black"
  char             *szHeadingColor; // e.g. w3-text-black or w3-text-red (uses color of szTitleColor)
  char             *szFontBody;     // @font NULL for default body font
  char             *szFontHeadings; // @font NULL for default heading font, h1,h2 etc..
  char             *szLogo;         // @logo defaults to a transparent icon
  char             *szVersion;      // @version may be NULL if no version
  flyDocExample_t  *pExampleList;   // examples in the text of the module/main page
} flyDocSection_t;

// see also flyDocSection_t
typedef struct
{
  char             *szBarColor;     // @color defaults to "w3-blue"
  char             *szTitleColor;   // @color defaults to "w3-black"
  char             *szHeadingColor; // e.g. w3-text-black or w3-text-red (uses color of szTitleColor)
  char             *szFontBody;     // @font NULL for default body font
  char             *szFontHeadings; // @font NULL for default heading font, h1,h2 etc..
  char             *szLogo;         // @logo defaults to a transparent icon
  char             *szVersion;      // @version may be NULL if no version  
} flyDocStyle_t;

// All 2-6 level headers, e.g. ## Title. Level 1 header is szTitle in the section
typedef struct flyDocMdHdr
{
  struct flyDocMdHdr      *pNext;
  const char              *szTitle; // NOT a string, just a pointer to the header line in markdwon file
} flyDocMdHdr_t;

// each markdown file gets it's own page
typedef struct flyDocMarkdown
{
  struct flyDocMarkdown  *pNext;
  flyDocSection_t         section;       // includes logo, colors, etc...
  char                   *szPath;        // path to markdown file
  const char             *szFile;
  flyDocMdHdr_t          *pHdrList;      // ptrs to headers in markdown file
} flyDocMarkdown_t;

// a class or module, see also FlyDocModPrint()
typedef struct flyDocModule
{
  struct flyDocModule    *pNext;         
  flyDocSection_t         section;        // includes logo, colors, etc...
  flyDocFunc_t           *pFuncList;      // list of functions/methods in module
} flyDocModule_t;

// only one per project
typedef struct
{
  flyDocSection_t         section;        // includes logo, colors, etc...
} flyDocMainPage_t;

// main state for a flydoc session
typedef struct
{
  unsigned          sanchk;
  unsigned          level;
  flyDocOpts_t      opts;                 // cmdline options
  char              szInPath[PATH_MAX];   // for recursing through input files/folders

  // current file being parsed or written
  char              szPath[PATH_MAX];     // current filename being processed
  const char       *szFile;               // current file contents being processed
  flyDocModule_t   *pCurMod;              // current module or class (for functions)
  flyStrHdr_t      *pCurHdr;              // current doc header or NULL
  const char       *szCurHdr;             // pointer to allocated text of doc header or NULL
  FILE             *fpOut;                // current file being written

  // parsed input ready for output
  flyDocMainPage_t *pMainPage;            // main page for entire project
  flyDocModule_t   *pModList;             // modules in project, or NULL if none
  flyDocModule_t   *pClassList;           // classes in project, or NULL if none
  flyDocMarkdown_t *pMarkdownList;        // markdown files (documents) in project , NULL if none
  flyDocImage_t    *pImageList;           // image references found in all text
  flyDocFile_t     *pImgFileList;         // list of input image files, some of which may be referenced
  bool_t            fNeedImgHome;         // need the flydoc_home.png image
  
  // statistics, see FlyDocStatsUpdate()
  unsigned          nModules;
  unsigned          nFunctions;
  unsigned          nClasses;
  unsigned          nMethods;
  unsigned          nExamples;            // total # of examples, all sections
  unsigned          nDocuments;
  unsigned          nImages;              // # of image references in markdown and @logo
  unsigned          nFiles;
  unsigned          nDocComments;
  unsigned          nWarnings;
} flyDoc_t;

#ifdef __cplusplus
  }
#endif

// flydoc.c
bool_t    FlyDocIsDoc               (const flyDoc_t *pDoc);
void      FlyDocStyleGet            (flyDoc_t *pDoc, flyDocSection_t *pSection, flyDocStyle_t *pStyle);
void      FlyDocAllocCheck          (void *pMem);
void     *FlyDocAlloc               (size_t n);
bool_t    FlyDocCreateFolder        (flyDoc_t *pDoc, const char *szPath);

// flydochome.c
extern uint8_t imgHome[];
extern long imgHome_size;

// flydochtml.c
bool_t    FlyDocWriteHtml           (flyDoc_t *pDoc);
size_t    FlyDocStrToRef            (char *szRef, unsigned size, const char *szBase, const char *szTitle);

// flydocmd.c
bool_t    FlyDocWriteMarkdown       (flyDoc_t *pDoc);

// flydoccss.c
extern const char szW3CssPath[];
extern const char szW3CssFile[];

// flydocmanual.c
extern const char szFlyDocManual[];

// flydocparse.c
unsigned          FlyDocExampleCountAll     (flyDoc_t *pDoc);
flyDocExample_t  *FlyDocExampleNew          (const char *szTitle);
flyDocExample_t  *FlyDocExampleFree         (flyDocExample_t *pExample);
const char       *FlyDocIsKeyword           (const char *szLine, flyDocKeyword_t *pKeyword);
bool_t            FlyDocIsKeywordProto      (flyDocKeyword_t keyword);
void              FlyDocProcessFolderTree   (flyDoc_t *pDoc, const char *szPath);
void              FlyDocPreProcess          (flyDoc_t *pDoc, const char *szPath);
void              FlyDocStatsUpdate         (flyDoc_t *pDoc);
unsigned          FlyDocMakeNameBase        (char *szNameBase, const char *szTitle, size_t size);
void              FlyDocDupCheck            (flyDoc_t *pDoc, const char *szTitle, const char *szPos);

// flydocprint.c
void      FlyDocPrintBanner         (const char *szText);
void      FlyDocPrintDoc            (const flyDoc_t *pDoc, flyDocDbg_t debug);
void      FlyDocPrintMainPage       (const flyDocMainPage_t *pMainPage, flyDocDbg_t debug);
void      FlyDocPrintModule         (const flyDocModule_t *pMod, flyDocDbg_t debug);
void      FlyDocPrintStats          (const flyDoc_t *pDoc);
void      FlyDocPrintFunc           (const flyDocFunc_t *pFunc, flyDocDbg_t debug, unsigned indent);
bool_t    FlyDocCheckOverWrite      (const char *szFilename);
void      FlyDocPrintSlug           (const char *szTitle);

void      FlyDocPrintWarning        (flyDoc_t *pDoc, const char *szWarning, const char *szExtra);
void      FlyDocPrintWarningEx      (flyDoc_t *pDoc, const char *szWarning, const char *szExtra, const char *szFile, const char *szFilePos);
void      FlyDocAssertMem           (void);

// warnings
extern const char szWarningNoModule[];
extern const char szWarningDuplicate[];
extern const char szWarningNoFunction[];
extern const char szWarningBadDocStr[];
extern const char szWarningSyntax[];
extern const char szWarningEmpty[];
extern const char szWarningInternal[];
extern const char szWarningInvalidInput[];
extern const char szWarningCreateFolder[];
extern const char szWarningCreateFile[];
extern const char szWarningNoObjects[];
extern const char szWarningNoImage[];
extern const char szWarningReadFile[];

#endif // FLY_DOC_H
